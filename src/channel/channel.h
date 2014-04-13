#ifndef _HPIO_CHANNEL_
#define _HPIO_CHANNEL_

#include <inttypes.h>
#include "transport/transport.h"


#define PF_NET    TP_TCP
#define PF_IPC    TP_IPC
#define PF_INPROC TP_MOCK_INPROC


char *channel_allocmsg(uint32_t size);
void channel_freemsg(char *payload);
uint32_t channel_msglen(char *payload);

int channel_listen(int pf, const char *sock);
int channel_connect(int pf, const char *peer);
int channel_accept(int cd);
int channel_recv(int cd, char **payload);
int channel_send(int cd, char *payload);
void channel_close(int cd);

#define CHANNEL_NOBLOCK 1
#define CHANNEL_SNDBUF  2
#define CHANNEL_RCVBUF  4
int channel_setopt(int cd, int opt, void *val, int valsz);
int channel_getopt(int cd, int opt, void *val, int valsz);


/* EXPORT user poll implementation, different from channel_poll */

#define UPOLLIN   1
#define UPOLLOUT  2
#define UPOLLERR  4

struct upoll_event {
    int cd;
    void *self;

    /* What events i care about ... */
    uint32_t care;

    /* What events happened now */
    uint32_t happened;
};

struct upoll_tb;

struct upoll_tb *upoll_create();
void upoll_close(struct upoll_tb *tb);

#define UPOLL_ADD 1
#define UPOLL_DEL 2
#define UPOLL_MOD 3

/* All channel must be remove from upoll before closed
 * Can't modify an unexist channel.
 */
int upoll_ctl(struct upoll_tb *ut, int op, struct upoll_event *ue);
int upoll_wait(struct upoll_tb *ut, struct upoll_event *events, int n,
	       u32 timeout);


#endif
