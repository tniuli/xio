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

#ifndef _H_PROXYIO_SCALABILITY_PROTOCOLS_
#define _H_PROXYIO_SCALABILITY_PROTOCOLS_

#include <xio/socket.h>
#include <xio/poll.h>
#include <xio/cmsghdr.h>
#include <xio/cplusplus_define.h>

#define SP_BUF      "sp_ubuf"
#define SP_HD       "sp_uhd"

#define SP_REQREP    1
#define SP_BUS       2
#define SP_PUBSUB    3

int sp_endpoint (int sp_family, int sp_type);
int sp_close (int eid);
int sp_send (int eid, char *ubuf);
int sp_recv (int eid, char **ubuf);
int sp_add (int eid, int fd);
int sp_rm (int eid, int fd);
int sp_setopt (int eid, int opt, void *optval, int optlen);
int sp_getopt (int eid, int opt, void *optval, int *optlen);


static inline int sp_connect (int eid, const char *sockaddr)
{
	int fd = xconnect (sockaddr);

	if (fd < 0)
		return -1;
	return sp_add (eid, fd);
}

static inline int sp_listen (int eid, const char *sockaddr)
{
	int fd = xlisten (sockaddr);

	if (fd < 0)
		return -1;
	return sp_add (eid, fd);
}


#include <xio/cplusplus_endif.h>
#endif
