/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _IRIX5COMPAT_H_
#define _IRIX5COMPAT_H_

/*  IRIX 5.3 predates both snprintf and usleep: neither its headers nor its
 *  libc have them.  The Makefile force includes this into every translation
 *  unit so that the shared xosview sources need no IRIX 5 conditionals.  */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>

int snprintf(char *, size_t, const char *, ...);
int vsnprintf(char *, size_t, const char *, va_list);
int usleep(unsigned int usec);

#endif
