/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#ifndef _SARMETER_H_
#define _SARMETER_H_

#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/elog.h>

/*  some structs  */
typedef struct {
    unsigned int recsize;
    unsigned int numrec;
} header;

typedef struct {
    char           name[12];
    char           pad1[68];
    struct iotime  stat;
    char           pad2[4];
} diskinfo;

#define MAX_DISKS 16
#define SAR_BUFSIZE 0x2000

typedef struct {
    unsigned int swapBuf;
} SarGfxInfo;

typedef struct {
    unsigned int nDevices;

    unsigned int read[MAX_DISKS];
    unsigned int write[MAX_DISKS];
} SarDiskInfo;

/*  Common source for all sar based graphs.  The first call starts sadc and
 *  every later one drains whatever it has written since.  */
SarGfxInfo *sarmeter_gfxinfo(void);
SarDiskInfo *sarmeter_diskinfo(void);

#endif
