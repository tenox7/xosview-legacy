//  
//  Copyright (c) 1994, 1995 by Mike Romberg ( romberg@fsl.noaa.gov )
//
//  This file may be distributed under terms of the GPL
//

#include "swapmeter.h"
#include "xosview.h"
#include <sys/pstat.h>
#include <stdlib.h>

static int MAX_SWAP_AREAS = 16;

//  Size of one swap pool, in 1 KB blocks.
static unsigned long swapblocks( const struct pst_swapinfo &si ){
#ifdef HPUX9
//  HP-UX 9 has no pss_nblksenabled and keeps the size of a block device
//  pool and of a file system pool in a union instead.
  if ( !(si.pss_flags & SW_ENABLED) )
    return 0;
  return (si.pss_flags & SW_BLOCK) ? si.pss_nblks : si.pss_allocated;
#else
  return si.pss_nblksenabled;
#endif
}

SwapMeter::SwapMeter( XOSView *parent )
: FieldMeterDecay( parent, 2, "SWAP", "USED/FREE" ){
}

SwapMeter::~SwapMeter( void ){
}

void SwapMeter::checkResources( void ){
  FieldMeterDecay::checkResources();

  setfieldcolor( 0, parent_->getResource( "swapUsedColor" ) );
  setfieldcolor( 1, parent_->getResource( "swapFreeColor" ) );
  priority_ = atoi(parent_->getResource( "swapPriority" ) );
  dodecay_ = parent_->isResourceTrue( "swapDecay" );
  SetUsedFormat( parent_->getResource( "swapUsedFormat" ) );
}

void SwapMeter::checkevent( void ){
  static int pass = 0;

  pass = (pass + 1)%5;
  if ( pass != 0 )
    return;
  
  getswapinfo();
  drawfields();
}

void SwapMeter::getswapinfo( void ){
  struct pst_swapinfo swapinfo;

  total_ = 0;
  fields_[1] = 0;

  for (int i = 0 ; i < MAX_SWAP_AREAS ; i++)
      {
      pstat_getswap(&swapinfo, sizeof(swapinfo), 1, i);
      if (swapinfo.pss_idx == (unsigned)i)
          {
          //  In doubles: 4 GB of swap overflows a 32 bit block count.
          total_ += (double)swapblocks( swapinfo ) * 1024.0;
          fields_[1] += (double)swapinfo.pss_nfpgs * 4.0 * 1024.0;
          }
      }

  fields_[0] = total_ - fields_[1];
  setUsed( fields_[0], total_ );
}
