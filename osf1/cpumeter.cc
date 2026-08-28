//
//  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
//
//  This file may be distributed under terms of the GPL
//

#include "cpumeter.h"
#include "osf1stats.h"
#include <stdlib.h>

//  The cpu ticks are accumulated system wide, so a multiprocessor machine
//  gets a single aggregate meter and the cpuFormat resource has no effect.

CPUMeter::CPUMeter( XOSView *parent )
  : FieldMeterGraph( parent, 5, "CPU", "USR/NICE/SYS/WIO/IDLE" ){
  double probe[5];

  for ( int i = 0 ; i < 2 ; i++ )
    for ( int j = 0 ; j < 5 ; j++ )
      cputime_[i][j] = 0;

  cpuindex_ = 0;
  ok_ = OSF1Stats::cpu( probe );

  if ( !ok_ )
    disableMeter();
}

CPUMeter::~CPUMeter( void ){
}

int CPUMeter::countCPUs( void ){
  return OSF1Stats::cpus();
}

void CPUMeter::checkResources( void ){
  FieldMeterGraph::checkResources();

  priority_ = atoi( parent_->getResource( "cpuPriority" ) );

  //  disableMeter() collapsed this meter to a single field, so setting the
  //  per field colours below would run off the end of the array.
  if ( !ok_ )
    return;

  setfieldcolor( 0, parent_->getResource( "cpuUserColor" ) );
  setfieldcolor( 1, parent_->getResource( "cpuNiceColor" ) );
  setfieldcolor( 2, parent_->getResource( "cpuSystemColor" ) );
  setfieldcolor( 3, parent_->getResource( "cpuWaitColor" ) );
  setfieldcolor( 4, parent_->getResource( "cpuFreeColor" ) );
  dodecay_ = parent_->isResourceTrue( "cpuDecay" );
  useGraph_ = parent_->isResourceTrue( "cpuGraph" );
  SetUsedFormat( parent_->getResource( "cpuUsedFormat" ) );
}

void CPUMeter::checkevent( void ){
  getcputime();
  drawfields();
}

void CPUMeter::getcputime( void ){
  if ( !OSF1Stats::cpu( cputime_[cpuindex_] ) )
    return;

  total_ = 0;

  int oldindex = ( cpuindex_ + 1 ) % 2;
  for ( int i = 0 ; i < 5 ; i++ ){
    fields_[i] = cputime_[cpuindex_][i] - cputime_[oldindex][i];
    if ( fields_[i] < 0 )
      fields_[i] = 0;
    total_ += fields_[i];
  }
  cpuindex_ = oldindex;

  if ( total_ )
    setUsed( total_ - fields_[4], total_ );
}
