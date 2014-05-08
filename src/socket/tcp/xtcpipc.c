/*
  Copyright (c) 2013-2014 Dong Fang. All rights reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <runner/taskpool.h>
#include "../xgb.h"

static i64 xio_connector_read(struct io *ops, char *buff, i64 sz) {
    struct tcpipc_sock *self = cont_of(ops, struct tcpipc_sock, ops);
    struct transport *tp = self->tp;

    BUG_ON(!tp);
    int rc = tp->read(self->sys_fd, buff, sz);
    return rc;
}

static i64 xio_connector_write(struct io *ops, char *buff, i64 sz) {
    struct tcpipc_sock *self = cont_of(ops, struct tcpipc_sock, ops);
    struct transport *tp = self->tp;

    BUG_ON(!tp);
    int rc = tp->write(self->sys_fd, buff, sz);
    return rc;
}

struct io default_xops = {
    .read = xio_connector_read,
    .write = xio_connector_write,
};



/******************************************************************************
 *  snd_head events trigger.
 ******************************************************************************/

static void snd_head_empty(struct sockbase *sb) {
    struct tcpipc_sock *self = cont_of(sb, struct tcpipc_sock, base);
    struct xcpu *cpu = xcpuget(sb->cpu_no);

    // Disable POLLOUT event when snd_head is empty
    if (self->et.events & EPOLLOUT) {
	DEBUG_OFF("%d disable EPOLLOUT", sb->fd);
	self->et.events &= ~EPOLLOUT;
	BUG_ON(eloop_mod(&cpu->el, &self->et) != 0);
    }
}

static void snd_head_nonempty(struct sockbase *sb) {
    struct tcpipc_sock *self = cont_of(sb, struct tcpipc_sock, base);
    struct xcpu *cpu = xcpuget(sb->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(self->et.events & EPOLLOUT)) {
	DEBUG_OFF("%d enable EPOLLOUT", sb->fd);
	self->et.events |= EPOLLOUT;
	BUG_ON(eloop_mod(&cpu->el, &self->et) != 0);
    }
}


/******************************************************************************
 *  rcv_head events trigger.
 ******************************************************************************/

static void rcv_head_pop(struct sockbase *sb) {
    if (sb->snd.waiters)
	condition_signal(&sb->cond);
}

static void rcv_head_full(struct sockbase *sb) {
    struct tcpipc_sock *self = cont_of(sb, struct tcpipc_sock, base);
    struct xcpu *cpu = xcpuget(sb->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if ((self->et.events & EPOLLIN)) {
	DEBUG_OFF("%d disable EPOLLIN", sb->fd);
	self->et.events &= ~EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &self->et) != 0);
    }
}

static void rcv_head_nonfull(struct sockbase *sb) {
    struct tcpipc_sock *self = cont_of(sb, struct tcpipc_sock, base);
    struct xcpu *cpu = xcpuget(sb->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(self->et.events & EPOLLIN)) {
	DEBUG_OFF("%d enable EPOLLIN", sb->fd);
	self->et.events |= EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &self->et) != 0);
    }
}

int xio_connector_handler(eloop_t *el, ev_t *et);

struct sockbase *xio_alloc() {
    struct tcpipc_sock *self =
	(struct tcpipc_sock *)mem_zalloc(sizeof(*self));

    if (self) {
	xsock_init(&self->base);
	bio_init(&self->in);
	bio_init(&self->out);
	return &self->base;
    }
    return 0;
}

static int xio_connector_bind(struct sockbase *sb, const char *sock) {
    struct tcpipc_sock *self = cont_of(sb, struct tcpipc_sock, base);
    int sys_fd;
    int on = 1;
    struct xcpu *cpu = xcpuget(sb->cpu_no);

    BUG_ON(!cpu);
    BUG_ON(!(self->tp = transport_lookup(sb->vfptr->pf)));
    if ((sys_fd = self->tp->connect(sock)) < 0)
	return -1;

    BUG_ON(self->tp->setopt(sys_fd, TP_NOBLOCK, &on, sizeof(on)));
    strncpy(sb->peer, sock, TP_SOCKADDRLEN);
    self->sys_fd = sys_fd;
    self->ops = default_xops;
    self->et.events = EPOLLIN|EPOLLRDHUP|EPOLLERR;
    self->et.fd = sys_fd;
    self->et.f = xio_connector_handler;
    self->et.data = self;
    BUG_ON(eloop_add(&cpu->el, &self->et) != 0);
    return 0;
}

static int xio_connector_snd(struct sockbase *sb);

static void xio_connector_close(struct sockbase *sb) {
    struct tcpipc_sock *self = cont_of(sb, struct tcpipc_sock, base);
    struct xcpu *cpu = xcpuget(sb->cpu_no);
    BUG_ON(!self->tp);

    /* Try flush buf massage into network before close */
    xio_connector_snd(sb);

    /* Detach xsock low-level file descriptor from poller */
    BUG_ON(eloop_del(&cpu->el, &self->et) != 0);
    self->tp->close(self->sys_fd);

    self->sys_fd = -1;
    self->et.events = -1;
    self->et.fd = -1;
    self->et.f = 0;
    self->et.data = 0;
    self->tp = 0;

    /* Destroy the xsock base and free xsockid. */
    xsock_exit(sb);
    mem_free(self, sizeof(*self));
}


static void rcv_head_notify(struct sockbase *sb, u32 events) {
    if (events & XMQ_POP)
	rcv_head_pop(sb);
    if (events & XMQ_FULL)
	rcv_head_full(sb);
    else if (events & XMQ_NONFULL)
	rcv_head_nonfull(sb);
}

static void snd_head_notify(struct sockbase *sb, u32 events) {
    if (events & XMQ_EMPTY)
	snd_head_empty(sb);
    else if (events & XMQ_NONEMPTY)
	snd_head_nonempty(sb);
}

static void xio_connector_notify(struct sockbase *sb, int type, u32 events) {
    switch (type) {
    case RECV_Q:
	rcv_head_notify(sb, events);
	break;
    case SEND_Q:
	snd_head_notify(sb, events);
	break;
    default:
	BUG_ON(1);
    }
}

static int bufio_check_msg(struct bio *b) {
    struct xmsg aim = {};
    
    if (b->bsize < sizeof(aim.vec))
	return false;
    bio_copy(b, (char *)(&aim.vec), sizeof(aim.vec));
    if (b->bsize < xiov_len(aim.vec.chunk) + (u32)aim.vec.oob_length)
	return false;
    return true;
}


static void bufio_rm(struct bio *b, struct xmsg **msg) {
    struct xmsg one = {};
    char *chunk;

    bio_copy(b, (char *)(&one.vec), sizeof(one.vec));
    chunk = xallocmsg(one.vec.size);
    bio_read(b, xiov_base(chunk), xiov_len(chunk));
    *msg = cont_of(chunk, struct xmsg, vec.chunk);
}

static int xio_connector_rcv(struct sockbase *sb) {
    struct tcpipc_sock *self = cont_of(sb, struct tcpipc_sock, base);
    int rc = 0;
    u16 oob_count;
    struct xmsg *aim = 0, *oob = 0;

    rc = bio_prefetch(&self->in, &self->ops);
    if (rc < 0 && errno != EAGAIN)
	return rc;
    while (bufio_check_msg(&self->in)) {
	aim = 0;
	bufio_rm(&self->in, &aim);
	BUG_ON(!aim);
	oob_count = aim->vec.oob;
	while (oob_count--) {
	    oob = 0;
	    bufio_rm(&self->in, &oob);
	    BUG_ON(!oob);
	    list_add_tail(&oob->item, &aim->oob);
	}
	recvq_push(sb, aim);
	DEBUG_OFF("%d xsock recv one message", sb->fd);
    }
    return rc;
}

static void bufio_add(struct bio *b, struct xmsg *msg) {
    struct xmsg *oob, *nx_oob;
    char *chunk = msg->vec.chunk;

    bio_write(b, xiov_base(chunk), xiov_len(chunk));
    xmsg_walk_safe(oob, nx_oob, &msg->oob) {
	chunk = oob->vec.chunk;
	bio_write(b, xiov_base(chunk), xiov_len(chunk));
    }
}

static int xio_connector_snd(struct sockbase *sb) {
    struct tcpipc_sock *self = cont_of(sb, struct tcpipc_sock, base);
    int rc;
    struct xmsg *msg;

    while ((msg = sendq_pop(sb))) {
	bufio_add(&self->out, msg);
	xfreemsg(msg->vec.chunk);
    }
    rc = bio_flush(&self->out, &self->ops);
    return rc;
}

int xio_connector_handler(eloop_t *el, ev_t *et) {
    int rc = 0;
    struct tcpipc_sock *self = cont_of(et, struct tcpipc_sock, et);
    struct sockbase *sb = &self->base;

    if (et->happened & EPOLLIN) {
	DEBUG_OFF("io xsock %d EPOLLIN", sb->fd);
	rc = xio_connector_rcv(sb);
    }
    if (et->happened & EPOLLOUT) {
	DEBUG_OFF("io xsock %d EPOLLOUT", sb->fd);
	rc = xio_connector_snd(sb);
    }
    if ((rc < 0 && errno != EAGAIN) || et->happened & (EPOLLERR|EPOLLRDHUP)) {
	DEBUG_OFF("io xsock %d EPIPE", sb->fd);
	sb->fok = false;
    }
    xeventnotify(sb);
    return rc;
}



struct sockbase_vfptr xtcp_connector_spec = {
    .type = XCONNECTOR,
    .pf = XPF_TCP,
    .alloc = xio_alloc,
    .bind = xio_connector_bind,
    .close = xio_connector_close,
    .notify = xio_connector_notify,
    .setopt = 0,
    .getopt = 0,
};

struct sockbase_vfptr xipc_connector_spec = {
    .type = XCONNECTOR,
    .pf = XPF_IPC,
    .alloc = xio_alloc,
    .bind = xio_connector_bind,
    .close = xio_connector_close,
    .notify = xio_connector_notify,
    .setopt = 0,
    .getopt = 0,
};