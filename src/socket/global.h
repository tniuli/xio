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

#ifndef _H_PROXYIO_XGLOBAL_
#define _H_PROXYIO_XGLOBAL_

#include <utils/mutex.h>
#include <utils/list.h>
#include "inproc.h"
#include "tcpipc.h"
#include "worker.h"

/* Max number of cpu core */
#define PROXYIO_MAX_CPUS 32


/* Max number of concurrent socks. */
#define PROXYIO_MAX_SOCKS 10240

struct xglobal {
	int exiting;
	mutex_t lock;

	/* The global table of existing xsock. The descriptor representing
	 * the xsock is the index to this table. This pointer is also used to
	 * find out whether context is initialised. If it is null, context is
	 * uninitialised.
	 */
	struct sockbase *sockbases[PROXYIO_MAX_SOCKS];

	/* Stack of unused xsock descriptors.  */
	int unused[PROXYIO_MAX_SOCKS];

	/* Number of actual socks. */
	size_t nsockbases;


	struct worker cpus[PROXYIO_MAX_CPUS];

	/* Stack of unused xsock descriptors.  */
	int cpu_unused[PROXYIO_MAX_CPUS];

	/* Number of actual runner poller.  */
	size_t ncpus;
	size_t ncpus_low;
	size_t ncpus_high;

	/* Backend cpu_cores and taskpool for cpu_worker.  */
	int cpu_cores;
	struct taskpool tpool;

	/* INPROC global listening address mapping */
	struct ssmap inproc_listeners;

	/* Sockbase_vfptr head */
	struct list_head sockbase_vfptr_head;
};

#define walk_sockbase_vfptr_s(pos, nx, head)				\
    walk_each_entry_s(pos, nx, head, struct sockbase_vfptr, link)

struct sockbase_vfptr *sockbase_vfptr_lookup (int pf, int type);

extern struct xglobal xgb;

static inline void xglobal_lock()
{
	mutex_lock (&xgb.lock);
}

static inline void xglobal_unlock()
{
	mutex_unlock (&xgb.lock);
}



#endif
