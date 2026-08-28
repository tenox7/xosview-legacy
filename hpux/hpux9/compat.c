/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#include "compat.h"

#include <sys/time.h>

/*  HP-UX 9 has no usleep(); a select(2) with no descriptors waits for the
 *  timeout and nothing else.  */
int usleep(unsigned int usec) {
  struct timeval tv;

  tv.tv_sec = usec / 1000000;
  tv.tv_usec = usec % 1000000;
  return select(0, 0, 0, 0, &tv);
}
