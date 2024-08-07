// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// GMDPlay loader for Composer 669 modules
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release

#include <string.h>
#include "binfile.h"
#include "mcp.h"
#include "gmdplay.h"
#include "err.h"

static inline void putcmd(unsigned char *&p, unsigned char c, unsigned char d)
{
  *p++=c;
  *p++=d;
}

extern "C" int mpLoad669(gmdmodule &m, binfile &file)
{
  mpReset(m);

  struct
  {
    unsigned short sig;
    char msg[108];
    unsigned char insnum;
    unsigned char patnum;
    unsigned char loop;
    unsigned char orders[0x80];
    unsigned char tempo[0x80];
    unsigned char patlen[0x80];
  } hdr;

  file.read(&hdr, sizeof(hdr));
  if ((hdr.sig!=*(unsigned short*)"if")&&(hdr.sig!=*(unsigned short*)"JN"))
    return errFormSig;

  memcpy(m.name, hdr.msg, 31);
  m.name[31]=0;

  int t;
  m.channum=8;
  m.instnum=hdr.insnum;
  m.sampnum=hdr.insnum;
  m.modsampnum=hdr.insnum;
  m.options=0;
  m.loopord=hdr.loop;

  m.patnum=0x80;
  for (t=0x7F; t>=0; t--)
  {
    if (hdr.orders[t]<hdr.patnum)
      break;
    m.patnum--;
  }
  if (!m.patnum)
    return errFormMiss;
  m.ordnum=m.patnum;
  m.endord=m.patnum;

  m.tracknum=m.patnum*9;

  m.message=new char *[4];
  char *msg=new char [111];
  if (!mpAllocInstruments(m, m.instnum)||!mpAllocTracks(m, m.tracknum)||!mpAllocPatterns(m, m.patnum)||!mpAllocSamples(m, m.sampnum)||!mpAllocModSamples(m, m.modsampnum)||!m.message||!msg||!mpAllocOrders(m, m.ordnum))
    return errAllocMem;

  m.message[0]=msg;
  m.message[1]=msg+37;
  m.message[2]=msg+74;
  m.message[3]=0;
  memcpy(m.message[0], hdr.msg, 36);
  m.message[0][36]=0;
  memcpy(m.message[1], hdr.msg+36, 36);
  m.message[1][36]=0;
  memcpy(m.message[2], hdr.msg+72, 36);
  m.message[2][36]=0;

  int i,j;
  for (i=0; i<m.ordnum; i++)
    m.orders[i]=i;

  gmdpattern *pp;
  for (pp=m.patterns, t=0; t<m.patnum; pp++, t++)
  {
    pp->patlen=hdr.patlen[hdr.orders[t]]+1;
    for (i=0; i<8; i++)
      pp->tracks[i]=t*9+i;
    pp->gtrack=t*9+8;
  }

  for (i=0; i<m.instnum; i++)
  {
    struct
    {
      char name[13];
      unsigned long length;
      unsigned long loopstart;
      unsigned long loopend;
    } sins;

    file.read(&sins, sizeof(sins));

    gmdinstrument &ip=m.instruments[i];
    gmdsample &sp=m.modsamples[i];
    sampleinfo &sip=m.samples[i];

    memcpy(ip.name, sins.name, 13);
    if (!sins.length)
      continue;

    for (j=0; j<128; j++)
      ip.samples[j]=i;

    *sp.name=0;
    sp.handle=i;
    sp.normnote=0;
    sp.stdvol=-1;
    sp.stdpan=-1;
    sp.opt=0;

    sip.length=sins.length;
    sip.loopstart=sins.loopstart;
    sip.loopend=sins.loopend;
    sip.samprate=8448; // ??
    sip.type=((sins.loopend<=sins.length)?mcpSampLoop:0)|mcpSampUnsigned;
  }

  unsigned char *buffer=new unsigned char[0x600*hdr.patnum];
  unsigned char *temptrack=new unsigned char [2000];
  if (!temptrack||!buffer)
    return errAllocMem;

  char chanused[8];
  memset(chanused, 0, 8);

  file.read(buffer, 0x600*hdr.patnum);

  unsigned char commands[8];
  unsigned char data[8];
  for (t=0; t<8; t++)
    commands[t]=0xFF;

  for (t=0; t<m.patnum; t++)
  {
    short j;
    for (j=0; j<8; j++)
    {
      unsigned char *bp=buffer+j*3+0x600*hdr.orders[t];
      unsigned char *tp=temptrack;

      short row;
      for (row=0; row<=hdr.patlen[hdr.orders[t]]; row++, bp+=24)
      {
        unsigned char *cp=tp+2;

        signed short ins=-1;
        signed short nte=-1;
        signed short pan=-1;
        signed short vol=-1;

        if (bp[0]<0xFE)
        {
          ins=((bp[0]<<4)|(bp[1]>>4))&0x3F;
          nte=(bp[0]>>2)+36;
          commands[j]=0xFF;
        }

        if (bp[0]!=0xFF)
          vol=(bp[1]&0xF)*0x11;

        if (bp[2]!=0xFF)
        {
          commands[j]=bp[2]>>4;
          data[j]=bp[2]&0xF;
          if (!data[j])
            commands[j]=0xFF;
          if (commands[j]==2)
          {
            ins=-1;
            nte|=128;
          }
        }

        if ((bp[0]!=0xFF)||(bp[2]!=0xFF))
          chanused[j]=1;

        if (!row&&!t)
          pan=(j&1)?0xFF:0;

        if ((bp[0]!=0xFF)||(pan!=-1))
        {
          unsigned char &act=*cp;
          *cp++=cmdPlayNote;
          if (ins!=-1)
          {
            act|=cmdPlayIns;
            *cp++=ins;
          }
          if (nte!=-1)
          {
            act|=cmdPlayNte;
            *cp++=nte;
          }
          if (vol!=-1)
          {
            act|=cmdPlayVol;
            *cp++=vol;
          }
          if (pan!=-1)
          {
            act|=cmdPlayPan;
            *cp++=pan;
          }
        }

        switch (commands[j])
        {
        case 0:
          putcmd(cp, cmdPitchSlideUp, data[j]);
          break;
        case 1:
          putcmd(cp, cmdPitchSlideDown, data[j]);
          break;
        case 2:
          putcmd(cp, cmdPitchSlideToNote, data[j]);
          break;
        case 3:
          putcmd(cp, cmdRowPitchSlideUp, data[j]<<2); // correct? down? both?
          break;
        case 4:
          putcmd(cp, cmdPitchVibrato, (data[j]<<4)|1);
          break;
        }

        if (cp!=(tp+2))
        {
          tp[0]=row;
          tp[1]=cp-tp-2;
          tp=cp;
        }
      }

      gmdtrack &trk=m.tracks[t*9+j];
      unsigned short len=tp-temptrack;

      if (!len)
        trk.ptr=trk.end=0;
      else
      {
        trk.ptr=new unsigned char[len];
        trk.end=trk.ptr+len;
        if (!trk.ptr)
          return errAllocMem;
        memcpy(trk.ptr, temptrack, len);
      }
    }

    unsigned char *tp=temptrack;
    unsigned char *bp=buffer+0x600*hdr.orders[t];

    unsigned char row;
    for (row=0; row<=hdr.patlen[hdr.orders[t]]; row++)
    {
      unsigned char *cp=tp+2;

      if (!row)
      {
        if (!t)
          putcmd(cp, cmdSpeed, 78);
        putcmd(cp, cmdTempo, hdr.tempo[hdr.orders[t]]);
      }

      unsigned char q;
      for (q=0; q<8; q++, bp+=3)
      {
        if ((bp[2]>>4)==5)
          if (bp[2]&0xF)
            putcmd(cp, cmdTempo, bp[2]&0xF);
      }

      if (cp!=(tp+2))
      {
        tp[0]=row;
        tp[1]=cp-tp-2;
        tp=cp;
      }
    }

    gmdtrack &trk=m.tracks[t*9+8];
    unsigned short len=tp-temptrack;

    if (!len)
      trk.ptr=trk.end=0;
    else
    {
      trk.ptr=new unsigned char[len+8];
      trk.end=trk.ptr+len;
      if (!trk.ptr)
        return errAllocMem;
      memcpy(trk.ptr, temptrack, len);
    }
  }
  delete buffer;
  delete temptrack;

  for (i=0; i<m.instnum; i++)
  {
    gmdinstrument &ip=m.instruments[i];
    gmdsample &sp=m.modsamples[i];
    sampleinfo &sip=m.samples[i];

    if (sp.handle==0xFFFF)
      continue;

    sip.ptr=new char [sip.length];
    if (!sip.ptr)
      return errAllocMem;
    file.read(sip.ptr, sip.length);
  }

//  for (i=m.channum-1; i>=0; i--)
//  {
//    if (chanused[i])
//      break;
//    m.channum--;
//  }
// if (!m.channum)
//    return MP_LOADFILE;

  return errOk;
}