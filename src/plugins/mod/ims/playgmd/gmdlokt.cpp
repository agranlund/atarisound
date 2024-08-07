// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// GMDPlay loader for Oktalyzer modules
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

static inline unsigned short swapw(unsigned short a)
{
  return ((a&0xFF)<<8)|((a&0xFF00)>>8);
}

static inline unsigned long swapl(unsigned long a)
{
  return ((a&0xFF)<<24)|((a&0xFF00)<<8)|((a&0xFF0000)>>8)|((a&0xFF000000)>>24);
}

extern "C" int mpLoadOKT(gmdmodule &m, binfile &file)
{
  mpReset(m);

  unsigned char sig[8];

  file.read(sig, 8);
  if (memcmp(sig, "OKTASONG", 8))
    return errFormSig;

  *m.name=0;
  m.message=0;

  m.options=MOD_TICK0;
  unsigned long blen;

  file.read(sig, 4);
  if (memcmp(sig, "CMOD", 4))
    return errFormStruc;
  blen=swapl(file.getl());
  if (blen!=8)
    return errFormStruc;

  unsigned short cflags[4];
  unsigned char cflag2[8];
  file.read(cflags, 8);
  int i,t;
  t=0;
  for (i=0; i<4; i++)
  {
    cflag2[t]=(swapw(cflags[i])&1)|((i+i+i)&2);
    if (cflag2[t++]&1)
      cflag2[t++]=(swapw(cflags[i])&1)|((i+i+i)&2);
  }
  m.channum=t;

  file.read(sig, 4);
  if (memcmp(sig, "SAMP", 4))
    return errFormStruc;
  blen=swapl(file.getl());
  if (blen&31)
    return errFormStruc;
  blen>>=5;

  m.modsampnum=m.sampnum=m.instnum=blen;

  if (!mpAllocInstruments(m, m.instnum)||!mpAllocSamples(m, m.sampnum)||!mpAllocModSamples(m, m.modsampnum))
    return errAllocMem;

  for (i=0; i<m.instnum; i++)
  {
    struct
    {
      char name[20];
      unsigned long length;
      unsigned short repstart;
      unsigned short replen;
      char pad1;
      unsigned char vol;
      short pad2;
    } mi;
    file.read(&mi, sizeof(mi));
    unsigned long length=swapl(mi.length);
    unsigned long loopstart=swapw(mi.repstart);
    unsigned long looplength=swapw(mi.replen);
    if (length<4)
      length=0;
    if (looplength<4)
      looplength=0;
    if (!looplength||(loopstart>=length))
      looplength=0;
    else
      if ((loopstart+looplength)>length)
        looplength=length-loopstart;

    gmdinstrument &ip=m.instruments[i];
    gmdsample &sp=m.modsamples[i];
    sampleinfo &sip=m.samples[i];

    memcpy(ip.name, mi.name, 20);
    ip.name[20]=0;
    if (!length)
      continue;

    for (t=0; t<128; t++)
      ip.samples[t]=i;

    *ip.name=0;
    sp.handle=i;
    sp.normnote=0;
    sp.stdvol=(mi.vol>0x3F)?0xFF:(mi.vol<<2);
    sp.stdpan=-1;
    sp.opt=0;

    sip.length=length;
    sip.loopstart=loopstart;
    sip.loopend=loopstart+looplength;
    sip.samprate=8363;
    sip.type=looplength?mcpSampLoop:0;
  }

  file.read(sig, 4);
  if (memcmp(sig, "SPEE", 4))
    return errFormStruc;
  blen=swapl(file.getl());
  if (blen!=2)
    return errFormStruc;
  unsigned short orgticks;
  orgticks=swapw(file.gets());

  file.read(sig, 4);
  if (memcmp(sig, "SLEN", 4))
    return errFormStruc;
  blen=swapl(file.getl());
  if (blen!=2)
    return errFormStruc;
  unsigned short pn;
  pn=swapw(file.gets());

  file.read(sig, 4);
  if (memcmp(sig, "PLEN", 4))
    return errFormStruc;
  blen=swapl(file.getl());
  if (blen!=2)
    return errFormStruc;
  unsigned short ordn;
  ordn=swapw(file.gets());

  file.read(sig, 4);
  if (memcmp(sig, "PATT", 4))
    return errFormStruc;
  blen=swapl(file.getl());
  if (blen>128)
    return errFormStruc;
  unsigned char orders[128];
  file.read(orders, blen);
  if (blen<ordn)
    ordn=blen;
  m.loopord=0;

  m.patnum=ordn;
  m.ordnum=ordn;
  m.endord=m.patnum;
  m.tracknum=pn*(m.channum+1);

  if (!mpAllocPatterns(m, m.patnum)||!mpAllocTracks(m, m.tracknum)||!mpAllocOrders(m, m.ordnum))
    return errAllocMem;

  for (i=0; i<m.ordnum; i++)
    m.orders[i]=i;

  gmdpattern *pp;
  for (pp=m.patterns, t=0; t<m.patnum; pp++, t++)
  {
    for (i=0; i<m.channum; i++)
      pp->tracks[i]=orders[t]*(m.channum+1)+i;
    pp->gtrack=orders[t]*(m.channum+1)+m.channum;
  }

  unsigned char *temptrack=new unsigned char[3000];
  unsigned char *buffer=new unsigned char[1024*m.channum];
  if (!buffer||!temptrack)
    return errAllocMem;

  for (t=0; t<pn; t++)
  {
    file.read(sig, 4);
    if (memcmp(sig, "PBOD", 4))
      return errFormStruc;
    blen=swapl(file.getl());
    unsigned short patlen;
    patlen=swapw(file.gets());
    if ((blen!=(2+4*m.channum*patlen))||(patlen>256))
      return errFormStruc;

    short q;
    for (q=0; q<m.patnum; q++)
      if (t==orders[q])
        m.patterns[q].patlen=patlen;

    file.read(buffer, 4*m.channum*patlen);
    for (q=0; q<m.channum; q++)
    {
      unsigned char *tp=temptrack;
      unsigned char *buf=buffer+4*q;

      unsigned char row;
      for (row=0; row<patlen; row++, buf+=m.channum*4)
      {
        unsigned char *cp=tp+2;

        unsigned char command=buf[2];
        unsigned char data=buf[3];
        signed short nte=buf[0]?(buf[0]+60-17+4):-1;
        signed short ins=buf[0]?buf[1]:-1;
        signed short pan=-1;
        signed short vol=-1;

        if (!row&&(t==orders[0]))
          pan=(cflag2[q]&2)?0xFF:0x00;

        if ((command==31)&&(data<=0x40))
        {
          vol=(data>0x3F)?0xFF:(data<<2);
          command=0;
        }
        if ((ins!=-1)||(nte!=-1)||(vol!=-1)||(pan!=-1))
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
        switch (command)
        {
        case 13: // note down
        case 17: // note up
        case 21: // note -
        case 30: // note +
          break;
        case 10: // LNH
        case 11: // NHNL
        case 12: // HHNLL
          break;
        case 31:
          if (data<=0x50)
            putcmd(cp, cmdVolSlideDown, (data&0xF)<<2);
          else
          if (data<=0x60)
            putcmd(cp, cmdVolSlideUp, (data&0xF)<<2);
          else
          if (data<=0x70)
            putcmd(cp, cmdRowVolSlideDown, (data&0xF)<<2);
          else
          if (data<=0x80)
            putcmd(cp, cmdRowVolSlideUp, (data&0xF)<<2);
          break;
        case 27: //release!!!
          putcmd(cp, cmdSetLoop, 0);
          break;
        case 0x1:
          putcmd(cp, cmdPitchSlideDown, data);
          break;
        case 0x2:
          putcmd(cp, cmdPitchSlideUp, data);
          break;
        }

        if (cp!=(tp+2))
        {
          tp[0]=row;
          tp[1]=cp-tp-2;
          tp=cp;
        }
      }
      gmdtrack &trk=m.tracks[t*(m.channum+1)+q];
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
    unsigned char *buf=buffer;
    unsigned char row;
    for (row=0; row<patlen; row++)
    {
      unsigned char *cp=tp+2;

      if (!row&&(t==orders[0]))
        putcmd(cp, cmdTempo, orgticks);

      for (q=0; q<m.channum; q++, buf+=4)
      {
        unsigned char command=buf[2];
        unsigned char data=buf[3];

        switch (command)
        {
        case 25:
          putcmd(cp, cmdGoto, data);
          break;
        case 28:
          if (data)
            putcmd(cp, cmdTempo, data);
          break;
        }
      }

      if (cp!=(tp+2))
      {
        tp[0]=row;
        tp[1]=cp-tp-2;
        tp=cp;
      }
    }

    gmdtrack &trk=m.tracks[t*(m.channum+1)+m.channum];
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
  delete temptrack;
  delete buffer;

  for (i=0; i<m.instnum; i++)
  {
    gmdinstrument &ip=m.instruments[i];
    gmdsample &sp=m.modsamples[i];
    sampleinfo &sip=m.samples[i];
    if (sp.handle==0xFFFF)
      continue;

    file.read(sig, 4);
    if (memcmp(sig, "SBOD", 4))
      return errFormStruc;
    blen=swapl(file.getl());

    sip.ptr=new char [blen+8];
    if (!sip.ptr)
      return errAllocMem;
    file.read(sip.ptr, blen);
    if (sip.length>blen)
      sip.length=blen;
    if (sip.loopend>blen)
      sip.loopend=blen;
    if (sip.loopstart>=sip.loopend)
      sip.type&=~mcpSampLoop;
  }

  return errOk;
}