/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#ifndef _MEMMETER_H_
#define _MEMMETER_H_

#include "fieldmeter.h"
#include <sys/types.h>
#include <sys/sysmp.h>
#include <sys/sysinfo.h>  /*  for SAGET and MINFO structures  */

typedef struct {
    FieldMeter f;
    int pageSize;
    struct rminfo mp;
    int minfosz;
} MemMeter;

Meter *memmeter_new(XOSView *parent);

#endif
