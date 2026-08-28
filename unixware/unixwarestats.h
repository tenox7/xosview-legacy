//
//  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
//
//  This file may be distributed under terms of the GPL
//

#ifndef _UNIXWARESTATS_H_
#define _UNIXWARESTATS_H_

//  The meters read every statistic through this interface, implemented in
//  unixware/unixwarestats.cc on top of the MAS kernel metric file,
//  swapctl(2) and /dev/kmem.  Only the load average needs privileges.
//
//  Every call returns false when its statistic can not be obtained.  Meters
//  disable themselves in that case rather than fail to start, so a machine
//  whose kernel does not register one of these metrics loses a single meter.

namespace UnixWareStats {
  //  Load averages over the last 1, 5 and 15 minutes, in processes.  Read
  //  from kernel memory, so this is the one call that needs privileges.
  bool load( double avg[3] );

  //  Cumulative cpu ticks: user, system, io wait, idle.
  bool cpu( double ticks[4] );

  //  Real memory in bytes.  free is zero until a second sample is available,
  //  because the kernel publishes it as an accumulator rather than a level.
  bool memory( double &total, double &free );

  //  Swap in bytes, totalled over every swap area.
  bool swap( double &total, double &free );

  //  Processors online, at least 1.
  int cpus( void );
}

#endif
