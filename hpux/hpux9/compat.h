//
//  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
//
//  This file may be distributed under terms of the GPL
//

#ifndef _HPUX9COMPAT_H_
#define _HPUX9COMPAT_H_

//  HP-UX 9 is a C++ compiler generation behind the other platforms: the
//  newest gcc for it is 2.7, which predates namespaces, and its libc has
//  routines that its headers never declare.  The Makefile force includes
//  this into every translation unit so that the shared xosview sources
//  need no HP-UX 9 specific conditionals.

#include <stdarg.h>
#include <stddef.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>

//  No namespaces, and the libg++ streams are in the global one.
#define std

//  snprintf and vsnprintf are in libc.a but in no header, and vsnprintf is
//  only exported under its internal name.
extern "C" {
  int snprintf( char *, size_t, const char *, ... );
  int _vsnprintf( char *, size_t, const char *, va_list );
}
#define vsnprintf _vsnprintf

//  usleep() arrived in HP-UX 10; a select(2) with no descriptors is the
//  usual stand-in.
inline int usleep( unsigned int usec ){
  struct timeval tv;
  tv.tv_sec = usec / 1000000;
  tv.tv_usec = usec % 1000000;
  return select( 0, 0, 0, 0, &tv );
}

#endif
