#ifndef __MP_H
#define __MP_H

#include "gmdinst.h"
#include "../core/binfile.h"

#define MP_MAXCHANNELS 32

struct sampleinfo;

typedef struct
{
  unsigned char *ptr;
  unsigned char *end;
} gmdtrack;

typedef struct
{
  char name[32];
  unsigned short patlen;
  unsigned short gtrack;
  unsigned short tracks[MP_MAXCHANNELS];
} gmdpattern;


#define MOD_TICK0 1
#define MOD_EXPOFREQ 2
#define MOD_S3M 4
#define MOD_GUSVOL 8
#define MOD_EXPOPITCHENV 16
#define MOD_S3M30 32
#define MOD_MODPAN 0x10000

typedef struct
{
  char name[32];
  char composer[32];
  unsigned long options;
  int channum;
  int instnum;
  int patnum;
  int ordnum;
  int endord;
  int loopord;
  int tracknum;
  int sampnum;
  int modsampnum;
  int envnum;
  gmdinstrument *instruments;
  gmdtrack *tracks;
  gmdenvelope *envelopes;
  sampleinfo *samples;
  gmdsample *modsamples;
  gmdpattern *patterns;
  char **message;
  unsigned short *orders;
} gmdmodule;

typedef struct
{
  unsigned char speed;
  unsigned char curtick;
  unsigned char tempo;
  unsigned char currow;
  unsigned short patlen;
  unsigned short curpat;
  unsigned short patnum;
  unsigned char globvol;
  unsigned char globvolslide;
} globinfo;

typedef struct
{
  unsigned char ins;
  unsigned short smp;
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
} chaninfo;

void mpReset(gmdmodule *m);
void mpFree(gmdmodule *m);
int mpAllocInstruments(gmdmodule *m, int n);
int mpAllocSamples(gmdmodule *m, int n);
int mpAllocModSamples(gmdmodule *m, int n);
int mpAllocTracks(gmdmodule *m, int n);
int mpAllocPatterns(gmdmodule *m, int n);
int mpAllocEnvelopes(gmdmodule *m, int n);
int mpAllocOrders(gmdmodule *m, int n);
void mpOptimizePatLens(gmdmodule *m);
void mpReduceInstruments(gmdmodule *m);
void mpReduceMessage(gmdmodule *m);
int mpReduceSamples(gmdmodule *m);
int mpLoadSamples(gmdmodule *m);
void mpRemoveText(gmdmodule *m);


extern int mpLoadMOD(gmdmodule *, binfile *);
extern int mpLoadMODt(gmdmodule *, binfile *);
extern int mpLoadMODd(gmdmodule *, binfile *);
extern int mpLoadM15(gmdmodule *, binfile *);
extern int mpLoadM15t(gmdmodule *, binfile *);
extern int mpLoadM31(gmdmodule *, binfile *);
extern int mpLoadWOW(gmdmodule *, binfile *);
extern int mpLoadMTM(gmdmodule *, binfile *);
extern int mpLoadS3M(gmdmodule *, binfile *);
extern int mpLoadXM (gmdmodule *, binfile *);
extern int mpLoad669(gmdmodule *, binfile *);
extern int mpLoadULT(gmdmodule *, binfile *);
extern int mpLoadDMF(gmdmodule *, binfile *);
extern int mpLoadOKT(gmdmodule *, binfile *);
extern int mpLoadPTM(gmdmodule *, binfile *);
extern int mpLoadAMS(gmdmodule *, binfile *);
extern int mpLoadMDL(gmdmodule *, binfile *);
extern int mpLoadIT (gmdmodule *, binfile *);


char mpPlayModule(const gmdmodule *m);
void mpStopModule();
void mpSetPosition(signed short pat, signed short row);
void mpGetPosition(unsigned short *pat, unsigned char *row);
int mpGetRealPos();
void mpGetChanInfo(unsigned char ch, chaninfo *ci);
unsigned short mpGetRealNote(unsigned char ch);
void mpGetGlobInfo(globinfo *gi);
char mpLooped();
void mpSetLoop(unsigned char s);
void mpLockPat(int st);
int mpGetChanSample(int ch, short *buf, int len, int rate, int opt);
void mpMute(int ch, int m);
void mpGetRealVolume(int ch, int *l, int *r);
int mpGetChanStatus(int ch);
int mpGetMute(int ch);

enum
{
  cmdTempo, cmdSpeed, cmdBreak, cmdGoto, cmdPatLoop, cmdPatDelay, cmdGlobVol, cmdGlobVolSlide, cmdSetChan, cmdFineSpeed
};

enum
{
  cmdVolSlideUp, cmdVolSlideDown, cmdRowVolSlideUp, cmdRowVolSlideDown,
  cmdPitchSlideUp, cmdPitchSlideDown, cmdPitchSlideToNote,
  cmdRowPitchSlideUp, cmdRowPitchSlideDown,
  cmdPanSlide, cmdRowPanSlide,
  cmdDelay,
  cmdVolVibrato, cmdVolVibratoSetWave, cmdTremor,
  cmdPitchVibrato, cmdPitchVibratoSetSpeed, cmdPitchVibratoFine,
  cmdPitchVibratoSetWave, cmdArpeggio,
  cmdNoteCut, cmdRetrig,
  cmdOffset,
  cmdPanSurround,
  cmdKeyOff,
  cmdSetEnvPos,

  cmdVolSlideUDMF, cmdVolSlideDDMF,
  cmdPanSlideLDMF, cmdPanSlideRDMF,
  cmdPitchSlideUDMF, cmdPitchSlideDDMF, cmdPitchSlideNDMF, cmdRowPitchSlideDMF,
  cmdVolVibratoSinDMF, cmdVolVibratoTrgDMF, cmdVolVibratoRecDMF,
  cmdPanVibratoSinDMF,
  cmdPitchVibratoSinDMF, cmdPitchVibratoTrgDMF, cmdPitchVibratoRecDMF,

  cmdPanDepth,
  cmdPanHeight,

  cmdChannelVol,

  cmdSpecial,
  cmdOffsetHigh,

  cmdOffsetEnd,
  cmdSetDir,
  cmdSetLoop,

  cmdPlayNote=0x80, cmdPlayIns=0x01, cmdPlayNte=0x02, cmdPlayVol=0x04, cmdPlayPan=0x08, cmdPlayDelay=0x10
};

enum
{
  cmdContVolSlide,
  cmdContRowVolSlide,
  cmdContMixVolSlide,
  cmdContMixVolSlideUp,
  cmdContMixVolSlideDown,
  cmdContMixPitchSlideUp,
  cmdContMixPitchSlideDown,
  cmdGlissOn,
  cmdGlissOff,
};

enum
{
  fxGVSUp=1, fxGVSDown,
  fxVSUp=1, fxVSDown, fxVSUDMF, fxVSDDMF,
  fxPSUp=1, fxPSDown, fxPSToNote, fxPSUDMF, fxPSDDMF, fxPSNDMF,
  fxPnSRight=1, fxPnSLeft, fxPnSLDMF, fxPnSRDMF,
  fxVXVibrato=1, fxVXTremor,
  fxPXVibrato=1, fxPXArpeggio,
  fxPnXVibrato=1,
  fxNXNoteCut=1, fxNXRetrig,

  fxVolSlideUp=1, fxVolSlideDown,
  fxRowVolSlideUp, fxRowVolSlideDown,
  fxPitchSlideUp, fxPitchSlideDown, fxPitchSlideToNote,
  fxRowPitchSlideUp, fxRowPitchSlideDown,
  fxPanSlideRight, fxPanSlideLeft,
  fxVolVibrato, fxTremor,
  fxPitchVibrato, fxArpeggio,
  fxNoteCut, fxRetrig,
  fxOffset,
  fxDelay,
  fxPanVibrato
};

#endif
