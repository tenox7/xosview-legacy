//
//  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
//
//  This file may be distributed under terms of the GPL
//

//  UnixWare 7 keeps its kernel metrics in the MAS metric file, which is what
//  sar(1) and top read, and swap in swapctl(2).  The load average is the one
//  figure MAS does not carry, so it comes out of kernel memory.

#include "unixwarestats.h"

#include <sys/types.h>
#include <sys/param.h>
#include <sys/swap.h>
#include <fcntl.h>
#include <nlist.h>
#include <iostream>
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

bool UnixWareStats::cpu( double ticks[4] ){
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

bool UnixWareStats::memory( double &total, double &avail ){
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

bool UnixWareStats::swap( double &total, double &avail ){
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

//  MAS carries no load average, so read avenrun out of kernel memory at the
//  address nlist() resolves from the boot image.  This is the one statistic
//  here that needs privileges: root, or setgid sys.
static const char KERNEL[] = "/stand/unix";

//  avenrun is fixed point scaled by 1 << FSHIFT, and FSHIFT is 8 here.
static const double FSCALE = 256.0;

bool UnixWareStats::load( double avg[3] ){
  static int kmemfd = -1;
  static unsigned long addr = 0;
  static bool tried = false;
  long avenrun[3];

  if ( !tried ){
    struct nlist nl[2];

    tried = true;
    memset( nl, 0, sizeof(nl) );
    nl[0].n_name = (char *)"avenrun";

    if ( nlist( KERNEL, nl ) != 0 || nl[0].n_value == 0 )
      std::cerr << "Can not resolve 'avenrun' in " << KERNEL << "." << std::endl;
    else {
      addr = nl[0].n_value;
      kmemfd = open( "/dev/kmem", O_RDONLY );
      if ( kmemfd < 0 )
        std::cerr << "Can not open /dev/kmem.  xosview must run as root or be\n"
                  << "  installed setgid sys to show the load average."
                  << std::endl;
    }
  }

  if ( kmemfd < 0 || addr == 0 )
    return false;

  if ( lseek( kmemfd, addr, SEEK_SET ) == -1 ||
       read( kmemfd, avenrun, sizeof(avenrun) ) != (int)sizeof(avenrun) )
    return false;

  for ( int i = 0 ; i < 3 ; i++ )
    avg[i] = avenrun[i] / FSCALE;

  return true;
}

int UnixWareStats::cpus( void ){
  return mas() ? ncpu_ : 1;
}
