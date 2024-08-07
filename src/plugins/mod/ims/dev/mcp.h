#ifndef __MCP_H
#define __MCP_H

#include "../core/imsrtns.h"

typedef struct sampleinfo_t
{
  unsigned long type;
  void *ptr;
  long length;
  long samprate;
  long loopstart;
  long loopend;
  long sloopstart;
  long sloopend;
} sampleinfo;

enum
{
  mcpSampUnsigned=1,
  mcpSampDelta=2,
  mcpSamp16Bit=4,
  mcpSampBigEndian=8,
  mcpSampLoop=16,
  mcpSampBiDi=32,
  mcpSampSLoop=64,
  mcpSampSBiDi=128,
  mcpSampStereo=256,
  mcpSampFloat=512,
  mcpSampRedBits=0x80000000,
  mcpSampRedRate2=0x40000000,
  mcpSampRedRate4=0x20000000,
  mcpSampRedStereo=0x10000000,
};

enum
{
  mcpMasterVolume, mcpMasterPanning, mcpMasterBalance, mcpMasterSurround,
  mcpMasterSpeed, mcpMasterPitch, mcpMasterBass, mcpMasterTreble,
  mcpMasterReverb, mcpMasterChorus, mcpMasterPause, mcpMasterFilter,
  mcpMasterAmplify,
  mcpGSpeed,
  mcpCVolume, mcpCPanning, mcpCPanY, mcpCPanZ, mcpCSurround, mcpCPosition,
  mcpCPitch, mcpCPitchFix, mcpCPitch6848, mcpCStop, mcpCReset,
  mcpCBass, mcpCTreble, mcpCReverb, mcpCChorus, mcpCMute, mcpCStatus,
  mcpCInstrument, mcpCLoop, mcpCDirect, mcpCFilterFreq, mcpCFilterRez,
  mcpGTimer, mcpGCmdTimer,
  mcpGRestrict
};

int mcpReduceSamples(sampleinfo *s, int n, long m, int o);
enum
{
  mcpRedAlways16Bit=1,
  mcpRedNoPingPong=2,
  mcpRedGUS=4,
  mcpRedToMono=8,
  mcpRedTo8Bit=16,
  //mcpRedToFloat=32,
};


enum
{
  mcpGetSampleStereo=1, mcpGetSampleHQ=2,
};

extern int mcpNChan;

extern int (*mcpLoadSamples)(sampleinfo* si, int n);
extern int (*mcpOpenPlayer)(int, void (*p)());
extern void (*mcpClosePlayer)();
extern void (*mcpSet)(int ch, int opt, int val);
extern int (*mcpGet)(int ch, int opt);
extern void (*mcpGetRealVolume)(int ch, int *l, int *r);
extern void (*mcpGetRealMasterVolume)(int *l, int *r);
extern void (*mcpGetMasterSample)(short *s, int len, int rate, int opt);
extern int (*mcpGetChanSample)(int ch, short *s, int len, int rate, int opt);
extern int (*mcpMixChanSamples)(int *ch, int n, short *s, int len, int rate, int opt);

extern int mcpMixMaxRate;
extern int mcpMixProcRate;
extern int mcpMixOpt;
extern int mcpMixBufSize;
extern int mcpMixMax;
extern int mcpMixPoll;

extern void (*mcpIdle)();


int mcpGetFreq6848(int note);
int mcpGetFreq8363(int note);
int mcpGetNote6848(int freq);
int mcpGetNote8363(int freq);

#endif
