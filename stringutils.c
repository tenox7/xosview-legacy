#include "stringutils.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

void snprintf_or_abort(char *str, size_t size, const char *format, ...) {
    va_list args;
    int needed;

    va_start(args, format);
    needed = vsnprintf(str, size, format, args);
    va_end(args);

    if (needed < 0) {
        fprintf(stderr, "Internal error: vsnprintf returned an error in "
                "snprintf_or_abort\n");
        abort();
    }
    if ((size_t)needed >= size) {
        fprintf(stderr, "Internal error: Buffer to snprintf_or_abort too "
                "small, required %d but got %lu\n", needed,
                (unsigned long)size);
        abort();
    }
}
