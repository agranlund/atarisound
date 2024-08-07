// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// GMDPlay loader for MultiTracker modules
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

extern "C" int mpLoadMTM(gmdmodule &m, binfile &file)
{
  mpReset(m);

  struct
  {
    unsigned long sig;
    char name[20];
    unsigned short trknum;
    unsigned char patnum;
    unsigned char ordnum;
    unsigned short comlen;
    unsigned char insnum;
    unsigned char attr;
    unsigned char patlen;
    unsigned char channum;
    char pan[32];
  } header;

  file.read(&header, 66);

  if ((header.sig&0xFFFFFF)!=0x4D544D)
    return errFormSig;

  if ((header.sig&0xFF000000)!=0x10000000)
    return errFormOldVer;

  memcpy(m.name, header.name, 20);
  m.name[20]=0;

  m.options=0;
  m.channum=header.channum;
  m.modsampnum=m.sampnum=m.instnum=header.insnum;
  m.ordnum=header.ordnum+1;
  m.patnum=header.ordnum+1;
  m.endord=m.patnum;
  m.tracknum=(header.channum+1)*(header.patnum+1);
  m.loopord=0;

  if (!mpAllocInstruments(m, m.instnum)||!mpAllocPatterns(m, m.patnum)||!mpAllocTracks(m, m.tracknum)||!mpAllocSamples(m, m.sampnum)||!mpAllocModSamples(m, m.modsampnum)||!mpAllocOrders(m, m.ordnum))
    return errAllocMem;

  int i,t;
  for (i=0; i<m.ordnum; i++)
    m.orders[i]=i;

  for (i=0; i<m.instnum; i++)
  {
    struct
    {
      char name[22];
      unsigned long length;
      unsigned long loopstart;
      unsigned long loopend;
      signed char finetune;
      unsigned char volume;
      char attr; //1=16 bit
    } mi;
    file.read(&mi, sizeof(mi));

    if (mi.length<4)
      mi.length=0;
    if (mi.loopend<4)
      mi.loopend=0;
    if (mi.attr&1)
    {
      mi.length>>=1;
      mi.loopstart>>=1;
      mi.loopend>>=1;
    }
    if (mi.finetune&0x08)
      mi.finetune|=0xF0;

    gmdinstrument &ip=m.instruments[i];
    gmdsample &sp=m.modsamples[i];
    sampleinfo &sip=m.samples[i];

    memcpy(ip.name, mi.name, 22);
    ip.name[22]=0;
    if (!mi.length)
      continue;
    for (t=0; t<128; t++)
      ip.samples[t]=i;
    *sp.name=0;
    sp.handle=i;
    sp.normnote=-mi.finetune*32;
    sp.stdvol=(mi.volume>=0x3F)?0xFF:(mi.volume<<2);
    sp.stdpan=-1;
    sp.opt=0;

    sip.loopstart=mi.loopstart;
    sip.loopend=mi.loopend;
    sip.length=mi.length;
    sip.samprate=8363;
    sip.type=((mi.loopend)?mcpSampLoop:0)|((mi.attr&1)?mcpSamp16Bit:0)|mcpSampUnsigned;
  }

  unsigned char orders[128];

  file.read(orders, 128);

  gmdpattern *pp;
  for (pp=m.patterns, t=0; t<m.patnum; pp++, t++)
  {
    pp->patlen=header.patlen;
    for (i=0; i<m.channum; i++)
      pp->tracks[i]=orders[t]*(m.channum+1)+i;
    pp->gtrack=orders[t]*(m.channum+1)+m.channum;
  }

  unsigned long filetracks=file.tell();

  unsigned char *temptrack=new unsigned char[2000];
  unsigned char *tbuffer=new unsigned char[192*header.trknum+192];
  unsigned short (*trackseq)[32]=new unsigned short[header.patnum+1][32];
  if (!tbuffer||!temptrack||!trackseq)
    return errAllocMem;

  memset(tbuffer, 0, 192);
  file.read(tbuffer+192, 192*header.trknum);
  file.read(trackseq, 64*(header.patnum+1));

  for (t=0; t<=header.patnum; t++)
  {
    unsigned char *buffer[32];
    for (i=0; i<m.channum; i++)
    {
      buffer[i]=tbuffer+192*trackseq[t][i];

      unsigned char *tp=temptrack;
      unsigned char *buf=buffer[i];

      unsigned char row;
      for (row=0; row<64; row++, buf+=3)
      {
        unsigned char *cp=tp+2;

        signed short nte=(buf[0]>>2);
        signed short vol=-1;
        signed short pan=-1;
        signed short ins=(((buf[0]&0x03)<<4)|((buf[1]&0xF0)>>4))-1;
        unsigned char command=buf[1]&0xF;
        unsigned char data=buf[2];
        unsigned char pansrnd=0;

        if (command==0xE)
        {
          command=(data&0xF0)|0xE;
          data&=0xF;
        }

        if (!row&&(t==orders[0]))
          pan=(header.pan[i]&0xF)+((header.pan[i]&0xF)<<4);

        if (nte)
          nte+=36;
        else
          nte=-1;

        if (command==0xC)
          vol=(data>0x3F)?0xFF:(data<<2);

        if (command==0x8)
        {
          pan=data;
          if (pan==164)
            pan=0xC0;
          if (pan>0x80)
          {
            pan=0x100-pan;
            pansrnd=1;
          }
          pan=(pan==0x80)?0xFF:(pan<<1);
        }

        if (command==0x8E)
          pan=data*0x11;

        if (((command==0x3)||(command==0x5))&&(nte!=-1))
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
          if (command==0xDE)
          {
            act|=cmdPlayDelay;
            *cp++=data;
          }
        }

        if (pansrnd)
          putcmd(cp, cmdPanSurround, 0);

        switch (command)
        {
        case 0x0:
          if (data)
            putcmd(cp, cmdArpeggio, data);
          break;
        case 0x1:
          if (data)
            putcmd(cp, cmdPitchSlideUp, data);
          break;
        case 0x2:
          if (data)
            putcmd(cp, cmdPitchSlideDown, data);
          break;
        case 0x3:
          putcmd(cp, cmdPitchSlideToNote, data);
          break;
        case 0x4:
          putcmd(cp, cmdPitchVibrato, data);
          break;
        case 0x5:
          if ((data&0x0F)&&(data&0xF0))
            data=0;
          putcmd(cp, cmdPitchSlideToNote, 0);
          if (data&0xF0)
            putcmd(cp, cmdVolSlideUp, (data>>4)<<2);
          else
          if (data&0x0F)
            putcmd(cp, cmdVolSlideDown, (data&0xF)<<2);
          break;
        case 0x6:
          if ((data&0x0F)&&(data&0xF0))
            data=0;
          putcmd(cp, cmdPitchVibrato, 0);
          if (data&0xF0)
            putcmd(cp, cmdVolSlideUp, (data>>4)<<2);
          else
          if (data&0x0F)
            putcmd(cp, cmdVolSlideDown, (data&0xF)<<2);
          break;
        case 0x7:
          putcmd(cp, cmdVolVibrato, data);
          break;
        case 0x9:
          putcmd(cp, cmdOffset, data);
          break;
        case 0xA:
          if ((data&0x0F)&&(data&0xF0))
            data=0;
          if (data&0xF0)
            putcmd(cp, cmdVolSlideUp, (data>>4)<<2);
          else
          if (data&0x0F)
            putcmd(cp, cmdVolSlideDown, (data&0xF)<<2);
          break;
        case 0x1E:
          if (data)
            putcmd(cp, cmdRowPitchSlideUp, data<<4);
          break;
        case 0x2E:
          if (data)
            putcmd(cp, cmdRowPitchSlideDown, data<<4);
          break;
        case 0x3E:
          putcmd(cp, cmdSpecial, data?cmdGlissOn:cmdGlissOff);
          break;
        case 0x4E:
          if (data<4)
            putcmd(cp, cmdPitchVibratoSetWave, data);
          break;
        case 0x7E:
          if (data<4)
            putcmd(cp, cmdVolVibratoSetWave, data);
          break;
        case 0x9E:
          if (data)
            putcmd(cp, cmdRetrig, data);
          break;
        case 0xAE:
          if (data)
            putcmd(cp, cmdRowVolSlideUp, data<<2);
          break;
        case 0xBE:
          if (data)
            putcmd(cp, cmdRowVolSlideDown, data<<2);
          break;
        case 0xCE:
          putcmd(cp, cmdNoteCut, data);
          break;
        }

        if (cp!=(tp+2))
        {
          tp[0]=row;
          tp[1]=cp-tp-2;
          tp=cp;
        }
      }

      gmdtrack &trk=m.tracks[t*(m.channum+1)+i];
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
    unsigned char *buf;
    unsigned char row;
    for (row=0; row<64; row++)
    {
      unsigned char *cp=tp+2;
      for (i=0; i<m.channum; i++)
      {
        buf=buffer[i]+row*3;
        unsigned char command=buf[1]&0xF;
        unsigned char data=buf[2];
        if (command==0xE)
        {
          command=(data&0xF0)|0xE;
          data&=0xF;
        }
        switch (command)
        {
        case 0xB:
          putcmd(cp, cmdGoto, data);
          break;
        case 0xD:
          if (data>=0x64)
            data=0;
          putcmd(cp, cmdBreak, (data&0xF)+(data>>4)*10);
          break;
        case 0x6E:
          putcmd(cp, cmdSetChan, i);
          putcmd(cp, cmdPatLoop, data);
          break;
        case 0xEE:
          putcmd(cp, cmdPatDelay, data);
          break;
        case 0xF:
          if (data)
            if (data<0x20)
              putcmd(cp, cmdTempo, data);
            else
              putcmd(cp, cmdSpeed, data);
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
  delete tbuffer;
  delete trackseq;

  if (header.comlen&&!(header.comlen%40))
  {
    header.comlen/=40;
    m.message=new char *[header.comlen+1];
    if (!m.message)
      return errAllocMem;
    *m.message=new char [header.comlen*41];
    if (!*m.message)
      return errAllocMem;
    for (t=0; t<header.comlen; t++)
    {
      m.message[t]=m.message[0]+t*41;
      file.read(m.message[t], 40);
      short xxx;
      for (xxx=0; xxx<40; xxx++)
        if (!m.message[t][xxx])
          m.message[t][xxx]=' ';
      m.message[t][40]=0;
    }
    m.message[header.comlen]=0;
  }
  else
    file.seekcur(header.comlen);

  for (i=0; i<m.instnum; i++)
  {
    gmdinstrument &ip=m.instruments[i];
    gmdsample &sp=m.modsamples[i];
    sampleinfo &sip=m.samples[i];
    if (sp.handle==0xFFFF)
      continue;
    unsigned long l=sip.length<<(!!(sip.type&mcpSamp16Bit));
    sip.ptr=new char[l+16];
    if (!sip.ptr)
      return errAllocMem;
    file.read(sip.ptr, l);
  }

  return errOk;
}