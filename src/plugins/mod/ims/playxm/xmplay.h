#ifndef __XMPLAY_H_
#define __XMPLAY_H_

#include "binfile.h"

enum
{
  xmpEnvLoop=1, xmpEnvBiDi=2, xmpEnvSLoop=4
};

typedef struct
{
  unsigned char *env;
  unsigned short len;
  unsigned short loops, loope;
  unsigned short sustain;
  unsigned char type;
  unsigned char speed;
} xmpenvelope;

typedef struct
{
  char name[32];
  unsigned short handle;
  signed short normnote;
  signed short normtrans;
  signed short stdvol;
  signed short stdpan;
  unsigned short opt;
#define MP_OFFSETDIV2 1
  unsigned short volfade;
  unsigned char pchint;
  unsigned short volenv;
  unsigned short panenv;
  unsigned short pchenv;
  unsigned char vibspeed;
  unsigned char vibtype;
  unsigned short vibrate;
  unsigned short vibdepth;
  unsigned short vibsweep;
} xmpsample;

typedef struct __attribute((packed))
{
  char name[32];
  unsigned short samples[128];
} xmpinstrument;

typedef struct __attribute((packed))
{
  char name[21];
  char ismod;
  int linearfreq;
  int nchan;
  int ninst;
  int nenv;
  int npat;
  int nord;
  int nsamp;
  int nsampi;
  int loopord;
  unsigned char initempo;
  unsigned char inibpm;

  xmpenvelope *envelopes;
  xmpsample *samples;
  xmpinstrument *instruments;
  struct sampleinfo_t *sampleinfos;
  unsigned short *patlens;
  unsigned char (**patterns)[5];
  unsigned short *orders;
  unsigned char panpos[256];
} xmodule;

typedef struct __attribute((packed))
{
  unsigned char globvol;
  unsigned char globvolslide;
} xmpglobinfo;

typedef struct __attribute((packed))
{
  unsigned char note;
  unsigned char vol;
  unsigned char pan;
  unsigned char notehit;
  unsigned char volslide;
  unsigned char pitchslide;
  unsigned char panslide;
  unsigned char volfx;
  unsigned char pitchfx;
  unsigned char notefx;
  unsigned char fx;
} xmpchaninfo;

enum
{
  xmpFXIns=0,
  xmpFXNote=1,
  xmpFXVol=2,
  xmpFXCmd=3,
  xmpFXData=4
};

#if 1

int xmpLoadSamples(xmodule *m);
int xmpLoadModule(xmodule *m, binfile *f);
int xmpLoadMOD(xmodule *m, binfile *f);
int xmpLoadMODt(xmodule *m, binfile *f);
int xmpLoadMODd(xmodule *m, binfile *f);
int xmpLoadMODf(xmodule *m, binfile *f);
int xmpLoadM31(xmodule *m, binfile *f);
int xmpLoadM15(xmodule *m, binfile *f);
int xmpLoadM15t(xmodule *m, binfile *f);
int xmpLoadWOW(xmodule *m, binfile *f);
int xmpLoadMXM(xmodule *m, binfile *f);
void xmpFreeModule(xmodule *m);

int xmpPlayModule(xmodule *m);
void xmpStopModule();
void xmpSetPos(int ord, int row);

void xmpGetRealVolume(int i, int *l, int *r);
void xmpMute(int i, int m);
int xmpGetLChanSample(int ch, short *b, int len, int rate, int opt);
unsigned short xmpGetPos();
int xmpGetRealPos();
int xmpGetDotsData(int ch, int *smp, int *frq, int *l, int *r, int *sus);
int xmpPrecalcTime(xmodule *m, int startpos, int (*calc)[2], int n, int ite);
int xmpLoop();
void xmpSetLoop(int);
int xmpGetChanIns(int);
int xmpGetChanSamp(int);
int xmpChanActive(int);
void xmpGetGlobInfo(int *tmp, int *bpm, int *gvol);
void xmpOptimizePatLens(xmodule *m);
int xmpGetSync(int ch, int *time);
int xmpGetTime();

int xmpGetTickTime();
int xmpGetRowTime();
void xmpSetEvPos(int ch, int pos, int modtype, int mod);
int xmpGetEvPos(int ch, int *time);
int xmpFindEvPos(int pos, int *time);

void xmpGetChanInfo(unsigned char ch, xmpchaninfo *ci);
unsigned short xmpGetRealNote(unsigned char ch);
void xmpGetGlobInfo2(xmpglobinfo *gi);


#endif

enum
{
  xmpCmdArpeggio=0,xmpCmdPortaU=1,xmpCmdPortaD=2,xmpCmdPortaNote=3,
  xmpCmdVibrato=4,xmpCmdPortaVol=5,xmpCmdVibVol=6,xmpCmdTremolo=7,
  xmpCmdPanning=8,xmpCmdOffset=9,xmpCmdVolSlide=10,xmpCmdJump=11,
  xmpCmdVolume=12,xmpCmdBreak=13,xmpCmdSpeed=15,xmpCmdGVolume=16,
  xmpCmdGVolSlide=17,xmpCmdKeyOff=20,xmpCmdEnvPos=21,xmpCmdPanSlide=25,
  xmpCmdMRetrigger=27,xmpCmdTremor=29,xmpCmdXPorta=33,xmpCmpSFilter=36,
  xmpCmdFPortaU=37,xmpCmdFPortaD=38,xmpCmdGlissando=39,xmpCmdVibType=40,
  xmpCmdSFinetune=41,xmpCmdPatLoop=42,xmpCmdTremType=43,xmpCmdSPanning=44,
  xmpCmdRetrigger=45,xmpCmdFVolSlideU=46, xmpCmdFVolSlideD=47,
  xmpCmdNoteCut=48,xmpCmdDelayNote=49,xmpCmdPatDelay=50, xmpCmdSync1=28,
  xmpCmdSync2=32, xmpCmdSync3=51,
  xmpVCmdVol0x=1,xmpVCmdVol1x=2,xmpVCmdVol2x=3,xmpVCmdVol3x=4,xmpVCmdVol40=5,
  xmpVCmdVolSlideD=6,xmpVCmdVolSlideU=7,xmpVCmdFVolSlideD=8,
  xmpVCmdFVolSlideU=9,xmpVCmdVibRate=10,xmpVCmdVibDep=11,xmpVCmdPanning=12,
  xmpVCmdPanSlideL=13,xmpVCmdPanSlideR=14,xmpVCmdPortaNote=15,
  xmpCmdMODtTempo=128,
};

enum
{
  xfxGVSUp=1, xfxGVSDown,
  xfxVSUp=1, xfxVSDown,
  xfxPSUp=1, xfxPSDown, xfxPSToNote,
  xfxPnSRight=1, xfxPnSLeft,
  xfxVXVibrato=1, xfxVXTremor,
  xfxPXVibrato=1, xfxPXArpeggio,
  xfxPnXVibrato=1,
  xfxNXNoteCut=1, xfxNXRetrig, xfxNXDelay,

  xfxVolSlideUp=1, xfxVolSlideDown,
  xfxRowVolSlideUp, xfxRowVolSlideDown,
  xfxPitchSlideUp, xfxPitchSlideDown, xfxPitchSlideToNote,
  xfxRowPitchSlideUp, xfxRowPitchSlideDown,
  xfxPanSlideRight, xfxPanSlideLeft,
  xfxVolVibrato, xfxTremor,
  xfxPitchVibrato, xfxArpeggio,
  xfxNoteCut, xfxRetrig,
  xfxOffset, xfxEnvPos,
  xfxDelay, xfxSetFinetune,
};

#endif // __XMPLAY_H_