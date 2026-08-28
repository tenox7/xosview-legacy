/*
 *  Initial port performed by Stefan Eilemann (eilemann@gmail.com)
 */

#include "sarmeter.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int started = 0;
static int input = 0;
static off_t bufSize = 0;
static char buf[SAR_BUFSIZE];

static struct {
    gfxinfo current;
    gfxinfo last;
    SarGfxInfo info;
} gi;

static struct {
    diskinfo current[MAX_DISKS];
    diskinfo last[MAX_DISKS];
    SarDiskInfo info;
} di;

/*  starts /usr/lib/sa/sadc, from where data is read  */
static int setupSadc(void) {
    char sarPath[] = "/usr/lib/sa/sadc";
    int fd[2];
    int in;

    if (pipe(fd) == -1) {
        perror("setupSar: pipe");
        return 0;
    }

    if (fork() == 0) {  /*  child  */
        close(1);       /*  move fd[write] to stdout  */
        dup(fd[1]);
        close(fd[1]);
        close(fd[0]);   /*  close other end of the pipe  */
        close(2);       /*  close stderr  */
        setbuf(stdout, NULL);  /*  unbuffered stdout  */

        /*  sar wants number of loops: 31536000 is one year  */
        if (execlp(sarPath, sarPath, "1", "31536000", (char *)NULL) == -1)
            perror("setupSar: exec sar");

        /*  not reached  */
        exit(0);
    }

    in = fd[0];
    close(fd[1]);  /*  Close other end of the pipe  */

    fcntl(in, F_SETFL, FNONBLK);

    return in;
}

static void start(void) {
    int i;

    if (started)
        return;
    started = 1;

    input = setupSadc();

    gi.last.gswapbuf = 0;
    gi.info.swapBuf = 0;

    for (i = 0; i < MAX_DISKS; i++) {
        di.last[i].stat.io_bcnt = 0;
        di.last[i].stat.io_wbcnt = 0;

        di.info.nDevices = 0;
        di.info.read[i] = 0;
        di.info.write[i] = 0;
    }
}

static int readLine(void) {
    while (bufSize < SAR_BUFSIZE - 100) {
        int ret = read(input, &buf[bufSize], 100);

        if (ret < 0)
            return 0;  /*  error equals eof in our case  */

        bufSize += ret;

        if (ret < 100)  /*  eof reached  */
            return 0;
    }

    /*  buffer full  */
    return 1;
}

static void forwardBufferTo(char *ptr) {
    size_t moveBytes = ptr - buf;
    size_t bytesLeft = bufSize - moveBytes;

    memmove(buf, ptr, bytesLeft);
    bufSize = bytesLeft;
}

static void newGfxInfo(void) {
    if (gi.last.gswapbuf == 0) {
        gi.last.gswapbuf = gi.current.gswapbuf;
        return;
    }

    gi.info.swapBuf = gi.current.gswapbuf - gi.last.gswapbuf;
    gi.last.gswapbuf = gi.current.gswapbuf;
}

static void newDiskInfo(void) {
    unsigned int i;

    for (i = 0; i < di.info.nDevices; i++) {
        di.info.read[i] =
            (di.current[i].stat.io_bcnt - di.current[i].stat.io_wbcnt) -
            (di.last[i].stat.io_bcnt - di.last[i].stat.io_wbcnt);

        di.info.write[i] =
            di.current[i].stat.io_wbcnt - di.last[i].stat.io_wbcnt;

        di.info.read[i] *= 512;
        di.info.write[i] *= 512;

        di.last[i].stat.io_bcnt = di.current[i].stat.io_bcnt;
        di.last[i].stat.io_wbcnt = di.current[i].stat.io_wbcnt;
    }
}

static void parseBuffer(void) {
    while (bufSize > 100) {
        /*  look for 'S' (starting of Sarmagic)  */
        char *ptr = (char *)memchr(buf, 'S', bufSize);

        /*  not found -- discard this buffer  */
        if (ptr == NULL) {
            bufSize = 0;
            return;
        }

        /*  not enough data read  */
        if ((ptr + 100) > (buf + bufSize))
            return;

        if (memcmp(ptr, "SarmagicGFX", 11) == 0) {
            forwardBufferTo(ptr);

            /*  data is not complete in buffer  */
            if (bufSize < (off_t)(24 + sizeof(gfxinfo)))
                return;

            /*  retrieve gfxinfo structure  */
            ptr = buf + 24;
            memcpy(&gi.current, ptr, sizeof(gfxinfo));
            ptr += sizeof(gfxinfo);

            forwardBufferTo(ptr);
            newGfxInfo();
        } else if (memcmp(ptr, "SarmagicNEODISK", 15) == 0) {
            int num, i;

            forwardBufferTo(ptr);

            ptr = buf + 20;

            /*  number of records [devices]  */
            memcpy(&num, ptr, 4);
            ptr += 4;

            if (num > MAX_DISKS)
                num = MAX_DISKS;

            /*  data is not complete in buffer  */
            if (bufSize < (off_t)(24 + num * sizeof(diskinfo)))
                return;

            di.info.nDevices = num;

            /*  read disk info  */
            for (i = 0; i < num; i++) {
                memcpy(&di.current[i], ptr, sizeof(diskinfo));
                ptr += sizeof(diskinfo);
            }

            forwardBufferTo(ptr);
            newDiskInfo();
        } else {  /*  no known Sarmagic record  */
            forwardBufferTo(ptr + 1);
        }
    }
}

static void checkSadc(void) {
    int dataInPipe = 1;

    start();

    while (dataInPipe) {
        dataInPipe = readLine();
        parseBuffer();
    }
}

SarGfxInfo *sarmeter_gfxinfo(void) {
    checkSadc();
    return &gi.info;
}

SarDiskInfo *sarmeter_diskinfo(void) {
    checkSadc();
    return &di.info;
}
