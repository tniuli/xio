#ifndef _BASE_H_
#define _BASE_H_

#include <stdlib.h>
#include <string.h>

#define true 1
#define false 0

#define PATH_MAX 4096
#define ZERO(x) memset(&(x), 0, sizeof(x))
#define ERROR (-1 & __LINE__)
#define STREQ(a, b) (strlen(a) == strlen(b) && memcmp(a, b , strlen(a)) == 0)

// Get offset of a member
#define __offsetof(TYPE, MEMBER) ((long) &(((TYPE *)0)->MEMBER))

// Casts a member of a structure out to the containning structure
// @param ptr             the pointer to the member
// @param type            the type of the container struct this is embeded in.
// @param member          the field name of the member within the struct
#define container_of(ptr, type, member) ({				\
	    (type *)((char *)ptr - __offsetof(type, member)); })


#endif
