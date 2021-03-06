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

#include "base.h"
#include "transport.h"
#include "sockaddr.h"

/* SOCKADDR example :
 * ipc    group@ipc://tmp/ipc.sock
 * net    group@net://182.33.49.10:8080
 * inproc group@inproc://inproc.sock
 */
int sockaddr_group (const char *url, char *buff, u32 size)
{
	char *at = strchr (url, '@');;

	if (!at) {
		errno = EINVAL;
		return -1;
	}
	strncpy (buff, url, size <= at - url ? size : at - url);
	return 0;
}

int sockaddr_pf (const char *url)
{
	int pf = 0;
	char *at = strchr (url, '@');;
	char *pfp = strstr (url, "://");;

	if (!pfp || at >= pfp) {
		errno = EINVAL;
		return -1;
	}
	if (at)
		++at;
	else
		at = (char *) url;
#ifdef strndup
	pfp = strndup (at, pfp - at);
#else
	pfp = strdup (at);
#endif
	pf |= strstr (pfp, "tcp") ? TP_TCP : 0;
	pf |= strstr (pfp, "ipc") ? TP_IPC : 0;
	pf |= strstr (pfp, "inproc") ? TP_INPROC : 0;
	free (pfp);
	if (!pf) {
		errno = EINVAL;
		return -1;
	}
	return pf;
}

int sockaddr_addr (const char *url, char *buff, u32 size)
{
	char *tok = "://";
	char *sock = strstr (url, tok);

	if (!sock) {
		errno = EINVAL;
		return -1;
	}
	sock += strlen (tok);
	strncpy (buff, sock, size);
	return 0;
}
