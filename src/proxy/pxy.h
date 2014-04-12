#ifndef _HPIO_PXY_
#define _HPIO_PXY_

#include <uuid/uuid.h>
#include <base.h>
#include <channel/channel.h>
#include <ds/map.h>
#include <ds/list.h>
#include <runner/thread.h>
#include <sync/spin.h>
#include "hdr.h"

struct fd;
struct xg;
struct pxy;

typedef void (*event_handler) (struct fd *f, uint32_t events);

struct fd {
    struct upoll_event event;
    int cd;
    int ty;
    uint32_t ok:1;
    event_handler h;
    struct pxy *y;
    struct xg *g;
    uint64_t mq_size;
    struct list_head mq;
    struct list_head link;
    uuid_t uuid;
    ssmap_node_t rb_link;
};

#define list_for_each_fd(f, nx, head)				\
    list_for_each_entry_safe(f, nx, head, struct fd, link)

struct fd *fd_new();
void fd_free(struct fd *f);


struct xg {
    char group[URLNAME_MAX];
    int ref;
    ssmap_node_t pxy_rb_link;
    ssmap_t fdmap;
    int rsz;
    struct list_head rcver_head;
    int ssz;
    struct list_head snder_head;
    struct list_head link;
};


#define list_for_each_xg(g, ng, head)				\
    list_for_each_entry_safe(g, ng, head, struct xg, link)

struct xg *xg_new();
void xg_free(struct xg *g);
struct fd *xg_find(struct xg *g, uuid_t ud);
int xg_add(struct xg *g, struct fd *f);
int xg_rm(struct xg *g, struct fd *f);
struct fd *xg_rrbin_go(struct xg *g);
struct fd *xg_route_back(struct xg *g, uuid_t ud);



struct pxy {
    spin_t lock;
    int flags;
    thread_t backend;

    struct upoll_tb *tb;
    int gsz;
    struct list_head g_head;
    ssmap_t gmap;
    struct list_head listener_head;
    struct list_head unknown_head;
};

/* URL example : 
 * net    group@net://182.33.49.10
 * ipc    group@ipc://xxxxxxxxxxxx
 * inproc group@inproc://xxxxxxxxxxx
 */
int url_parse_group(const char *url, char *buff, u32 size);
int url_parse_pf(const char *url);
int url_parse_sockaddr(const char *url, char *buff, u32 size);


struct pxy *pxy_new();
void pxy_free(struct pxy *y);

int pxy_listen(struct pxy *y, const char *url);
int pxy_connect(struct pxy *y, const char *url);

int pxy_onceloop(struct pxy *y);
int pxy_startloop(struct pxy *y);
void pxy_stoploop(struct pxy *y);

#endif
