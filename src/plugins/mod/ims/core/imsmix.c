// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// IMS mixer replacement routines
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release

// modified for c99 and Atari by agranlund 2024

#include "mcp.h"
#include "mix.h"

void mixGetMasterSample(short *s, int len, int rate, int opt)
{
}

int mixMixChanSamples(int *ch, int n, short *s, int len, int rate, int opt)
{
  return 0;
}

int mixGetChanSample(int ch, short *s, int len, int rate, int opt)
{
  return 0;
}

void mixGetRealVolume(int ch, int *l, int *r)
{
}

void mixGetRealMasterVolume(int *l, int *r)
{
}

void mixSetAmplify(int amp)
{
}

int mixInit(void (*getchan)(int ch, mixchannel *chn, int rate), int masterchan, int chan, int amp)
{
  mcpGetRealVolume=mixGetRealVolume;
  mcpGetChanSample=mixGetChanSample;
  mcpMixChanSamples=mixMixChanSamples;
  if (masterchan)
  {
    mcpGetRealMasterVolume=mixGetRealMasterVolume;
    mcpGetMasterSample=mixGetMasterSample;
  }

  return 1;
}

void mixClose()
{
}
