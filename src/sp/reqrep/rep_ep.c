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

#include "rep_ep.h"

static struct epbase *rep_ep_alloc() {
    struct rep_ep *rep_ep = (struct rep_ep *)mem_zalloc(sizeof(*rep_ep));

    if (rep_ep) {
	epbase_init(&rep_ep->base);
	rep_ep->peer = 0;
    }
    return &rep_ep->base;
}

static void rep_ep_destroy(struct epbase *ep) {
    struct rep_ep *rep_ep = cont_of(ep, struct rep_ep, base);
    BUG_ON(!rep_ep);
    epbase_exit(ep);
    mem_free(rep_ep, sizeof(*rep_ep));
}

static int rep_ep_add(struct epbase *ep, struct socktg *sk, char *ubuf) {
    struct xmsg *msg = cont_of(ubuf, struct xmsg, vec.xiov_base);
    struct rrr *r = rt_cur(ubuf);
    
    if (uuid_compare(r->uuid, sk->uuid))
	uuid_copy(sk->uuid, r->uuid);
    DEBUG_OFF("ep %d recv req %10.10s from socket %d", ep->eid, ubuf, sk->fd);
    mutex_lock(&ep->lock);
    list_add_tail(&msg->item, &ep->rcv.head);
    ep->rcv.size += xubuflen(ubuf);
    BUG_ON(ep->rcv.waiters < 0);
    if (ep->rcv.waiters)
	condition_broadcast(&ep->cond);
    mutex_unlock(&ep->lock);
    return 0;
}

static void __routeback(struct epbase *ep, struct xmsg *msg) {
    char *ubuf = msg->vec.xiov_base;
    struct rrr *rt = rt_cur(ubuf);
    struct socktg *tg = 0;

    get_socktg_if(tg, &ep->connectors, !uuid_compare(tg->uuid, rt->uuid));
    if (tg) {
	list_add_tail(&msg->item, &tg->snd_cache);
	__socktg_try_enable_out(tg);
    } else
	xfreemsg(msg);
}

static void routeback(struct epbase *ep) {
    struct xmsg *msg, *nmsg;

    mutex_lock(&ep->lock);
    walk_msg_s(msg, nmsg, &ep->snd.head) {
	list_del_init(&msg->item);
	__routeback(ep, msg);
    }
    mutex_unlock(&ep->lock);
}

static int rep_ep_rm(struct epbase *ep, struct socktg *sk, char **ubuf) {
    struct xmsg *msg = 0;
    struct rrhdr *rr_hdr = 0;

    routeback(ep);
    if (list_empty(&sk->snd_cache))
	return -1;
    BUG_ON(!(msg = list_first(&sk->snd_cache, struct xmsg, item)));
    list_del_init(&msg->item);
    *ubuf = msg->vec.xiov_base;
    
    mutex_lock(&ep->lock);
    ep->snd.size -= xubuflen(*ubuf);
    BUG_ON(ep->snd.waiters < 0);
    if (ep->snd.waiters)
	condition_broadcast(&ep->cond);
    mutex_unlock(&ep->lock);

    rr_hdr = get_rrhdr(*ubuf);
    rr_hdr->go = 0;
    rr_hdr->end_ttl = rr_hdr->ttl;
    DEBUG_OFF("ep %d send resp %10.10s to socket %d", ep->eid, *ubuf, sk->fd);
    return 0;
}

static int rep_ep_join(struct epbase *ep, struct socktg *sk, int nfd) {
    struct socktg *nsk = sp_generic_join(ep, nfd);

    if (!nsk)
	return -1;
    return 0;
}

static int set_proxyto(struct epbase *ep, void *optval, int optlen) {
    int rc, backend_eid = *(int *)optval;
    struct epbase *peer = eid_get(backend_eid);

    if (!peer) {
	errno = EBADF;
	return -1;
    }
    rc = epbase_proxyto(ep, peer);
    eid_put(backend_eid);
    return rc;
}

static const ep_setopt setopt_vfptr[] = {
    0,
    set_proxyto,
};

static const ep_getopt getopt_vfptr[] = {
    0,
    0,
};

static int rep_ep_setopt(struct epbase *ep, int opt, void *optval, int optlen) {
    int rc;
    if (opt < 0 || opt >= NELEM(setopt_vfptr, ep_setopt) || !setopt_vfptr[opt]) {
	errno = EINVAL;
	return -1;
    }
    rc = setopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

static int rep_ep_getopt(struct epbase *ep, int opt, void *optval, int *optlen) {
    int rc;
    if (opt < 0 || opt >= NELEM(getopt_vfptr, ep_getopt) || !getopt_vfptr[opt]) {
	errno = EINVAL;
	return -1;
    }
    rc = getopt_vfptr[opt] (ep, optval, optlen);
    return rc;
}

struct epbase_vfptr rep_epbase = {
    .sp_family = SP_REQREP,
    .sp_type = SP_REP,
    .alloc = rep_ep_alloc,
    .destroy = rep_ep_destroy,
    .add = rep_ep_add,
    .rm = rep_ep_rm,
    .join = rep_ep_join,
    .setopt = rep_ep_setopt,
    .getopt = rep_ep_getopt,
};

struct epbase_vfptr *rep_epbase_vfptr = &rep_epbase;






