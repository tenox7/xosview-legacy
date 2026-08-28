/*
 *  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
 *
 *  This file may be distributed under terms of the GPL
 */

#ifndef _HPUX9COMPAT_H_
#define _HPUX9COMPAT_H_

/*  HP-UX 9 has libc routines that its headers never declare, and it
 *  predates usleep().  The Makefile force includes this into every
 *  translation unit so that the shared xosview sources need no HP-UX 9
 *  specific conditionals.  */

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

/*  snprintf and vsnprintf are in libc.a but in no header, and vsnprintf is
 *  only exported under its internal name.  */
int snprintf(char *, size_t, const char *, ...);
int _vsnprintf(char *, size_t, const char *, va_list);
#define vsnprintf _vsnprintf

/*  usleep() arrived in HP-UX 10; hpux/hpux9/compat.c stands in for it with
 *  a select(2) on no descriptors, which is the usual substitute.  */
int usleep(unsigned int usec);

#endif
