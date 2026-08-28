//
//  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
//
//  This file may be distributed under terms of the GPL
//

#include "memmeter.h"
#include "unixwarestats.h"
#include <stdlib.h>

MemMeter::MemMeter( XOSView *parent )
  : FieldMeterDecay( parent, 2, "MEM", "USED/FREE" ){
  double total, free;

  ok_ = UnixWareStats::memory( total, free );

  if ( !ok_ )
    disableMeter();
}

MemMeter::~MemMeter( void ){
}

void MemMeter::checkResources( void ){
  FieldMeterDecay::checkResources();

  priority_ = atoi( parent_->getResource( "memPriority" ) );

  //  disableMeter() collapsed this meter to a single field, so setting the
  //  per field colours below would run off the end of the array.
  if ( !ok_ )
    return;

  setfieldcolor( 0, parent_->getResource( "memUsedColor" ) );
  setfieldcolor( 1, parent_->getResource( "memFreeColor" ) );
  dodecay_ = parent_->isResourceTrue( "memDecay" );
  SetUsedFormat( parent_->getResource( "memUsedFormat" ) );
}

void MemMeter::checkevent( void ){
  getmeminfo();
  drawfields();
}

void MemMeter::getmeminfo( void ){
  //  The kernel keeps freefilemem identical to freemem on this release, so
  //  the file cache can not be shown as a field of its own.
  if ( !UnixWareStats::memory( total_, fields_[1] ) )
    return;

  fields_[0] = total_ - fields_[1];

  setUsed( fields_[0], total_ );
}
