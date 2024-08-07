// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// Wavetable Device: AMD InterWave
//
// revision history: (please note changes here)
//  -nb980510   Tammo Hinrichs <kb@nwn.de>
//    -first release
//  -kb980511   Tammo Hinrichs <opencp@gmx.net>
//    -changed mixing frequency again to the right value (IW plays
//     everything at approx. 101% pitch of what it should)
//    -removed smooth panning (caused dropouts because of the IW's
//     logarithmic volume registers)
//    -added card detection by reading of IW.INI file (finally)
//    -fixed wrong synchronization when the IW's timer is used.
//     Note: the resolution of the timer (player ticks) is enough
//     for OpenCP's screen display, but NOT for IMS based apps... so
//     don't ever dare activate the IW timer there.
//    -made reverb somewhat smoother
//    -added panning for Step C or above chips (they have linear instead
//     of logarithmic panning registers)
//    -changed pitch and timer speed again, as it still seemed wrong.
//     (that's why i like software mixing ;)
//  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
//    -removed iw step c panning again (didnt work as expected)
//    -rewritten mem handling (didnt work at all)
//  -fd981206   Felix Domke    <tmbinc@gmx.net>
//    -edited for new binfile

// modified for c99 and Atari by agranlund 2024

#include <string.h>
#include <stdlib.h>
#include "imsdev.h"
#include "mcp.h"
#include "mix.h"
#include "irq.h"
#include "timer.h"
#include "imsrtns.h"

#define FAKE_GUS 0

#define MAXSAMPLES   256

#define FXBUFFERSIZE 65536

extern sounddevice mcpInterWave;

static unsigned short iwPort;
static unsigned char iwIRQ;
static unsigned char iwRev;

static unsigned char bufferbank;
static unsigned long bufferpos;

static unsigned long iwMem[4];
static unsigned long memsize;
static unsigned long largestbank;
static unsigned long lbsize;

static unsigned short linvol[513];

static unsigned char useiwtimer;
static unsigned char iweffects;
static unsigned char forceeffects;

static int  rdelay[4]   = { 3395, 2023, 2857, 1796};
static int  rfeedback[4]= { 0x80, 0xc0, 0xa0,0x100};
static char rfxchans[4] = { 0xc0, 0xc0 ,0x30, 0x30};
static int  rpanning[4] = {0x152, 0xAB,0x200,0x000};
static char pan2chan[4] = { 0xA0, 0x30, 0x30, 0x50};

static inline void delayIW(unsigned int len) {
    // todo: better delay..
    for (unsigned int i=0; i<=len; i++) {
        inp(0x300);
    }
}

static unsigned char inpIW(unsigned short p)
{
  return inp(iwPort+p);
}

static void outpIW(unsigned short p, unsigned char v)
{
  outp(iwPort+p,v); delayIW(4);
}

static void outIW(unsigned char c, unsigned char v)
{
  outp(iwPort+0x103, c); delayIW(4);
  outp(iwPort+0x105, v); delayIW(4);
}

static void outwIW(unsigned char c, unsigned short v)
{
  outp(iwPort+0x103, c); delayIW(4);
  outpw(iwPort+0x104, v); delayIW(4);
}

static unsigned char inIW(unsigned char c)
{
  outp(iwPort+0x103, c); delayIW(4);
  return inp(iwPort+0x105);
}

static unsigned short inwIW(unsigned char c)
{
  outp(iwPort+0x103, c); delayIW(4);
  return inpw(iwPort+0x104);
}

static void resetIW()
{
  int i;

  outIW(0x4C, 0);
  for (i=0; i<20; i++)
    inp(iwPort+0x107);

  outIW(0x4C, 1);
  for (i=0; i<20; i++)
    inp(iwPort+0x107);
}

static unsigned char peekIW(unsigned long adr)
{
  outwIW(0x43, adr);
  outIW(0x44, adr>>16);
  return inpIW(0x107);
}

static void pokeIW(unsigned long adr, unsigned char data)
{
  outwIW(0x43, adr);
  outIW(0x44, adr>>16);
  outpIW(0x107, data);
}

static void selvoc(char ch)
{
  outpIW(0x102, ch);
}

static void setfreq(unsigned short frq)
{
  outwIW(0x01, frq);
}

static void setvol(unsigned short vol)
{
  outwIW(0x09, vol<<4);
}

static unsigned short getvol()
{
  return inwIW(0x89)>>4;
}

static void setrelvoll(unsigned short vol,char mode)
{
  vol=0xfff-linvol[vol];
  if (!mode)
    outwIW(0x13,vol<<4);
  outwIW(0x1C,vol<<4);
}

static void setrelvolr(unsigned short vol,char mode)
{
  vol=0xfff-linvol[vol];
  if (!mode)
    outwIW(0x0c,vol<<4);
  outwIW(0x1B,vol<<4);
}

static void seteffvol(unsigned short vol)
{
  vol=0xfff-vol;
  outwIW(0x16,vol<<4);
  outwIW(0x1D,vol<<4);
}

static void seteffchan(char ch)
{
  outIW(0x14,ch);
}

static void setbank(char b)
{
  outIW(0x10,b);
}

static void setpoint(unsigned long p, unsigned char t)
{
  t=(t==1)?0x02:(t==2)?0x04:(t==3)?0x11:0x0A;  // new: t==3 -> FX buffer write position
  outwIW(t, p>>7);
  outwIW(t+1, p<<9);
}

static unsigned char getbank()
{
  return (inIW(0x90)&0x03);
}

static unsigned long getpoint()
{
  return (inwIW(0x8A)<<7)|(inwIW(0x8B)>>9);
}

static void setmode(unsigned char m)
{
  outIW(0x00, m);
}

static unsigned char getmode()
{
  return inIW(0x80);
}

static void setvst(unsigned char s)
{
  outIW(0x07, s);
}

static void setvend(unsigned char s)
{
  outIW(0x08, s);
}

static void setvmode(unsigned char m)
{
  outIW(0x0D, m);
}

static unsigned char getvmode()
{
  return inIW(0x8D);
}

static void settimer(unsigned char o)
{
  outIW(0x45, o);
}

static void settimerlen(unsigned char l)
{
  outIW(0x46, l);
}

static char setenhmode(unsigned char m)
{
  if (m)
  {
    outIW(0x19,inIW(0x99)|0x01);
    return inIW(0x99)&0x01;
  }
  outIW(0x19,inIW(0x99)&~0x01);
  return 1;
}

static unsigned long findMem(unsigned long max)
{
  char v0,v1,v2,v3;
  unsigned long fnd=0;

  //dbgprintf("findmem %d\r\n", max / 1024);
  unsigned long bankmax = 4 * 1024 * 1024UL;
  unsigned long step    = 64 * 1024;
  unsigned long count   = bankmax / step;

  for (int b=0; b<4; b++)
  {
    int ba=b<<22;
    iwMem[b]=ba;

    v0=peekIW(ba);
    v1=peekIW(ba+1);
    pokeIW(ba,0x55);
    pokeIW(ba+1,0x56);

    unsigned char testval=0x55;
    int i;
    for (i=0; i<count; i++)
    {
      if (fnd==max) {
        //dbgprintf("found max\r\n");
        break;
      }
      v2=peekIW(iwMem[b]);
      v3=peekIW(iwMem[b]+1);
      pokeIW(iwMem[b],testval);
      pokeIW(iwMem[b]+1,testval+1);
      if ((peekIW(iwMem[b])!=testval)||(peekIW(iwMem[b]+1)!=(testval+1))||(peekIW(ba)!=0x55)||(peekIW(ba+1)!=0x56)) {
        //dbgprintf("rw mismatch %02x,%02x, %02x,%02x, %02x,%02x\r\n", peekIW(ba), peekIW(ba+1), peekIW(iwMem[b]), peekIW(iwMem[b+1]), testval, testval+1);
	    break;
      }
      iwMem[b] += step;
      fnd += step;
      testval+=2;
      pokeIW(iwMem[b],v2);
      pokeIW(iwMem[b]+1,v3);
    }

    pokeIW(0,v0);
    pokeIW(1,v1);
    //dbgprintf("iwmem[d] = 0x%08x : %d\r\n", iwMem[b], (iwMem[b]-ba) / 1024);
    iwMem[b]-=ba;
  }
  return fnd;
}

static char testPort(unsigned short port)
{
#if FAKE_GUS
  unsigned long maxsize=4096*1024;
  iwMem[0] = maxsize;
  iwMem[1] = maxsize;
  iwMem[2] = maxsize;
  iwMem[3] = maxsize;

  for (int b=1; b<4; b++) if (iwMem[b]&&iwMem[b]<maxsize)
  {
    maxsize=iwMem[b];
    bufferbank=b;
  }

  if (iweffects)
    iwMem[bufferbank]-=FXBUFFERSIZE;

  bufferpos=iwMem[bufferbank]>>1;
  memsize=iwMem[0]+iwMem[1]+iwMem[2]+iwMem[3];
  iwPort = port;
  iwRev = 1;
//  iwRev=inIW(0x5b)>>4;

#else
  iwPort=port;

  resetIW();

  char v0,v1;

  v0=peekIW(0);
  v1=peekIW(1);

  pokeIW(0,0xAA);
  pokeIW(1,0x55);

  char iw=peekIW(0)==0xAA;

  pokeIW(0,v0);
  pokeIW(1,v1);

  if (!iw) {
    dbgprintf("iw test 1 failed\r\n");
    return 0;
  }

  outwIW(0x43, 0);
  outIW(0x44, 0);
  outwIW(0x51, 0x1234);
  if ((peekIW(0)!=0x34)||(peekIW(1)!=0x12)) {
    dbgprintf("%02x %02x %04x\r\n", peekIW(0), peekIW(1));
    dbgprintf("iw test 2 failed\r\n");
    iw=0;
  }

  pokeIW(0,v0);
  pokeIW(1,v1);

  if (!iw) {
    return 0;
  }

  if (!setenhmode(1)) {
    return 0;
  }

  outwIW(0x52,(inwIW(0x52)&0xFFF0)|0x0c);
  outIW(0x53,inIW(0x53)&0xFD);

  unsigned long realmem=findMem(0x1000000);

  dbgprintf("realmem = %d kb [%d][%d][%d][%d]\r\n", realmem / 1024, iwMem[0]/1024, iwMem[1]/1024, iwMem[2]/1024, iwMem[3]/1024);

  unsigned long memcfg=(iwMem[3]>>18);
  memcfg=(memcfg<<8)|(iwMem[2]>>18);
  memcfg=(memcfg<<8)|(iwMem[1]>>18);
  memcfg=(memcfg<<8)|(iwMem[0]>>18);
  char lmcfi;

  switch (memcfg)
  {
    case 0x00000001:
      lmcfi=0x0; break;
    case 0x00000101: case 0x00010101:
      lmcfi=0x1; break;
    case 0x01010101:
      lmcfi=0x2; break;
    case 0x00000401:
      lmcfi=0x3; break;
    case 0x00010401: case 0x00040401: case 0x01040401: case 0x04040401:
      lmcfi=0x4; break;
    case 0x00040101:
      lmcfi=0x5; break;
    case 0x01040101: case 0x04040101:
      lmcfi=0x6; break;
    case 0x00000004:
      lmcfi=0x7; break;
    case 0x00000104: case 0x00000404:
      lmcfi=0x8; break;
    case 0x00010404: case 0x00040404: case 0x01040404: case 0x04040404:
      lmcfi=0x9; break;
    case 0x00000010:
      lmcfi=0xA; break;
    case 0x00000110: case 0x00000410: case 0x00001010:
      lmcfi=0xB; break;
    default:
      lmcfi=0xC;
  }

  outwIW(0x52,(inwIW(0x52)&0xFFF0)|lmcfi);
  findMem(realmem);

  unsigned long maxsize=4096*1024;
  for (int b=1; b<4; b++)
  {
    if (iwMem[b]&&iwMem[b]<maxsize)
    {
      maxsize=iwMem[b];
      bufferbank=b;
    }
  }

  //dbgprintf("maxsize = %d [%d]\r\n", maxsize / 1024, bufferbank);

  if (iweffects)
    iwMem[bufferbank]-=FXBUFFERSIZE;

  bufferpos=iwMem[bufferbank]>>1;

  memsize=iwMem[0]+iwMem[1]+iwMem[2]+iwMem[3];

  //dbgprintf("memsize = %d kb\r\n", memsize / 1024);

  iwRev=inIW(0x5b)>>4;

  setenhmode(0);

#endif
  return 1;
}

void dofill(unsigned long iwpos, unsigned long maxlen, unsigned short port)
{
    dbgprintf("dofill %08x %d %04x", iwpos, maxlen, port);
  outp(iwPort+0x103, 0x44);
  outp(iwPort+0x105, (iwpos>>16));
  outp(iwPort+0x103, 0x43);
  outpw(iwPort+0x104, (iwpos & 0xffff));
  outp(iwPort+0x103, 0x51);
  unsigned long end = iwpos + maxlen;
  while (iwpos < end) {
      outpw(iwPort+0x104, 0);
      iwpos += 2;
  }
}
/*
#pragma aux dofill parm [ebx] [ecx] [edx] modify [eax] = \
  "pushf" \
  "cli" \
  "add dx,103h" \
  "mov al,44h" \
  "out dx,al" \
  "add dx,2" \
  "mov eax,ebx" \
  "shr eax,16" \
  "out dx,al" \
  "sub dx,2" \
  "mov al,43h" \
  "out dx,al" \
  "inc dx" \
  "shr ecx,1" \
  "adc ecx,0" \
  "mov ax,bx" \
  "out dx,ax" \
  "dec dx" \
  "mov al,51h" \
  "out dx,ax" \
  "inc dx" \
  "mov ax,0" \
"lp:" \
  "out dx,ax" \
  "loop lp" \
  "popf"
*/

static void FillIWMem(unsigned long pos, unsigned long len)
{
  unsigned char lmci=inIW(0x53);
  outIW(0x53,(lmci|0x01)&0x4D);
  dofill(pos,len,iwPort);
  outIW(0x53,lmci);
}


static void SetupReverb()
{
  FillIWMem((bufferbank<<22)+(bufferpos<<1),FXBUFFERSIZE);

  unsigned long actpos=((bufferbank&0x01)<<21)|bufferpos;
  for (int i=28; i<32; i++)
  {
    int rc=i-28;
    selvoc(i);

    setmode(0x0c);
    setvmode(7);

    outIW(0x10,bufferbank>>1);
    setpoint(actpos,0);
    setpoint(actpos,1);
    setpoint(actpos+0xfff,2);
    setpoint(actpos+rdelay[rc],3);

    setrelvoll(0,1);
    setrelvolr(0,1);
    setvol(0xfff);
    seteffvol(linvol[rfeedback[rc]]);
    seteffchan(rfxchans[rc]);

    setfreq(0x400);

    outIW(0x15,0x21);

    actpos+=0x2000;
  }
}


static void initiw(char enhmode,char chans)
{
  int i;

  if (chans>32) chans=32;
  if (forceeffects&&(chans>28)) chans=28;

  resetIW();

#if 0
    // temp codec setuo
    unsigned char b=0;
    unsigned short codar = 0x32c;
    {
        setenhmode(1);

        outIW(0x4c, 0x07);

        b = inp(codar+0);

        outp(codar+0, b | 0x0a);        // external control reg
        outp(codar+1, 0x02);            // global irq enable

        outp(codar+0, b | 0x0c);        // mode
        outp(codar+1, 0x6c);            // 3

        // right channel    = sound
        // left channel     = big noise

//        outp(codar+0, b | 0x08);        // playback format
//        outp(codar+1, 0x5b);            // pio, 1chn, record di, playback en

        outp(codar+0, b | 0x09);        // conf1
        outp(codar+1, 0xc5);            // pio, 1chn, record di, playback en

        //outp(codar+0, b | 0x10);        // conf2
        //outp(codar+1, 0x80);            // full scale voltage select

        outp(codar+0, b | 0x11);        // conf3
        outp(codar+1, 0x02);            // enable synth

        outp(codar+0, b | 0x00);        // adc source left
        outp(codar+1, 0x40);            // aux1
        outp(codar+0, b | 0x01);        // adc source right
        outp(codar+1, 0x40);            // aux1

        outp(codar+0, b | 0x02);        // aux1 / synth left
        outp(codar+1, 0x0f);            // 
        outp(codar+0, b | 0x03);        // aux1 / synth right
        outp(codar+1, 0x0f);            //

        outp(codar+0, b | 0x04);        // aux2 left
        outp(codar+1, 0x0f);            // 
        outp(codar+0, b | 0x05);        // aux2 right
        outp(codar+1, 0x0f);            //

        outp(codar+0, b | 0x06);        // dac left
        outp(codar+1, 0x80);            // 
        outp(codar+0, b | 0x07);        // dac right
        outp(codar+1, 0x80);            // 

        outp(codar+0, b | 0x12);        // line-in left
        outp(codar+1, 0x0f);            // 
        outp(codar+0, b | 0x13);        // line-in right
        outp(codar+1, 0x0f);            // 


        outp(codar+0, b | 0x16);        // mic-in left
        outp(codar+1, 0x80);            // 
        outp(codar+0, b | 0x17);        // mic-in right
        outp(codar+1, 0x80);            // 

        outp(codar+0, b | 0x19);        // line-out left
        outp(codar+1, 0x20);            // 
        outp(codar+0, b | 0x1b);        // line-out right
        outp(codar+1, 0x20);            // 


        outIW(0x59, 0x00);              // compatibility mode


        //return;
    }
    // temp
#endif

  setenhmode(enhmode);

  outIW(0x41, 0x00);
  outIW(0x45, 0x00);
  outIW(0x49, 0x00);

  outIW(0xE, 0xff);  // only for GUS compatibility

  inpIW(0x6);
  inIW(0x41);
  inIW(0x49);
  inIW(0x8F);

  for (i=0; i<32; i++)
  {
    selvoc(i);
    setvol(0);  // vol=0
    setmode(3);  // stop voice
    setvmode(3);  // stop volume
    setpoint(0,0);
    setpoint(1,0);
    outIW(0x06,63);
    outwIW(0x0C,0xfff0); // reset panning
    if (enhmode)
    {
      outIW(0x15,(i<chans)?0x20:0x02);   // fine panning vs. voice off
      setbank(0);
      outwIW(0x13,0xfff0);                // reset vol offsets
      outwIW(0x1b,0xfff0);
      outwIW(0x1c,0xfff0);
      outwIW(0x16,0);                     // reset effects depth
      outwIW(0x1d,0);
      outIW(0x14,0x00);                        // disable FX channels;
    }
    else
      outIW(0x15,0x00);
  }

  outIW(0x4C,0x07);             // synth irq enable, dac enable

  if (iweffects&&enhmode&&chans&&chans<29)
    SetupReverb();

  selvoc(0);
  outpIW(0x00,0x08);            // irqdma enable, mic off, lineout on, linein on

  //outpIW(0x00,0x0);
}


typedef struct
{
  unsigned char bank;
  unsigned long startpos;
  unsigned long endpos;
  unsigned long loopstart;
  unsigned long loopend;
  unsigned long sloopstart;
  unsigned long sloopend;
  unsigned long samprate;
  unsigned long curstart;
  unsigned long curend;
  unsigned char redlev;

  unsigned char curloop;
  int samptype;

  unsigned short cursamp;
  unsigned char mode;

  unsigned short volume;
  unsigned short voll;
  unsigned short volr;
  unsigned short reverb;
  unsigned char fxsend;

  unsigned char inited;
  signed char chstatus;
  signed short nextsample;
  signed long nextpos;
  unsigned char orgloop;
  signed char loopchange;
  signed char dirchange;

  unsigned long orgfreq;
  unsigned long orgdiv;
  unsigned short orgvol;
  signed short orgpan;
  unsigned char orgrev;
  unsigned char pause;
  unsigned char wasplaying;

  void *smpptr;
}  iwchan;

typedef struct
{
  unsigned char bank;
  signed long pos;
  unsigned long length;
  unsigned long loopstart;
  unsigned long loopend;
  unsigned long sloopstart;
  unsigned long sloopend;
  unsigned long samprate;
  int type;
  unsigned char redlev;
  void *ptr;
} iwsample;

static unsigned long mempos[4];
static iwsample samples[MAXSAMPLES];
static unsigned short samplenum;

static unsigned char channelnum;
static void (*playerproc)();
static iwchan channels[32];
static unsigned long gtimerlen;
static unsigned long gtimerpos;
static unsigned long gtoldlen;
static unsigned long cmdtimerpos;
static unsigned long stimerlen;
static unsigned long stimerpos;
static unsigned long stoldlen;
static unsigned short relspeed;
static unsigned long orgspeed;
static unsigned char mastervol;
static signed char masterpan;
static signed char masterbal;
static unsigned short masterfreq;
static signed short masterreverb;
static unsigned long amplify;

static unsigned char paused;

static void *dmaxfer;
static unsigned long dmaleft;
static unsigned long dmapos;
static unsigned char dma16bit;

static unsigned char filter;

static void fadevol(unsigned short v)
{
  unsigned short start=getvol();
  unsigned short end=v;
  unsigned char vmode;
  if (abs((short)(start-end))<64)
  {
    setvol(end);
    return;
  }

  if (start>end)
  {
    unsigned short t=start;
    start=end;
    end=t;
    vmode=0x40;
  }
  else
    vmode=0;
  if (start<64)
    start=64;
  if (end>4032)
    end=4032;
  setvst(start>>4);
  setvend(end>>4);
  setvmode(vmode);
}

static void fadevoldown()
{
  setvst(0x04);
  setvend(0xFC);
  setvmode(0x40);
}


static void processtick()
{
  int i;
  for (i=0; i<channelnum; i++)
  {
    iwchan *c=&channels[i];
    if (c->inited&&(c->chstatus||(c->nextpos!=-1)))
    {
      selvoc(i);
      setmode(c->mode|3);
      fadevoldown();
    }

    c->chstatus=0;
  }

/*
  for (i=0; i<channelnum; i++) {
        selvoc(i);
        while (!getvmode() & 1);
  }
*/
#if 1
  for (i=0; i<channelnum; i++)
  {
    selvoc(i);
    if (!getvmode() & 1) {
        iwchan *c=&channels[i];
        if (c->inited && c->chstatus) {
            while (!(getvmode()&1));
        }
    }
  }
#elif 0

//#if !FAKE_GUS    
#if 0
    while (!(getvmode()&1));
#else
    for (int j=0; j<1; j++) {

        if (getvmode() & 1)
            break;
    }
#endif
  }
#endif

  for (i=0; i<channelnum; i++)
  {
    iwchan *c=&channels[i];
    selvoc(i);
    if (c->inited)
    {
      if (c->nextsample!=-1)
      {
        iwsample *s=&samples[c->nextsample];
	unsigned char bit16=!!(s->type&mcpSamp16Bit);
        c->bank=s->bank;
	c->startpos=(s->pos+(s->bank<<22))>>bit16;
	c->endpos=c->startpos+s->length;
	c->loopstart=c->startpos+s->loopstart;
	c->loopend=c->startpos+s->loopend;
	c->sloopstart=c->startpos+s->sloopstart;
	c->sloopend=c->startpos+s->sloopend;
	c->samprate=s->samprate;
	c->samptype=s->type;
	c->redlev=s->redlev;
	c->smpptr=s->ptr;
	if (c->loopchange==-1)
	  c->loopchange=1;
	c->mode=(bit16)?0x07:0x03;
	c->cursamp=c->nextsample;
	setbank(c->bank>>bit16);
	setmode(c->mode|3);
        setrelvoll(c->voll,0);
        setrelvolr(c->volr,0);
      }

      if (c->nextpos!=-1)
	c->nextpos=c->startpos+(c->nextpos>>c->redlev);
      if ((c->loopchange==1)&&!(c->samptype&mcpSampSLoop))
	c->loopchange=2;
      if ((c->loopchange==2)&&!(c->samptype&mcpSampLoop))
	c->loopchange=0;
      if (c->loopchange==0)
      {
	c->curstart=c->startpos;
	c->curend=c->endpos;
	c->mode&=~0x18;
      }
      if (c->loopchange==1)
      {
	c->curstart=c->sloopstart;
	c->curend=c->sloopend;
	c->mode|=0x08;
	if (c->samptype&mcpSampSBiDi)
	  c->mode|=0x10;
      }
      if (c->loopchange==2)
      {
	c->curstart=c->loopstart;
	c->curend=c->loopend;
	c->mode|=0x08;
	if (c->samptype&mcpSampBiDi)
	  c->mode|=0x10;
      }

      int dir=getmode()&0x40;
      if (c->loopchange!=-1)
      {
        c->curloop=c->loopchange;
        setpoint(c->curstart, 1);
        setpoint(c->curend, 2);
      }
      if (c->dirchange!=-1)
        dir=(c->dirchange==2)?(dir^0x40):c->dirchange?0x40:0;
      int pos=-1;
      if ((c->loopchange!=-1)||(c->dirchange!=-1))
        pos=getpoint();
      if (c->nextpos!=-1)
        pos=c->nextpos;
      if (pos!=-1)
      {
        if (((pos<c->curstart)&&dir)||((pos>=c->curend)&&!dir))
          dir^=0x40;
        if (c->nextpos!=-1)
        {
          c->mode&=~3;
          if (pos>=c->endpos)
            pos=c->endpos;
          if (pos<=c->startpos)
            pos=c->startpos;
          setpoint(pos, 0);
        }
        setmode(c->mode|dir);
      }

      if (!(getmode()&1))
      {
	if (c->pause)
	  fadevoldown();
	else
	{
	  fadevol(linvol[c->volume]);
          setrelvoll(c->voll,0);
          setrelvolr(c->volr,0);
	  seteffvol(linvol[c->reverb]);
	  seteffchan(pan2chan[c->fxsend]);
	}
        setfreq(umuldivrnd(c->orgfreq, c->samprate*masterfreq, c->orgdiv)/11025);
      }
      else
	fadevoldown();
    }
    else
      fadevoldown();

    c->nextsample=-1;
    c->nextpos=-1;
    c->loopchange=-1;
    c->dirchange=-1;
  }
}


void doupload8(const void *buf, unsigned long iwpos, unsigned long maxlen, unsigned short port)
{
    //dbgprintf("doupload %08x, %08x, %d, %d\r\n", buf, iwpos, maxlen, port);
  outp(port+0x103, 0x44);
  outp(port+0x105, (iwpos>>16));
  outp(port+0x103, 0x43);
  outpw(port+0x104, (iwpos & 0xffff));

  outp(port+0x103, 0x51);
  unsigned short* ptr = (unsigned short*)buf;
  unsigned long end = iwpos + maxlen;
  while (iwpos < end) {
    unsigned short v = *ptr++;
    //swap16(&v);
    outpw(port+0x104, v);
    iwpos += 2;
  }
}
#define doupload16 doupload8

/*
#pragma aux doupload8 parm [esi] [ebx] [ecx] [edx] modify [eax] = \
  "pushf" \
  "cli" \
  "add dx,103h" \
  "mov al,44h" \
  "out dx,al" \
  "add dx,2" \
  "mov eax,ebx" \
  "shr eax,16" \
  "out dx,al" \
  "sub dx,2" \
  "mov al,43h" \
  "out dx,al" \
  "inc dx" \
  "shr ecx,1" \
  "adc ecx,0" \
  "mov ax,bx" \
  "out dx,ax" \
  "dec dx" \
  "mov al,51h" \
  "out dx,ax" \
  "inc dx" \
  "rep outsw" \
  "popf"
*/
/*
void doupload16(const void *buf, unsigned long iwpos, unsigned long maxlen, unsigned short port)
{
}
*/
/*
#pragma aux doupload16 parm [esi] [ebx] [ecx] [edx] modify [eax] = \
  "pushf" \
  "cli" \
  "add dx,103h" \
  "mov al,44h" \
  "out dx,al" \
  "add dx,2" \
  "mov eax,ebx" \
  "shr eax,16" \
  "out dx,al" \
  "sub dx,2" \
  "mov al,43h" \
  "out dx,al" \
  "inc dx" \
  "shr ecx,1" \
  "mov ax,bx" \
  "out dx,ax" \
  "dec dx" \
  "mov al,51h" \
  "out dx,ax" \
  "inc dx" \
  "rep outsw" \
  "popf"
*/

static void slowupload()
{
  unsigned char lmci=inIW(0x53);
  outIW(0x53,(lmci|0x01)&0x4D);

  if (!dma16bit)
  {
    if ((dmapos&1)&&dmaleft)
    {
      pokeIW(dmapos, *(char*)dmaxfer);
      dmaxfer=(char*)dmaxfer+1;
      dmapos++;
      dmaleft--;
    }
    if (dmaleft&1)
    {
      pokeIW(dmapos+dmaleft-1, ((char*)dmaxfer)[dmaleft-1]);
      dmaleft--;
    }
    doupload8(dmaxfer, dmapos, dmaleft, iwPort);
  }
  else
    doupload16(dmaxfer, dmapos, dmaleft, iwPort);

  outIW(0x53,lmci);

}

static void irqrout()
{
  while (1)
  {
    unsigned char source=inpIW(0x6);
    if (!source)
      break;
    if (source&0x03)
      inpIW(0x100);
    if (source&0x04)
    {
      if (!paused)
      {
	if ((gtimerpos>>8)<=256)
          gtimerpos=(gtimerpos&255)+gtimerlen;
	else
          gtimerpos-=256<<8;
        if (gtimerpos!=gtoldlen)
        {
          gtoldlen=gtimerpos;
          settimer(0x00);
          settimerlen(((gtimerpos>>8)<=256)?(256-(gtimerpos>>8)):0);
          settimer(0x04);
        }
	if (!((gtimerpos-gtimerlen)>>8))
        {
          processtick();
          playerproc();
          cmdtimerpos+=umuldiv(gtimerlen, 256*65536, 12615*3600);
          gtimerlen=umuldiv(256, 12615*256*256, orgspeed*relspeed);
        }
      }
      else
      {
	settimer(0x00);
	settimer(0x04);
      }
    }
    if (source&0x08)
    {
      settimer(0x00);
      settimer(0x04);
    }
  }
}

static void timerrout()
{
  if (paused)
    return;

#if 0
    unsigned short sr = _disableint();
    processtick();
    _restoreint(sr);

    playerproc();
#else
  if (stimerpos<=65536)
    stimerpos=stimerlen;
  else
    stimerpos-=65536;

  if (stimerpos!=stoldlen)
  {
    stoldlen=stimerpos;
    tmSetNewRate((stimerpos<=65536)?stimerpos:65536);
  }
  if (stimerpos==stimerlen)
  {
    unsigned short sr = _disableint();
    processtick();
    _restoreint(sr);
    playerproc();
    cmdtimerpos+=stimerlen;
    stimerlen=umuldiv(256, 1193046*256, orgspeed*relspeed);
  }
#endif
}

static void voidtimer()
{
}

static void calcfxvols()
{
  short vl,vr;
  if (channelnum<29)
    for (int i=28; i<32; i++)
    {
      vr=rpanning[i-28];
      vl=0x200-vr;
      if (masterbal)
      {
	    if (masterbal<0)
	        vr=(vr*(64+masterbal))>>6;
	    else
	        vl=(vl*(64-masterbal))>>6;
      }
      selvoc(i);
      setrelvoll(vl,1);
      setrelvolr(vr,1);
    }
}


static void calcvols(iwchan *c)
{
  short cv=(c->orgvol*mastervol*amplify)>>20;
  if (cv>=0x200) cv=0x1ff;

  short vr=(((c->orgpan*masterpan)>>6)+128)<<1;
  if (vr>=0x200) vr=0x1ff;
  short vl=0x1ff-vr;
  char  ch=vr>>8;

  short rv;
  if (masterreverb>0)
    rv=((masterreverb<<2)+((c->orgrev*(64-masterreverb))>>6));
  else
    rv=(c->orgrev*(masterreverb+64))>>6;
  if (rv>=0x200) rv=0x1ff;

  if (masterbal)
  {
    if (masterbal<0)
      vr=(vr*(64+masterbal))>>6;
    else
      vl=(vl*(64-masterbal))>>6;
  }

  c->volume=cv;
  c->voll=vl;
  c->volr=vr;
  c->reverb=rv;
  c->fxsend=ch;
}

static int LoadSamples(sampleinfo *sil, int n)
{
  unsigned long samplen[MAXSAMPLES];

    dbgprintf("LoadSamples %d\r\n", n);

  if (n>MAXSAMPLES) return 0;

  for (int sc=0; sc<n; sc++) {
    samplen[sc] = (sil[sc].type & mcpSamp16Bit) ? (sil[sc].length << 1) : sil[sc].length;
  }

  int largestsample=0;
  for (int sa=0; sa<n; sa++) {
    if (samplen[sa]>samplen[largestsample]) {
        largestsample=sa;
    }
  }

  if (!mcpReduceSamples(sil, n, memsize-samplen[largestsample], mcpRedToMono)) {
    dbgprintf("reduce %d fail\r\n", n);
    return 0;
  }

  samplenum=n;

  dbgprintf("load");
  mempos[0]=0;mempos[1]=0;mempos[2]=0;mempos[3]=0;
  while(1)
  {
    dbgprintf(".");
    largestbank=0;
    lbsize=iwMem[0]-mempos[0];
    for (unsigned char b=1; b<4; b++) {
        if ((iwMem[b]-mempos[b])>lbsize)
        {
        lbsize=iwMem[b]-mempos[b];
        largestbank=b;
        }
    }

    for (int sa=0; sa<n; sa++) {
        if (samplen[sa]>samplen[largestsample]) {
            largestsample=sa;
        }
    }

    if (!samplen[largestsample]) {
      dbgprintf("\r\n");
      return 1;
    }
    if (samplen[largestsample]>lbsize) {
      dbgprintf("\r\n");
      return 0;
    }

    sampleinfo *si=&sil[largestsample];
    iwsample *s=&samples[largestsample];
    s->pos=mempos[largestbank];
    s->length=si->length;
    s->loopstart=si->loopstart;
    s->loopend=si->loopend;
    s->sloopstart=si->sloopstart;
    s->sloopend=si->sloopend;
    s->samprate=si->samprate;
    s->type=si->type;
    s->redlev=(si->type&mcpSampRedRate4) ? 2 : (si->type&mcpSampRedRate2) ? 1 : 0;
    int bit16=(si->type&mcpSamp16Bit) ? 1 : 0;
    s->bank=largestbank;
    mempos[largestbank]+=(((s->length+2)<<bit16)+31)&~31UL;
#if 0
    dbgprintf("  bank  = %d\r\n", s->bank);
    dbgprintf("  pos   = %d\r\n", s->pos);
    dbgprintf("  len   = %d\r\n", s->length);
    dbgprintf("  16bit = %d\r\n", bit16);
#endif
    dma16bit=bit16;
    dmaleft=(s->length+2)<<dma16bit;
    dmaxfer=si->ptr;
    dmapos=s->pos|(s->bank<<22);
#if 0
    dbgprintf("    dmaxfer = %08x\r\n", dmaxfer);
    dbgprintf("    dmaleft = %06x\r\n", dmaleft);
    dbgprintf("    dmapos  = %06x\r\n", dmapos);
#endif
    short sr = _disableint();
    slowupload();
    _restoreint(sr);
    samplen[largestsample]=0;
    s->ptr=si->ptr;
  }
  dbgprintf("\r\n");
  return 1;
}


static void recalcvols()
{
  int i;
  for (i=0; i<channelnum; i++)
    calcvols(&channels[i]);
}


static void GetMixChannel(int ch, mixchannel *chn, int rate)
{
  chn->status=0;

  //unsigned short is=_disableint();
  selvoc(ch);
  unsigned long pos=getpoint()+(getbank()<<22);
  unsigned char mode=getmode();
  //_restoreint(is);
  iwchan *c=&channels[ch];

  if ((paused&&!c->wasplaying)||(!paused&&(mode&1))||!c->inited)
    return;

  if (c->pause)
    chn->status|=MIX_MUTE;

  unsigned int resvoll,resvolr;
  resvoll=c->volume*c->voll; resvolr=c->volume*c->volr;

  chn->vols[0]=resvoll*8/amplify;
  chn->vols[1]=resvolr*8/amplify;
  chn->status|=((mode&0x08)?MIX_LOOPED:0)|((mode&0x10)?MIX_PINGPONGLOOP:0)|((mode&0x04)?MIX_PLAY16BIT:0);
  if (c->orgdiv)
    chn->step=umuldivrnd(umuldivrnd(c->orgfreq, c->samprate*masterfreq, c->orgdiv), 256, rate);
  else
    chn->step=0;
  if (mode&0x40) chn->step=-chn->step;
  chn->samp=c->smpptr;
  chn->length=c->endpos-c->startpos;
  chn->loopstart=c->curstart-c->startpos;
  chn->loopend=c->curend-c->startpos;
  chn->fpos=0;
  chn->pos=pos-c->startpos;
  if (filter)
    chn->status|=MIX_INTERPOLATE;
  chn->status|=MIX_PLAYING;
}

static void Pause(int p)
{
  if (p==paused)
    return;
  int i;
  if (paused)
  {
    for (i=0; i<channelnum; i++)
      if (channels[i].wasplaying)
      {
	selvoc(i);
	setmode(channels[i].mode|(getmode()&0x40));
      }
    stimerpos=0;
    gtimerpos=0;
    if (useiwtimer)
      settimer(0x04);
    paused=0;
  }
  else
  {
    paused=1;
    if (useiwtimer)
      settimer(0x00);
    for (i=0; i<channelnum; i++)
    {
      selvoc(i);
      channels[i].wasplaying=!(getmode()&1);
      setmode(3|(getmode()&0x40));
    }
  }
}

static void SET(int ch, int opt, int val)
{
  switch (opt)
  {
  case mcpGSpeed:
    orgspeed=val;
    //dbgprintf("orgspeed = %d\r\n", val);
    break;
  case mcpCInstrument:
    channels[ch].chstatus=1;
    channels[ch].nextpos=-1;
    channels[ch].nextsample=val;
    channels[ch].loopchange=1;
    channels[ch].inited=1;
    break;
  case mcpCMute:
    channels[ch].pause=val;
    break;
  case mcpCStatus:
    if (!val)
    {
      channels[ch].nextpos=-1;
      channels[ch].chstatus=1;
    }
    break;
  case mcpCLoop:
    channels[ch].loopchange=((val>2)||(val<0))?-1:val;
    break;
  case mcpCDirect:
    channels[ch].dirchange=((val>2)||(val<0))?-1:val;
    break;
  case mcpCPosition:
    channels[ch].nextpos=val;
    break;
  case mcpCPitch:
    channels[ch].orgfreq=8363;
    channels[ch].orgdiv=mcpGetFreq8363(-val);
    if (!channels[ch].orgdiv)
      channels[ch].orgdiv=256;
    break;
  case mcpCPitchFix:
    channels[ch].orgfreq=val;
    channels[ch].orgdiv=0x10000;
    break;
  case mcpCPitch6848:
    channels[ch].orgfreq=6848;
    channels[ch].orgdiv=val;
    if (!channels[ch].orgdiv)
      channels[ch].orgdiv=256;
    break;
  case mcpCReset:
    {
    int reswasmute;
    reswasmute=channels[ch].pause;
    memset(channels+ch, 0, sizeof(iwchan));
    channels[ch].pause=reswasmute;
    }
    break;
  case mcpCVolume:
    channels[ch].orgvol=(val<0)?0:(val>0x100)?0x100:val;
    calcvols(&channels[ch]);
    break;
  case mcpCPanning:
    channels[ch].orgpan=(val>0x80)?0x80:(val<-0x80)?-0x80:val;
    calcvols(&channels[ch]);
    break;
  case mcpCReverb:
    channels[ch].orgrev=(val<0)?0:(val>0x100)?0x100:val;
    calcvols(&channels[ch]);
    break;
  case mcpMasterAmplify:
    amplify=val;
    recalcvols();
    if (channelnum)
      mixSetAmplify(amplify);
    break;
  case mcpMasterPause:
    Pause(val);
    break;
  case mcpMasterVolume:
    mastervol=val;
    recalcvols();
    break;
  case mcpMasterPanning:
    masterpan=val;
    recalcvols();
    break;
  case mcpMasterBalance:
    masterbal=val;
    recalcvols();
    if (channelnum)
      calcfxvols();
    break;
  case mcpMasterReverb:
    masterreverb=val;
    recalcvols();
    break;
  case mcpMasterSpeed:
    relspeed=(val<16)?16:val;
    //dbgprintf("relspeed = %d\r\n", relspeed);
    break;
  case mcpMasterPitch:
    masterfreq=val;
    break;
  case mcpMasterFilter:
    filter=val;
    break;
  }
}

static int GET(int ch, int opt)
{
  switch (opt)
  {
  case mcpCStatus:
    selvoc(ch);
    return !(getmode()&1)||(paused&&channels[ch].wasplaying);
  case mcpCMute:
    return !!channels[ch].pause;
  case mcpGTimer:
    if (!useiwtimer)
      return tmGetTimer();
    else
      return umulshr16(cmdtimerpos,3600);
  case mcpGCmdTimer:
    return umulshr16(cmdtimerpos, 3600);
  }
  return 0;
}


static int OpenPlayer(int chan, void (*proc)())
{
  if (chan>32) chan=32;
  if (forceeffects&&(chan>28)) chan=28;
  if (!mixInit(GetMixChannel, 1, chan, amplify))
    return 0;

  int iwchan = chan < 14 ? 14: chan;

  //orgspeed=35*256;
  orgspeed = 50*256;

  memset(channels, 0, sizeof(iwchan)*chan);
  playerproc=proc;
  initiw(1,iwchan);
  calcfxvols();
  channelnum=chan;

  selvoc(0);
  outpIW(0x00,0x09);

  cmdtimerpos=0;
  if (!useiwtimer)
  {
    stimerlen=umuldiv(256, 1193046*256, orgspeed*relspeed);
    stimerpos=stoldlen=stimerlen;
    tmInit(timerrout, (stimerpos<=65536)?stimerpos:65536, 8192);
  }
  else
  {
    irqInit(iwIRQ, irqrout, 1, 8192);
    gtimerlen=umuldiv(256, 12615*256*256, orgspeed*relspeed);
    gtimerpos=gtoldlen=gtimerlen;
    settimerlen(((gtimerpos>>8)<=256)?(256-(gtimerpos>>8)):0);
    settimer(0x04);
    tmInit(voidtimer, 65536, 256);
  }
  outpIW(8,0x04);
  outpIW(9,0x01);

  mcpNChan=chan;

  return 1;
}

static void ClosePlayer()
{
  mcpNChan=0;

  tmClose();
  if (useiwtimer)
    irqClose();

  initiw(1,0);
  channelnum=0;
  mixClose();
}


static int initu(const deviceinfo *c)
{
  useiwtimer=(c->irq!=-1)&&(c->opt&0x01);
  iweffects=(c->opt&0x02);
  forceeffects=(c->opt&0x04);

  int i;

  if (!testPort(c->port)) {
    return 0;
  }

  iwPort=c->port;
  iwIRQ=c->irq;

  channelnum=0;
  filter=0;

  initiw(1,0);

  relspeed=256;
  paused=0;

  mastervol=64;
  masterpan=64;
  masterbal=0;
  masterfreq=256;
  amplify=65536;

  linvol[0]=0;
  linvol[512]=0x0FFF;
  for (i=1; i<512; i++)
  {
    int k=i;
    int j;
    for (j=0x0600; k; j+=0x0100)
      k>>=1;
    linvol[i]=j|((i<<(8-((j-0x700)>>8)))&0xFF);
  }

  mcpLoadSamples=LoadSamples;
  mcpOpenPlayer=OpenPlayer;
  mcpClosePlayer=ClosePlayer;
  mcpSet=SET;
  mcpGet=GET;

  return 1;
}


static void closeu()
{
  mcpOpenPlayer=0;
  initiw(0,14);
}


static int detectu(deviceinfo *c)
{
  iwRev  = -1;
  iwIRQ  = -1;
  iwPort = 0x220;

  iweffects=(c->opt&0x02);

  dbgprintf("testport %04x...", iwPort);
  if (!testPort(iwPort)) {
    dbgprintf("fail\r\n");
    return 0;
  }
  dbgprintf("mem = %d %d %d %d\r\n", iwMem[0], iwMem[1], iwMem[2], iwMem[3]);

  c->dev=&mcpInterWave;
  c->port=iwPort;
  c->port2=-1;
  c->irq=(c->opt&0x01)?iwIRQ:-1;
  c->irq2=-1;
  c->dma=-1;
  c->dma2=-1;
  c->subtype=iwRev;
  c->chan=32;
  c->mem=memsize;
  return 1;
}


#include "devigen.h"
//#include "psetting.h"

static unsigned long iwGetOpt(const char *sec)
{
  unsigned long opt=0;
    /*
  if (cfGetProfileBool(sec, "iwiwtimer", 0, 0))
    opt|=0x01;
  if (cfGetProfileBool(sec, "iweffects", 1, 1))
    opt|=0x02;
  if (cfGetProfileBool(sec, "iwforceeffects", 0, 0))
    opt|=0x04;
  */
  opt |= 0x02;
  return opt;
}


sounddevice mcpInterWave={SS_WAVETABLE, "AMD InterWave", detectu, initu, closeu};
devaddstruct mcpIWAdd = {iwGetOpt, 0, 0, 0};
char *dllinfo = "driver _mcpInterWave; addprocs _mcpIWAdd";

