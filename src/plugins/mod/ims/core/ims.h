#ifndef __IMS_H
#define __IMS_H

#include "imsrtns.h"

typedef struct
{
  int usequiet;
  int usersetup;
  int rate;
  int stereo;
  int bit16;
  int interpolate;
  int amplify;
  int panning;
  int reverb;
  int chorus;
  int surround;
  int bufsize;
  int pollmin;
} imsinitstruct;

void imsFillDefaults(imsinitstruct* is);
int imsInit(imsinitstruct* is);
void imsClose();

#endif //__IMS_H

