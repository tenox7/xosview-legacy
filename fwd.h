/*
 *  Forward declarations shared by the core headers.
 *
 *  Keeping them in one place lets meter.h and xwin.h name each other's
 *  types without including each other, which is what the C++ version
 *  needed the class forward declarations for.
 */

#ifndef _FWD_H_
#define _FWD_H_

typedef struct XWin XWin;
typedef struct XOSView XOSView;
typedef struct Meter Meter;
typedef struct FieldMeter FieldMeter;
typedef struct BitMeter BitMeter;
typedef struct BitFieldMeter BitFieldMeter;

#endif
