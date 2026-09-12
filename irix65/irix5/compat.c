/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "compat.h"

#include <string.h>
#include <sys/time.h>
#include <sys/types.h>

/*  IRIX 5.3 has vsprintf but nothing bounded, so format into a scratch
 *  buffer big enough for anything xosview prints and copy back what fits.  */
#define SCRATCH 8192

int vsnprintf(char *str, size_t size, const char *fmt, va_list ap) {
    static char scratch[SCRATCH];
    int len = vsprintf(scratch, fmt, ap);

    if (!size)
        return len;

    if ((size_t)len < size) {
        memcpy(str, scratch, len + 1);
        return len;
    }

    memcpy(str, scratch, size - 1);
    str[size - 1] = '\0';
    return len;
}

int snprintf(char *str, size_t size, const char *fmt, ...) {
    va_list ap;
    int len;

    va_start(ap, fmt);
    len = vsnprintf(str, size, fmt, ap);
    va_end(ap);

    return len;
}

/*  A select(2) with no descriptors waits for the timeout and nothing else. */
int usleep(unsigned int usec) {
    struct timeval tv;

    tv.tv_sec = usec / 1000000;
    tv.tv_usec = usec % 1000000;
    return select(0, 0, 0, 0, &tv);
}
