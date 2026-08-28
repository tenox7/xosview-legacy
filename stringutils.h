#ifndef _STRINGUTILS_H_
#define _STRINGUTILS_H_

#include <stdlib.h>

/*  Like snprintf, except that instead of truncating the string, it
 *  prints an error message and abort()s.  */
void snprintf_or_abort(char *str, size_t size, const char *format, ...);

#endif
