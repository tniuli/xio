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

#include "sp_module.h"

struct sp_global sg;

/* Default snd/rcv buffer size = 1G */
static int SP_SNDWND = 1048576000;
static int SP_RCVWND = 1048576000;

static void epsk_bad_status(struct epsk *sk) {
    struct epbase *ep = sk->owner;
    mutex_lock(&ep->lock);
    list_move_tail(&sk->item, &ep->bad_socks);
    mutex_unlock(&ep->lock);
}

void sg_add_sk(struct epsk *sk) {
    int rc;
    mutex_lock(&sg.lock);
    rc = xpoll_ctl(sg.po, XPOLL_ADD, &sk->ent);
    mutex_unlock(&sg.lock);
    BUG_ON(rc);
}

void sg_rm_sk(struct epsk *sk) {
    int rc;
    mutex_lock(&sg.lock);
    rc = xpoll_ctl(sg.po, XPOLL_DEL, &sk->ent);
    mutex_unlock(&sg.lock);
    BUG_ON(rc);
}

void __sg_update_sk(struct epsk *sk, u32 ev) {
    int rc;
    sk->ent.care = ev;
    rc = xpoll_ctl(sg.po, XPOLL_MOD, &sk->ent);
    BUG_ON(rc);
}

void sg_update_sk(struct epsk *sk, u32 ev) {
    mutex_lock(&sg.lock);
    __sg_update_sk(sk, ev);
    mutex_unlock(&sg.lock);
}

static void connector_event_hndl(struct epsk *sk) {
    struct epbase *ep = sk->owner;
    int rc;
    char *ubuf = 0;
    int happened = sk->ent.happened;

    if (happened & XPOLLIN) {
	DEBUG_OFF("ep %d socket %d recv begin", ep->eid, sk->fd);
	if ((rc = xrecv(sk->fd, &ubuf)) == 0) {
	    DEBUG_OFF("ep %d socket %d recv ok", ep->eid, sk->fd);
	    if ((rc = ep->vfptr.add(ep, sk, ubuf)) < 0) {
		xfreemsg(ubuf);
		DEBUG_ON("ep %d drop message from socket %d of can't back",
			  ep->eid, sk->fd);
	    }
	} else if (errno != EAGAIN)
	    happened |= XPOLLERR;
    }
    if (happened & XPOLLOUT) {
	if ((rc = ep->vfptr.rm(ep, sk, &ubuf)) == 0) {
	    DEBUG_OFF("ep %d socket %d send begin", ep->eid, sk->fd);
	    if ((rc = xsend(sk->fd, ubuf)) < 0) {
		xfreemsg(ubuf);
		if (errno != EAGAIN)
		    happened |= XPOLLERR;
		DEBUG_OFF("ep %d socket %d send with errno %d", ep->eid, sk->fd,
			  errno);
	    } else
		DEBUG_OFF("ep %d socket %d send ok", ep->eid, sk->fd);
	}
    }
    if (happened & XPOLLERR) {
	DEBUG_OFF("ep %d socket %d epipe", ep->eid, sk->fd);
	sg_rm_sk(sk);
	epsk_bad_status(sk);
    }
}

static void listener_event_hndl(struct epsk *sk) {
    int rc, nfd, happened;
    int on, optlen = sizeof(on);
    struct epbase *ep = sk->owner;

    happened = sk->ent.happened;
    if (happened & XPOLLIN) {
	while ((nfd = xaccept(sk->fd)) >= 0) {
	    rc = xsetopt(nfd, XL_SOCKET, XNOBLOCK, &on, optlen);
	    BUG_ON(rc);
	    DEBUG_OFF("%d join fd %d begin", ep->eid, nfd);
	    if ((rc = ep->vfptr.join(ep, sk, nfd)) < 0) {
		xclose(nfd);
		DEBUG_OFF("%d join fd %d with errno %d", ep->eid, nfd, errno);
	    }
	}
	if (errno != EAGAIN)
	    happened |= XPOLLERR;
    }
    if (happened & XPOLLERR) {
	DEBUG_OFF("socket %d epipe", sk->fd);
	sg_rm_sk(sk);
	epsk_bad_status(sk);
    }
}

static void event_hndl(struct xpoll_event *ent) {
    int socktype = 0;
    int optlen;
    struct epsk *sk = (struct epsk *)ent->self;

    BUG_ON(sk->fd != sk->ent.xd);
    sk->ent.happened = ent->happened;
    xgetopt(sk->fd, XL_SOCKET, XSOCKTYPE, &socktype, &optlen);

    switch (socktype) {
    case XCONNECTOR:
	connector_event_hndl(sk);
	break;
    case XLISTENER:
	listener_event_hndl(sk);
	break;
    default:
	BUG_ON(1);
    }
    DEBUG_OFF(sleep(1));
}

static void shutdown_epbase() {
    struct epbase *ep, *nep;

    mutex_lock(&sg.lock);
    walk_epbase_safe(ep, nep, &sg.shutdown_head) {
	DEBUG_OFF("eid %d shutdown", ep->eid);
	list_del_init(&ep->item);
	sg.unused[--sg.nendpoints] = ep->eid;
	ep->vfptr.destroy(ep);
    }
    mutex_unlock(&sg.lock);
}

extern const char *xpoll_str[];

static int po_routine_worker(void *args) {
    int rc, i;
    const char *estr;
    struct xpoll_event ent[100];

    while (!sg.exiting) {
	rc = xpoll_wait(sg.po, ent, NELEM(ent, struct xpoll_event), 1);
	if (rc < 0)
	    continue;
	DEBUG_OFF("%d sockets happened events", rc);
	for (i = 0; i < rc; i++) {
	    estr = xpoll_str[ent[i].happened];
	    DEBUG_OFF("socket %d with events %s", ent[i].xd, estr);
	    event_hndl(&ent[i]);
	}
	shutdown_epbase();
    }
    shutdown_epbase();
    return 0;
}

extern struct epbase_vfptr *req_epbase_vfptr;
extern struct epbase_vfptr *rep_epbase_vfptr;

void sp_module_init() {
    int eid;

    sg.exiting = false;
    mutex_init(&sg.lock);
    sg.po = xpoll_create();
    BUG_ON(!sg.po);
    for (eid = 0; eid < XIO_MAX_ENDPOINTS; eid++) {
	sg.unused[eid] = eid;
    }
    sg.nendpoints = 0;
    thread_start(&sg.po_routine, po_routine_worker, 0);
    INIT_LIST_HEAD(&sg.epbase_head);
    list_add_tail(&req_epbase_vfptr->item, &sg.epbase_head);
    list_add_tail(&rep_epbase_vfptr->item, &sg.epbase_head);
    INIT_LIST_HEAD(&sg.shutdown_head);
}


void sp_module_exit() {
    sg.exiting = true;
    thread_stop(&sg.po_routine);
    mutex_destroy(&sg.lock);
    xpoll_close(sg.po);
    BUG_ON(sg.nendpoints);
}

int eid_alloc(int sp_family, int sp_type) {
    struct epbase_vfptr *vfptr = epbase_vfptr_lookup(sp_family, sp_type);
    int eid;
    struct epbase *ep;

    if (!vfptr) {
	errno = EPROTO;
	return -1;
    }
    if (!(ep = vfptr->alloc()))
	return -1;
    ep->vfptr = *vfptr;
    mutex_lock(&sg.lock);
    BUG_ON(sg.nendpoints >= XIO_MAX_ENDPOINTS);
    eid = sg.unused[sg.nendpoints++];
    ep->eid = eid;
    sg.endpoints[eid] = ep;
    atomic_inc(&ep->ref);
    mutex_unlock(&sg.lock);
    return eid;
}

struct epbase *eid_get(int eid) {
    struct epbase *ep;
    mutex_lock(&sg.lock);
    if (!(ep = sg.endpoints[eid])) {
	mutex_unlock(&sg.lock);
	return 0;
    }
    BUG_ON(!atomic_read(&ep->ref));
    atomic_inc(&ep->ref);
    mutex_unlock(&sg.lock);
    return ep;
}

void eid_put(int eid) {
    struct epbase *ep = sg.endpoints[eid];

    BUG_ON(!ep);
    atomic_dec_and_lock(&ep->ref, mutex, sg.lock) {
	list_add_tail(&ep->item, &sg.shutdown_head);
	mutex_unlock(&sg.lock);
    }
}

struct epsk *epsk_new() {
    struct epsk *sk = (struct epsk *)mem_zalloc(sizeof(*sk));
    if (sk) {
	INIT_LIST_HEAD(&sk->snd_cache);
    }
    return sk;
}

void epsk_free(struct epsk *sk) {
    mem_free(sk, sizeof(*sk));
}


void epbase_init(struct epbase *ep) {
    atomic_init(&ep->ref);
    mutex_init(&ep->lock);
    condition_init(&ep->cond);

    ep->rcv.wnd = SP_RCVWND;
    ep->rcv.size = 0;
    ep->rcv.waiters = 0;
    INIT_LIST_HEAD(&ep->rcv.head);

    ep->snd.wnd = SP_SNDWND;
    ep->snd.size = 0;
    ep->snd.waiters = 0;
    INIT_LIST_HEAD(&ep->snd.head);

    INIT_LIST_HEAD(&ep->listeners);
    INIT_LIST_HEAD(&ep->connectors);
    INIT_LIST_HEAD(&ep->bad_socks);
}

void epbase_exit(struct epbase *ep) {
    struct xmsg *msg, *nmsg;
    struct epsk *sk, *nsk;
    struct list_head closed_head;

    INIT_LIST_HEAD(&closed_head);
    list_splice(&ep->snd.head, &closed_head);
    list_splice(&ep->rcv.head, &closed_head);
    walk_msg_safe(msg, nmsg, &closed_head) {
	list_del_init(&msg->item);
	xfreemsg(msg->vec.chunk);
    }
    BUG_ON(!list_empty(&closed_head));
    INIT_LIST_HEAD(&closed_head);
    list_splice(&ep->listeners, &closed_head);
    list_splice(&ep->connectors, &closed_head);
    list_splice(&ep->bad_socks, &closed_head);

    walk_epsk_safe(sk, nsk, &closed_head) {
	list_del_init(&sk->item);
	xclose(sk->fd);
	epsk_free(sk);
    }

    BUG_ON(atomic_read(&ep->ref));
    atomic_destroy(&ep->ref);
    mutex_destroy(&ep->lock);
    condition_destroy(&ep->cond);
}