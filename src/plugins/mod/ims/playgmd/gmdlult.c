// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// GMDPlay loader for UltraTracker modules
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release

// modified for c99 and Atari by agranlund 2024


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

extern "C" int mpLoadULT(gmdmodule &m, binfile &file)
{
  mpReset(m);

  char id[15];
  file.read(id, 15);
  if (memcmp(id, "MAS_UTrack_V00", 14))
    return errFormMiss;

  unsigned char ver=id[14]-'1';

  if (ver>3)
    return errFormOldVer;

  m.options=(ver<1)?MOD_GUSVOL:0;

  file.read(m.name, 32);
  m.name[31]=0;

  unsigned char msglen;
  msglen=file.getc();

  if (msglen)
  {
    m.message=new char *[msglen+1];
    if (!m.message)
      return errAllocMem;
    *m.message=new char [msglen*33];
    if (!*m.message)
      return errAllocMem;
    short t;
    for (t=0; t<msglen; t++)
    {
      m.message[t]=*m.message+t*33;
      file.read(m.message[t], 32);
      m.message[t][32]=0;
    }
    m.message[msglen]=0;
  }


  unsigned char insn;
  insn=file.getc();

  m.modsampnum=m.sampnum=m.instnum=insn;

  if (!mpAllocInstruments(m, m.instnum)||!mpAllocSamples(m, m.sampnum)||!mpAllocModSamples(m, m.modsampnum))
    return errAllocMem;

  unsigned long samplen=0;

  int i,j;
  for (i=0; i<m.instnum; i++)
  {
    struct
    {
      char name[32];
      char dosname[12];
      unsigned long loopstart;
      unsigned long loopend;
      unsigned long sizestart;
      unsigned long sizeend;
      unsigned char vol;
      unsigned char opt;
      unsigned short c2spd;
      unsigned short finetune;
    } mi;
    file.read(&mi, sizeof(mi)-((ver<3)?2:0));
    if (ver<3)
    {
      mi.finetune=mi.c2spd;
      mi.c2spd=8363;
    }
    unsigned long length=mi.sizeend-mi.sizestart;
    if (mi.opt&4)
    {
      mi.loopstart>>=1;
      mi.loopend>>=1;
    }
    if (mi.loopstart>length)
      mi.opt&=~8;
    if (mi.loopend>length)
      mi.loopend=length;
    if (mi.loopstart==mi.loopend)
      mi.opt&=~8;

    gmdinstrument &ip=m.instruments[i];
    gmdsample &sp=m.modsamples[i];

    memcpy(ip.name, mi.name, 31);
    ip.name[31]=0;
    if (!length)
      continue;
    for (j=0; j<128; j++)
      ip.samples[j]=i;

    memcpy(sp.name, mi.dosname, 12);
    sp.name[12]=0;

    sp.handle=i;
    sp.normnote=-mcpGetNote8363(mi.c2spd);
    sp.stdvol=mi.vol;
    sp.stdpan=-1;
    sp.opt=(mi.opt&4)?MP_OFFSETDIV2:0;

    sampleinfo &sip=m.samples[i];
    sip.length=length;
    sip.loopstart=mi.loopstart;
    sip.loopend=mi.loopend;
    sip.samprate=8363;
    sip.type=((mi.opt&8)?mcpSampLoop:0)|((mi.opt&16)?mcpSampBiDi:0)|((mi.opt&4)?mcpSamp16Bit:0);

    samplen+=((mi.opt&4)?2:1)*sip.length;
  }

  unsigned char orders[256];
  file.read(orders, 256);

  unsigned char chnn;
  unsigned char patn;

  chnn=file.getc();
  patn=file.getc();

  m.channum=chnn+1;

  unsigned char panpos[32];

  if (ver>=2)
    file.read(panpos, m.channum);
  else
    memcpy(panpos, "\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF\x0\xF", 32);

  m.loopord=0;

  short ordn;
  for (ordn=0; ordn<256; ordn++)
    if (orders[ordn]>patn)
      break;

  m.patnum=ordn;
  m.ordnum=ordn;
  m.endord=m.patnum;
  m.tracknum=(patn+1)*(m.channum+1);

  if (!mpAllocPatterns(m, m.patnum)||!mpAllocTracks(m, m.tracknum)||!mpAllocOrders(m, m.ordnum))
    return errAllocMem;

  for (i=0; i<m.ordnum; i++)
    m.orders[i]=i;

  gmdpattern *pp;
  int t;
  for (pp=m.patterns, t=0; t<m.patnum; pp++, t++)
  {
    pp->patlen=64;
    for (i=0; i<m.channum; i++)
      pp->tracks[i]=orders[t]*(m.channum+1)+i;
    pp->gtrack=orders[t]*(m.channum+1)+m.channum;
  }

  unsigned long patlength=file.length()-file.tell()-samplen;

  unsigned char *temptrack=new unsigned char[2000];
  unsigned char *buffer=new unsigned char[patlength];
  if (!buffer||!temptrack)
    return errAllocMem;

  file.read(buffer, patlength);

  unsigned char *bp=buffer;

  unsigned char *chbp[32];

  int q;
  for (q=0; q<m.channum; q++)
  {
    chbp[q]=bp;

    for (t=0; t<=patn; t++)
    {
      unsigned char *curcmd;
      unsigned char repn=0;

      unsigned char *tp=temptrack;

      unsigned char row;
      for (row=0; row<64; row++)
      {
        if (!repn)
        {
          if (*bp==0xFC)
          {
            repn=bp[1]-1;
            curcmd=bp+2;
            bp+=7;
          }
          else
          {
            curcmd=bp;
            bp+=5;
          }
        }
        else
          repn--;

        unsigned char *cp=tp+2;

        signed short ins=(signed short)curcmd[1]-1;
        signed short nte=curcmd[0]?(curcmd[0]+35):-1;
        signed short pan=-1;
        signed short vol=-1;
        unsigned char pansrnd=0;
        unsigned char command[2];
        unsigned char data[2];
        command[0]=curcmd[2]>>4;
        command[1]=curcmd[2]&0xF;
        data[0]=curcmd[4];
        data[1]=curcmd[3];

        if (command[0]==0xE)
        {
          command[0]=(data[0]&0xF0)|0xE;
          data[0]&=0xF;
        }
        if (command[1]==0xE)
        {
          command[1]=(data[1]&0xF0)|0xE;
          data[1]&=0xF;
        }
        if (command[0]==0x5)
        {
          command[0]=(data[0]&0xF0)|0x5;
          data[0]&=0xF;
        }
        if (command[1]==0x5)
        {
          command[1]=(data[1]&0xF0)|0x5;
          data[1]&=0xF;
        }

        if (!row&&(t==orders[0]))
          pan=panpos[q]*0x11;

        if (command[0]==0xC)
          vol=data[0];
        if (command[1]==0xC)
          vol=data[1];

        if (command[0]==0xB)
          pan=data[0]*0x11;
        if (command[1]==0xB)
          pan=data[1]*0x11;

        if (((command[0]==0x3)||(command[1]==0x3))&&(nte!=-1))
          nte|=128;

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
          if (command[1]==0xDE)
          {
            act|=cmdPlayDelay;
            *cp++=data[1];
          }
          else if (command[0]==0xDE)
          {
            act|=cmdPlayDelay;
            *cp++=data[0];
          }
        }
        short f;
        for (f=0; f<2; f++)
        {
        switch (command[f])
        {
        case 0x1:
          if (data[f])
            putcmd(cp, cmdPitchSlideUp, data[f]);
          break;
        case 0x2:
          if (data[f])
            putcmd(cp, cmdPitchSlideDown, data[f]);
          break;
        case 0x3:
          putcmd(cp, cmdPitchSlideToNote, data[f]);
          break;
        case 0x4:
          putcmd(cp, cmdPitchVibrato, data[f]);
          break;
        case 0xA:
          if ((data[f]&0x0F)&&(data[f]&0xF0))
            data[f]=0;
          if (data[f]&0xF0)
            putcmd(cp, cmdVolSlideUp, data[f]>>4);
          else
          if (data[f]&0x0F)
            putcmd(cp, cmdVolSlideDown, data[f]&0xF);
          break;
        case 0x1E:
          if (data[f])
            putcmd(cp, cmdRowPitchSlideUp, data[f]<<4);
          break;
        case 0x2E:
          if (data[f])
            putcmd(cp, cmdRowPitchSlideDown, data[f]<<4);
          break;
        case 0x9E:
          if (data[f])
            putcmd(cp, cmdRetrig, data[f]);
          break;
        case 0xAE:
          if (data[f])
            putcmd(cp, cmdRowVolSlideUp, data[f]);
          break;
        case 0xBE:
          if (data[f])
            putcmd(cp, cmdRowVolSlideDown, data[f]);
          break;
        case 0xCE:
          putcmd(cp, cmdNoteCut, data[f]);
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
  }

  unsigned char *chcurcmd[32];
  unsigned char chrepn[32];

  for (t=0; t<=patn; t++)
  {
    unsigned char *tp=temptrack;

    for (q=0; q<m.channum; q++)
      chrepn[q]=0;
    unsigned char row;
    for (row=0; row<64; row++)
    {
      unsigned char *cp=tp+2;

      for (q=0; q<m.channum; q++)
      {
        if (!chrepn[q])
          if (*chbp[q]==0xFC)
          {
            chrepn[q]=chbp[q][1]-1;
            chcurcmd[q]=chbp[q]+2;
            chbp[q]+=7;
          }
          else
          {
            chcurcmd[q]=chbp[q];
            chbp[q]+=5;
          }
        else
          chrepn[q]--;

        unsigned char command[2];
        unsigned char data[2];
        command[0]=chcurcmd[q][2]>>4;
        command[1]=chcurcmd[q][2]&0xF;
        data[0]=chcurcmd[q][4];
        data[1]=chcurcmd[q][3];

        unsigned char f;
        for (f=0; f<2; f++)
        {
        switch (command[f])
        {
        case 0xD:
          if (data[f]>=0x64)
            data[f]=0;
          putcmd(cp, cmdBreak, (data[f]&0xF)+(data[f]>>4)*10);
          break;
        case 0xF:
          if (data[f])
            if (data[f]<=0x20)
              putcmd(cp, cmdTempo, data[f]);
            else
              putcmd(cp, cmdSpeed, data[f]);
          break;
        }
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
    unsigned long l=sip.length<<(!!(sip.type&mcpSamp16Bit));

    sip.ptr=new unsigned char[l+16];
    if (!sip.ptr)
      return errAllocMem;
    file.read(sip.ptr, l);
  }

  return errOk;
}