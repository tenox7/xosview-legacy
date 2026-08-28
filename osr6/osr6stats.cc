//
//  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
//
//  This file may be distributed under terms of the GPL
//

//  SCO OpenServer 6 keeps its kernel metrics in the MAS metric file, which
//  is what sar(1) and top read.  Swap comes from swapctl(2) and the load
//  average is accumulated from /proc, because the kernel publishes neither.

#include "osr6stats.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/swap.h>
#include <sys/procfs.h>
#include <dirent.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <mas.h>
#include <metreg.h>

static int masfd_ = -1;
static bool masopen_ = false;
static int ncpu_ = 1;

//  Opens the metric file once.  Every entry point calls this first, so a
//  machine without MAS simply reports nothing rather than crashing.
static bool mas( void ){
  unsigned int *n;

  if ( masopen_ )
    return masfd_ >= 0;

  masopen_ = true;
  masfd_ = mas_open( MAS_FILE, MAS_MMAP_ACCESS );
  if ( masfd_ < 0 )
    return false;

  n = (unsigned int *)mas_get_met( masfd_, NCPU, 0 );
  if ( n )
    ncpu_ = *(short *)n;
  if ( ncpu_ < 1 )
    ncpu_ = 1;

  return true;
}

//  Per cpu metrics are registered once for each processor, so a system wide
//  figure is the sum over all of them.
static unsigned int metric( int id ){
  unsigned int total = 0;

  for ( int i = 0 ; i < ncpu_ ; i++ ){
    unsigned int *p = (unsigned int *)mas_get_met( masfd_, id, i );
    if ( p )
      total += *p;
  }

  return total;
}

bool OSR6Stats::cpu( double ticks[4] ){
  if ( !mas() )
    return false;

  ticks[0] = metric( MPC_CPU_USR );
  ticks[1] = metric( MPC_CPU_SYS );
  ticks[2] = metric( MPC_CPU_WIO );
  ticks[3] = metric( MPC_CPU_IDLE );

  return true;
}

//  freemem is an accumulator: the kernel adds the current free page count to
//  it once a second, so the level is the difference between two samples over
//  the seconds between them.  This is what sar -r and top do.  Returns -1
//  until a second sample is available.
static long freepages( void ){
  static dl_t prev;
  static time_t prevtime = 0;
  dl_t *cur, diff, den, quot;
  time_t now;
  long secs;

  cur = (dl_t *)mas_get_met( masfd_, FREEMEM, 0 );
  if ( !cur )
    return -1;

  now = time( NULL );
  if ( prevtime == 0 ){
    prev = *cur;
    prevtime = now;
    return -1;
  }

  //  Too soon to divide; keep the baseline rather than resetting it.
  secs = (long)( now - prevtime );
  if ( secs < 1 )
    return -1;

  diff = lsub( *cur, prev );
  den.dl_lop = (ulong_t)secs;
  den.dl_hop = 0;
  quot = ldivide( diff, den );

  prev = *cur;
  prevtime = now;

  return (long)quot.dl_lop;
}

bool OSR6Stats::memory( double &total, double &avail ){
  static double lastfree = 0;
  double pagesize = getpagesize();
  long pages = sysconf( _SC_TOTAL_MEMORY );

  if ( !mas() || pages <= 0 )
    return false;

  total = (double)pages * pagesize;

  //  The kernel registers freefilemem too, but keeps it identical to freemem
  //  on this release, so the file cache can not be shown separately.
  long f = freepages();
  if ( f >= 0 )
    lastfree = (double)f * pagesize;
  avail = lastfree;

  return true;
}

bool OSR6Stats::swap( double &total, double &avail ){
  swaptbl_t *swt;
  char *paths;
  double pagesize = getpagesize();
  bool ok = false;
  int n, i;

  total = avail = 0;

  n = swapctl( SC_GETNSWP, 0 );
  if ( n < 0 )
    return false;
  if ( n == 0 )
    return true;

  swt = (swaptbl_t *)malloc( sizeof(int) + n * sizeof(swapent_t) );
  //  swapctl() writes the path of each area back through these pointers, so
  //  every entry has to be given somewhere to put one.
  paths = (char *)malloc( n * MAXPATHLEN );

  if ( swt && paths ){
    swt->swt_n = n;
    for ( i = 0 ; i < n ; i++ )
      swt->swt_ent[i].ste_path = paths + i * MAXPATHLEN;

    if ( swapctl( SC_LIST, swt ) >= 0 ){
      for ( i = 0 ; i < swt->swt_n ; i++ ){
        total += (double)swt->swt_ent[i].ste_pages * pagesize;
        avail += (double)swt->swt_ent[i].ste_free * pagesize;
      }
      ok = true;
    }
  }

  free( paths );
  free( swt );

  return ok;
}

//  Processes on or waiting for a processor.  Our own is always on-proc while
//  the scan runs, so it does not count.
static int runnable( void ){
  DIR *dir;
  struct dirent *ent;
  int run = 0;

  dir = opendir( "/proc" );
  if ( !dir )
    return -1;

  while ( ( ent = readdir( dir ) ) != NULL ){
    psinfo_t ps;
    char path[MAXPATHLEN];
    int fd;

    if ( ent->d_name[0] < '0' || ent->d_name[0] > '9' )
      continue;

    snprintf( path, sizeof(path), "/proc/%s/psinfo", ent->d_name );
    fd = open( path, O_RDONLY );
    if ( fd < 0 )
      continue;

    if ( read( fd, &ps, sizeof(ps) ) == (int)sizeof(ps) &&
         ( ps.pr_lwp.pr_sname == 'R' || ps.pr_lwp.pr_sname == 'O' ) )
      run++;
    close( fd );
  }
  closedir( dir );

  return run > 0 ? run - 1 : 0;
}

bool OSR6Stats::load( double avg[3] ){
  //  avenrun exists as a symbol but the kernel never updates it, which is why
  //  uptime(1) always prints 0.00, and there is no moving run queue metric
  //  either.  Decay run queue samples into a 1 minute average here instead.
  //  Only that one is real; the 5 and 15 minute figures repeat it.
  static double load = -1.0;
  static time_t prevtime = 0;
  time_t now;
  int run = runnable();

  if ( run < 0 )
    return false;

  now = time( NULL );
  if ( load < 0.0 ){
    load = run;
  } else {
    long secs = (long)( now - prevtime );
    if ( secs < 1 )
      secs = 1;
    double weight = exp( -(double)secs / 60.0 );
    load = load * weight + run * ( 1.0 - weight );
  }
  prevtime = now;

  avg[0] = avg[1] = avg[2] = load;

  return true;
}

int OSR6Stats::cpus( void ){
  return mas() ? ncpu_ : 1;
}
