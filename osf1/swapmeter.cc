//
//  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
//
//  This file may be distributed under terms of the GPL
//

#include "swapmeter.h"
#include "osf1stats.h"
#include <stdlib.h>

//  TBL_SWAPINFO is walked one partition at a time, so the totals here cover
//  every swap device the machine has configured.

SwapMeter::SwapMeter( XOSView *parent )
  : FieldMeterDecay( parent, 2, "SWAP", "USED/FREE" ){
  double total, free;

  ok_ = OSF1Stats::swap( total, free );

  if ( !ok_ )
    disableMeter();
}

SwapMeter::~SwapMeter( void ){
}

void SwapMeter::checkResources( void ){
  FieldMeterDecay::checkResources();

  priority_ = atoi( parent_->getResource( "swapPriority" ) );

  //  disableMeter() collapsed this meter to a single field, so setting the
  //  per field colours below would run off the end of the array.
  if ( !ok_ )
    return;

  setfieldcolor( 0, parent_->getResource( "swapUsedColor" ) );
  setfieldcolor( 1, parent_->getResource( "swapFreeColor" ) );
  dodecay_ = parent_->isResourceTrue( "swapDecay" );
  SetUsedFormat( parent_->getResource( "swapUsedFormat" ) );
}

void SwapMeter::checkevent( void ){
  getswapinfo();
  drawfields();
}

void SwapMeter::getswapinfo( void ){
  if ( !OSF1Stats::swap( total_, fields_[1] ) )
    return;

  fields_[0] = total_ - fields_[1];

  setUsed( fields_[0], total_ );
}
