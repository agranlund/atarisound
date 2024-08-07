#ifndef __IMSSETUP_H_
#define __IMSSETUP_H_

typedef struct 
{
  int device;
  int rate;
  int stereo;
  int bit16;
  int intrplt;
  int amplify;
  int panning;
  int reverb;
  int chorus;
  int surround;
} imssetupresultstruct;

typedef struct
{
  char *name;
  int nports;
  int nports2;
  int ndmas;
  int ndmas2;
  int nirqs;
  int nirqs2;
  int nrates;
  int mixer;
  int stereo;
  int bit16;
  int amppan;
  int rev;
  int cho;
  int srnd;
  int *ports;
  int *ports2;
  int *dmas;
  int *dmas2;
  int *irqs;
  int *irqs2;
  int *rates;
  int port;
  int port2;
  int dma;
  int dma2;
  int irq;
  int irq2;
} imssetupdevicepropstruct;

int imsSetup(imssetupresultstruct *res, imssetupdevicepropstruct *devprops, int ndevs);

#endif //__IMSSETUP_H_
