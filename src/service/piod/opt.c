#include <stdio.h>
#include "core/core.h"

static char __usage[] = "\n\
NAME\n\
    pio - a high throughput, low latency network communication middleware\n\
\n\
SYNOPSIS\n\
    pio -r time -w threads -m ipaddr\n\
\n\
OPTIONS\n\
    -r service running time. default forever\n\
    -w threads. backend threadpool workers number\n\n\
    -m monitor_center host = ip + port\n\
\n\
EXAMPLE:\n\
    pio -r 60 -w 20\n\n";


static inline void usage() {
    system("clear");
    printf("%s", __usage);
}

int64_t deadline = 0;

int getoption(int argc, char* argv[], struct cf *cf) {
    int rc;

    while ( (rc = getopt(argc, argv, "r:w:m:h")) != -1 ) {
        switch(rc) {
	case 'r':
	    deadline = rt_mstime() + atoi(optarg) * 1E3;
	    break;
        case 'w':
	    cf->tp_workers = atoi(optarg);
            break;
	case 'm':
	    cf->monitor_addr = optarg;
	    break;
        case 'h':
	    usage();
	    return -1;
        }
    }
    return 0;
}

