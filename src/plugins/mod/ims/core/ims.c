// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// IMS library main file (initialisation/deinitialisation)
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release
//  -fd9810xx   Felix Domke <tmbinc@gmx.net>
//    -some hacks for WINIMS. maybe they should be removed.

// modified for c99 and Atari by agranlund 2024

#include <stdlib.h>
#include "imssetup.h"
#include "imsdev.h"
#include "mcp.h"
//#include "player.h"
#include "ims.h"

extern sounddevice mcpUltraSound;
extern sounddevice mcpInterWave;
extern sounddevice mcpSoundBlaster32;

static int sb32ports[]={0x620,0x640,0x660,0x680};
static int stdports[]={0x220,0x240,0x260,0x280};
static int gusirqs[]={2,3,5,7,11,12,15};
static int gusrates[]={19293,22050,32494,44100};

static const int ndevs=1;
static imssetupdevicepropstruct devprops[]=
{
  {"AMD InterWave",             4,0,0,0,0,0,0,0,0,0,2,1,0,0,stdports,0,0,0,0,0,0},
//  {"Gravis UltraSound",         4,0,0,0,0,0,0,0,0,0,2,0,0,0,stdports,0,0,0,0,0,0},
//  {"SoundBlaster 32",           4,0,0,0,0,0,0,0,0,0,2,1,1,0,sb32ports,0,0,0,0,0,0},
};
static sounddevice *snddevs[] = {
    &mcpInterWave,
//    &mcpUltraSound,
//    &mcpSoundBlaster32
};

static int subdevs[]={-1,-1,-1};
static int devopts[]={0,0,0};
static int detectorder[]={0,1,2};

static deviceinfo curdev;
static deviceinfo mixdev;

int imsInit(imsinitstruct *is)
{
  deviceinfo devs[ndevs];

  int i;
  for (i=0; i<ndevs; i++)
  {
    devs[i].port=-1;
    devs[i].port2=-1;
    devs[i].dma=-1;
    devs[i].dma2=-1;
    devs[i].irq=-1;
    devs[i].irq2=-1;
    devs[i].dev=snddevs[i];
    devs[i].opt=devopts[i];
    devs[i].subtype=subdevs[i];
  }

  int devno=-1;
  if (!is->usequiet)
    for (i=0; i<ndevs; i++)
      if (devs[detectorder[i]].dev->Detect(&devs[detectorder[i]]))
      {
        devno=detectorder[i];
      }

  if (devno < 0)
    return 0;

  for (i=0; i<ndevs; i++)
  {
    devprops[i].port=devs[i].port;
    devprops[i].port2=devs[i].port2;
    devprops[i].dma=devs[i].dma;
    devprops[i].dma2=devs[i].dma2;
    devprops[i].irq=devs[i].irq;
    devprops[i].irq2=devs[i].irq2;
  }
  imssetupresultstruct res;
  res.device=devno;
  res.rate=is->rate;
  res.stereo=is->stereo;
  res.bit16=is->bit16;
  res.intrplt=is->interpolate;
  res.amplify=is->amplify;
  res.panning=is->panning;
  res.reverb=is->reverb;
  res.chorus=is->chorus;
  res.surround=is->surround;

  curdev=devs[res.device];
  curdev.port=devprops[res.device].port;
  curdev.port2=devprops[res.device].port2;
  curdev.dma=devprops[res.device].dma;
  curdev.dma2=devprops[res.device].dma2;
  curdev.irq=devprops[res.device].irq;
  curdev.irq2=devprops[res.device].irq2;
  mixdev.dev=0;
/*
  if (devprops[res.device].mixer)
  {
    mixdev.opt=0;
    mixdev.subtype=0;
    mcpMixer.Detect(mixdev);
  }
*/
  if (!curdev.dev->Init(&curdev))
    return 0;
/*    
  if (mixdev.dev)
  {
    if (!mixdev.dev->Init(mixdev))
    {
      curdev.dev->Close();
      return 0;
    }
    mcpMixMaxRate=res.rate;
    mcpMixProcRate=res.rate*64;
    mcpMixOpt=(res.bit16?PLR_16BIT:0)|(res.stereo?PLR_STEREO:0);
    mcpMixBufSize=mcpMixMax=is->bufsize;
    mcpMixPoll=is->bufsize-is->pollmin;
  }
*/
  mcpSet(-1, mcpMasterAmplify, res.amplify*16384/100);
  mcpSet(-1, mcpMasterPanning, res.panning*64/100);
  mcpSet(-1, mcpMasterReverb, res.reverb*64/100);
  mcpSet(-1, mcpMasterChorus, res.chorus*64/100);
  mcpSet(-1, mcpMasterFilter, res.intrplt?1:0);
  mcpSet(-1, mcpMasterSurround, res.surround?1:0);

  return 1;
}

void imsClose()
{
  if (mixdev.dev)
    mixdev.dev->Close();
  curdev.dev->Close();
}

void imsFillDefaults(imsinitstruct *is)
{
  is->bufsize=8192;
  is->pollmin=0;
  is->usequiet=0;
  is->usersetup=1;
  is->rate=44100;
  is->stereo=1;
  is->bit16=1;
  is->interpolate=1;
  is->amplify=100;
  is->panning=100;
  is->reverb=0;
  is->chorus=0;
  is->surround=0;
}