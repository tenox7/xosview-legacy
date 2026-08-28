/*
 *  Copyright (c) 1999, 2006 by Mike Romberg (mike.romberg@noaa.gov)
 *
 *  This file may be distributed under terms of the GPL
 */

#include "diskmeter.h"
#include "xosview.h"
#include "xwin.h"
#include <dirent.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define MAX_PROCSTAT_LENGTH 4096

/*---------------------------------------------------------------------------*/

static unsigned long diskmap_get(const DiskMap *m, const char *name) {
  int i;

  for (i = 0; i < m->n; i++)
    if (strcmp(m->e[i].name, name) == 0)
      return m->e[i].value;

  return 0;  /*  an unseen disk reads as zero, as std::map's default did  */
}

static void diskmap_set(DiskMap *m, const char *name, unsigned long value) {
  int i;

  for (i = 0; i < m->n; i++)
    if (strcmp(m->e[i].name, name) == 0) {
      m->e[i].value = value;
      return;
    }

  if (m->n == m->cap) {
    int cap = m->cap ? m->cap * 2 : 8;
    DiskEntry *grown = (DiskEntry *)realloc(m->e, cap * sizeof(DiskEntry));

    if (grown == NULL) {
      fprintf(stderr, "Out of memory.\n");
      exit(1);
    }
    m->e = grown;
    m->cap = cap;
  }
  snprintf(m->e[m->n].name, sizeof m->e[m->n].name, "%s", name);
  m->e[m->n].value = value;
  m->n++;
}

static void diskmap_copy(DiskMap *dst, const DiskMap *src) {
  int i;

  dst->n = 0;
  for (i = 0; i < src->n; i++)
    diskmap_set(dst, src->e[i].name, src->e[i].value);
}

static void diskmap_free(DiskMap *m) {
  free(m->e);
  m->e = NULL;
  m->n = m->cap = 0;
}

/*---------------------------------------------------------------------------*/

static void destroy(Meter *m) {
  DiskMeter *dm = (DiskMeter *)m;

  diskmap_free(&dm->sysfs_read_prev);
  diskmap_free(&dm->sysfs_write_prev);
  fieldmeter_fini(m);
}

static void checkres(Meter *m) {
  FieldMeter *fm = (FieldMeter *)m;

  fieldmeter_checkresources(m);

  fieldmeter_setcolorname(fm, 0, xwin_getresource(m->xw, "diskReadColor"));
  fieldmeter_setcolorname(fm, 1, xwin_getresource(m->xw, "diskWriteColor"));
  fieldmeter_setcolorname(fm, 2, xwin_getresource(m->xw, "diskIdleColor"));
  m->priority = atoi(xwin_getresource(m->xw, "diskPriority"));
  fm->dodecay = xwin_isresourcetrue(m->xw, "diskDecay");
  fm->usegraph = xwin_isresourcetrue(m->xw, "diskGraph");
  fieldmeter_setusedformat(fm, xwin_getresource(m->xw, "diskUsedFormat"));
}

/*  IMHO the logic here is quite broken - but for backward compat UNCHANGED: */
static void updateinfo(DiskMeter *dm, unsigned long one, unsigned long two,
                       int fudgeFactor) {
  /*  assume each "unit" is 1k.
   *  This is true for ext2, but seems to be 512 bytes for vfat and 2k for
   *  cdroms; work in 512-byte blocks.
   *
   *  tw: strange, on my system, a ext2fs (read and write) unit seems to be
   *  2048.  kernel 2.2.12 and the file system is on a SW-RAID5 device
   *  (/dev/md0).
   *
   *  So this is a FIXME - but how ???  */
  FieldMeter *fm = &dm->f;
  double itim = fieldmeter_usecs(fm);
  unsigned long read_curr = one * fudgeFactor;   /*  FIXME!  */
  unsigned long write_curr = two * fudgeFactor;  /*  FIXME!  */

  /*  avoid strange values at first call  */
  if (dm->read_prev == 0)
    dm->read_prev = read_curr;
  if (dm->write_prev == 0)
    dm->write_prev = write_curr;

  /*  calculate rate in bytes per second  */
  fm->fields[0] = ((read_curr - dm->read_prev) * 1e6 * 512) / itim;
  fm->fields[1] = ((write_curr - dm->write_prev) * 1e6 * 512) / itim;

  /*  fix overflow (conversion bug?)  */
  if (fm->fields[0] < 0.0)
    fm->fields[0] = 0.0;
  if (fm->fields[1] < 0.0)
    fm->fields[1] = 0.0;

  if (fm->fields[0] + fm->fields[1] > fm->total)
    fm->total = fm->fields[0] + fm->fields[1];

  fm->fields[2] = fm->total - (fm->fields[0] + fm->fields[1]);

  dm->read_prev = read_curr;
  dm->write_prev = write_curr;

  fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
  fieldmeter_timerstart(fm);
}

static FILE *openstats(DiskMeter *dm) {
  FILE *f = fopen(dm->statFileName, "r");

  if (!f) {
    fprintf(stderr, "Can not open file : %s\n", dm->statFileName);
    exit(1);
  }
  return f;
}

static void getvmdiskinfo(DiskMeter *dm) {
  FILE *f;
  char buf[MAX_PROCSTAT_LENGTH];
  unsigned long one = 0, two = 0;

  fieldmeter_timerstop(&dm->f);
  dm->f.total = dm->maxspeed;
  f = openstats(dm);

  /*  kernel >= 2.5  */
  while (fscanf(f, "%4095s", buf) == 1 && strncmp(buf, "pgpgin", 7))
    ;
  if (fscanf(f, "%lu", &one) != 1)
    one = 0;

  while (fscanf(f, "%4095s", buf) == 1 && strncmp(buf, "pgpgout", 7))
    ;
  if (fscanf(f, "%lu", &two) != 1)
    two = 0;

  fclose(f);
  updateinfo(dm, one, two, 4);
}

static void getdiskinfo(DiskMeter *dm) {
  FILE *f;
  char buf[MAX_PROCSTAT_LENGTH];
  unsigned long one = 0, two = 0;
  unsigned long junk, read1, write1;

  fieldmeter_timerstop(&dm->f);
  dm->f.total = dm->maxspeed;
  f = openstats(dm);

  /*  Find the line with 'disk_io:'  */
  while (fscanf(f, "%4095s", buf) == 1 && strncmp(buf, "disk_io:", 8))
    ;

  /*  read values  */
  while (fscanf(f, "%4095s", buf) == 1
         && sscanf(buf, "(%lu,%lu):(%lu,%lu,%lu,%lu,%lu)", &junk, &junk,
                   &junk, &junk, &read1, &junk, &write1) == 7) {
    one += read1;
    two += write1;
  }

  fclose(f);
  updateinfo(dm, one, two, 1);
}

/*  sysfs version - works with long-long !!  */
static void update_info(DiskMeter *dm, const DiskMap *reads,
                        const DiskMap *writes) {
  FieldMeter *fm = &dm->f;
  double itim = fieldmeter_usecs(fm);
  /*  the sum of all disks  */
  unsigned long long all_bytes_read = 0, all_bytes_written = 0;
  /*  from linux-3.10/Documentation/block/stat.txt  */
  unsigned int sect_size = 512;
  int i;

  /*  avoid strange values at first call (by this - the first value
   *  displayed becomes zero)  */
  if (dm->sysfs_read_prev.n == 0) {
    diskmap_copy(&dm->sysfs_read_prev, reads);
    diskmap_copy(&dm->sysfs_write_prev, writes);
    itim = 1;  /*  itim is garbage here too.  Valgrind complains.  */
  }

  for (i = 0; i < reads->n; i++) {
    unsigned long prev = diskmap_get(&dm->sysfs_read_prev, reads->e[i].name);

    if (reads->e[i].value < prev)  /*  counter wrapped  */
      all_bytes_read += ULONG_MAX - prev + reads->e[i].value;
    else
      all_bytes_read += reads->e[i].value - prev;
  }
  for (i = 0; i < writes->n; i++) {
    unsigned long prev = diskmap_get(&dm->sysfs_write_prev, writes->e[i].name);

    if (writes->e[i].value < prev)  /*  counter wrapped  */
      all_bytes_written += ULONG_MAX - prev + writes->e[i].value;
    else
      all_bytes_written += writes->e[i].value - prev;
  }

  all_bytes_read *= sect_size;
  all_bytes_written *= sect_size;
  XOSDEBUG("disk: read: %llu, written: %llu\n", all_bytes_read,
           all_bytes_written);

  /*  convert rate from bytes/microsec into bytes/second  */
  fm->fields[0] = all_bytes_read * (1e6 / itim);
  fm->fields[1] = all_bytes_written * (1e6 / itim);

  /*  fix overflow (conversion bug?)  */
  if (fm->fields[0] < 0.0)
    fm->fields[0] = 0.0;
  if (fm->fields[1] < 0.0)
    fm->fields[1] = 0.0;

  /*  bump up max total  */
  if (fm->fields[0] + fm->fields[1] > fm->total)
    fm->total = fm->fields[0] + fm->fields[1];

  fm->fields[2] = fm->total - (fm->fields[0] + fm->fields[1]);

  /*  save old vals for next round  */
  diskmap_copy(&dm->sysfs_read_prev, reads);
  diskmap_copy(&dm->sysfs_write_prev, writes);

  fieldmeter_setused(fm, fm->fields[0] + fm->fields[1], fm->total);
  fieldmeter_timerstart(fm);
}

/*  XXX: sysfs - read Documentation/iostats.txt !!!
 *  extract stats from /sys/block/<dev>/stat
 *  each disk reports an unsigned long, which can WRAP around  */
static void getsysfsdiskinfo(DiskMeter *dm) {
  /*  field-3: sects read since boot (but can wrap!)
   *  field-7: sects written since boot (but can wrap!)
   *  just sum up everything in /sys/block/<dev>/stat  */
  char disk[PATH_MAX], tmp[PATH_MAX];
  struct stat buf;
  char line[128];
  unsigned long vals[7];
  DiskMap reads, writes;
  struct dirent *dirent;
  DIR *dir;

  memset(&reads, 0, sizeof reads);
  memset(&writes, 0, sizeof writes);

  fieldmeter_timerstop(&dm->f);
  dm->f.total = dm->maxspeed;

  dir = opendir(dm->statFileName);
  if (dir == NULL) {
    XOSDEBUG("sysfs: Cannot open directory : %s\n", dm->statFileName);
    return;
  }

  /*  visit every /sys/block/<dev>/stat and sum up the values  */
  while ((dirent = readdir(dir)) != NULL) {
    FILE *diskstat;

    if (strncmp(dirent->d_name, ".", 1) == 0 ||
        strncmp(dirent->d_name, "..", 2) == 0 ||
        strncmp(dirent->d_name, "loop", 4) == 0 ||
        strncmp(dirent->d_name, "ram", 3) == 0)
      continue;

    snprintf(disk, sizeof disk, "%s/%s", dm->statFileName, dirent->d_name);
    if (stat(disk, &buf) != 0 || !(buf.st_mode & S_IFDIR)) {
      XOSDEBUG("disk is not dir: %s - errno=%d\n", disk, errno);
      continue;
    }

    /*  only scan for real HW (raid, md, and lvm all mapped on them)  */
    snprintf(tmp, sizeof tmp, "%s/device", disk);
    if (lstat(tmp, &buf) != 0 || (buf.st_mode & S_IFLNK) == 0)
      continue;

    /*  is a dir, locate 'stat' file in it  */
    snprintf(tmp, sizeof tmp, "%s/stat", disk);
    diskstat = fopen(tmp, "r");
    if (!diskstat) {
      XOSDEBUG("disk stat open: %s - errno=%d\n", tmp, errno);
      continue;
    }

    if (fgets(line, sizeof line, diskstat)) {
      char *cur = line, *end;
      int i;

      for (i = 0; i < 7; i++) {
        vals[i] = strtoul(cur, &end, 10);
        cur = end;
      }
      diskmap_set(&reads, dirent->d_name, vals[2]);
      diskmap_set(&writes, dirent->d_name, vals[6]);

      XOSDEBUG("disk stat: %s | read: %lu, written: %lu\n", tmp, vals[2],
               vals[6]);
    }
    fclose(diskstat);
  }
  closedir(dir);

  update_info(dm, &reads, &writes);
  diskmap_free(&reads);
  diskmap_free(&writes);
}

static void checkevent(Meter *m) {
  DiskMeter *dm = (DiskMeter *)m;

  if (dm->vmstat)
    getvmdiskinfo(dm);
  else if (dm->sysfs)
    getsysfsdiskinfo(dm);
  else
    getdiskinfo(dm);

  fieldmeter_drawfields(&dm->f, 0);
}

Meter *diskmeter_new(XOSView *parent, float max) {
  DiskMeter *dm = (DiskMeter *)meter_alloc(sizeof *dm);
  struct stat buf;

  fieldmeter_init(&dm->f, parent, 3, "DiskMeter", "DISK", "READ/WRITE/IDLE",
                  0, 0, 0);
  dm->f.m.checkres = checkres;
  dm->f.m.checkevent = checkevent;
  dm->f.m.destroy = destroy;

  memset(&dm->sysfs_read_prev, 0, sizeof dm->sysfs_read_prev);
  memset(&dm->sysfs_write_prev, 0, sizeof dm->sysfs_write_prev);
  dm->read_prev = 0;
  dm->write_prev = 0;
  dm->maxspeed = max;
  dm->sysfs = dm->vmstat = 0;
  dm->statFileName = "/proc/stat";

  /*  first - try sysfs  */
  if (stat("/sys/block", &buf) == 0 && buf.st_mode & S_IFDIR) {
    dm->sysfs = 1;
    dm->statFileName = "/sys/block";
    XOSDEBUG("diskmeter: using sysfs /sys/block\n");
    getsysfsdiskinfo(dm);
  } else if (stat("/proc/vmstat", &buf) == 0 && buf.st_mode & S_IFREG) {
    /*  try vmstat  */
    dm->vmstat = 1;
    dm->statFileName = "/proc/vmstat";
    getvmdiskinfo(dm);
  } else {  /*  fall back to stat  */
    getdiskinfo(dm);
  }

  return &dm->f.m;
}
