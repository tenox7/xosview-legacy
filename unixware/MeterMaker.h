//
//  Copyright (c) 2026 by Antoni Sawicki ( as@tenoware.com )
//
//  This file may be distributed under terms of the GPL
//

#ifndef _MeterMaker_h
#define _MeterMaker_h

#include "pllist.h"

class XOSView;
class Meter;

class MeterMaker : public PLList<Meter *> {
public:
  MeterMaker( XOSView *xos );

  void makeMeters( void );

private:
  XOSView *_xos;
};

#endif
