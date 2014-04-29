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
#include "xgb.h"

extern struct io default_xops;

/***************************************************************************
 *  request_socks events trigger.
 ***************************************************************************/

static void request_socks_full(int xd) {
    struct xsock *sx = xget(xd);    
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if ((sx->io.et.events & EPOLLIN)) {
	sx->io.et.events &= ~EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &sx->io.et) != 0);
    }
}

static void request_socks_nonfull(int xd) {
    struct xsock *sx = xget(xd);    
    struct xcpu *cpu = xcpuget(sx->cpu_no);

    // Enable POLLOUT event when snd_head isn't empty
    if (!(sx->io.et.events & EPOLLIN)) {
	sx->io.et.events |= EPOLLIN;
	BUG_ON(eloop_mod(&cpu->el, &sx->io.et) != 0);
    }
}

static int xio_listener_handler(eloop_t *el, ev_t *et);

static int xio_listener_bind(int xd, const char *sock) {
    int s, on = 1;
    struct xsock *sx = xget(xd);
    struct xcpu *cpu = xcpuget(sx->cpu_no);
    struct transport *tp = transport_lookup(sx->pf);

    BUG_ON(!tp);
    if ((s = tp->bind(sock)) < 0)
	return -1;

    ZERO(sx->io);
    strncpy(sx->addr, sock, TP_SOCKADDRLEN);

    tp->setopt(s, TP_NOBLOCK, &on, sizeof(on));
    sx->io.fd = s;
    sx->io.tp = tp;
    sx->io.et.events = EPOLLIN|EPOLLERR;
    sx->io.et.fd = s;
    sx->io.et.f = xio_listener_handler;
    sx->io.et.data = sx;

    BUG_ON(eloop_add(&cpu->el, &sx->io.et) != 0);
    return 0;
}

static void xio_listener_close(int xd) {
    struct xsock *sx = xget(xd);
    struct xcpu *cpu = xcpuget(sx->cpu_no);
    struct transport *tp = sx->io.tp;

    BUG_ON(!tp);

    /* Detach xsock low-level file descriptor from poller */
    BUG_ON(eloop_del(&cpu->el, &sx->io.et) != 0);
    tp->close(sx->io.fd);

    sx->io.fd = -1;
    sx->io.et.events = -1;
    sx->io.et.fd = -1;
    sx->io.et.f = 0;
    sx->io.et.data = 0;
    sx->io.tp = 0;

    /* Destroy the xsock base and free xsockid. */
    xsock_free(sx);
}

static void request_socks_notify(int xd, u32 events) {
    if (events & XMQ_FULL)
	request_socks_full(xd);
    else if (events & XMQ_NONFULL)
	request_socks_nonfull(xd);
}

static void xio_listener_notify(int xd, int type, uint32_t events) {
    switch (type) {
    case SOCKS_REQ:
	request_socks_notify(xd, events);
	break;
    default:
	BUG_ON(1);
    }
}

extern int xio_connector_handler(eloop_t *el, ev_t *et);
extern void xio_connector_init(struct xsock *sx,
			       struct transport *tp, int s);

static int xio_listener_handler(eloop_t *el, ev_t *et) {
    int s;
    struct xsock *sx = cont_of(et, struct xsock, io.et);
    struct transport *tp = sx->io.tp;
    struct xsock *req_sx;

    if ((et->happened & EPOLLERR) || !(et->happened & EPOLLIN)) {
	sx->fok = false;
	errno = EPIPE;
	return -1;
    } 
    if ((s = tp->accept(sx->io.fd)) < 0)
	return -1;
    if (!(req_sx = xsock_alloc())) {
	errno = EMFILE;
	return -1;
    }
    DEBUG_OFF("xsock accept new connection %d", s);
    req_sx->type = XCONNECTOR;
    req_sx->pf = sx->pf;
    req_sx->l4proto = l4proto_lookup(req_sx->pf, req_sx->type);
    xio_connector_init(req_sx, tp, s);
    reqsocks_push(sx, req_sx);
    return 0;
}

struct xsock_protocol xtcp_listener_protocol = {
    .type = XLISTENER,
    .pf = XPF_TCP,
    .bind = xio_listener_bind,
    .close = xio_listener_close,
    .notify = xio_listener_notify,
    .getsockopt = 0,
    .setsockopt = 0,
};

struct xsock_protocol xipc_listener_protocol = {
    .type = XLISTENER,
    .pf = XPF_IPC,
    .bind = xio_listener_bind,
    .close = xio_listener_close,
    .notify = xio_listener_notify,
    .getsockopt = 0,
    .setsockopt = 0,
};
