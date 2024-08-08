// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// GMDPlay loader for ScreamTracker ]I[ modules
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release
//  -kb980717 Tammo Hinrichs <kb@nwn.de>
//    -re-enabled and fixed pattern reordering to finally get rid
//     of the mysterious SCB (Skaven Crash Bug)  ;)

// modified for c99 and Atari by agranlund 2024


#include <string.h>
#include "binfile.h"
#include "mcp.h"
#include "gmdplay.h"
#include "err.h"

static inline void putcmd(unsigned char **pp, unsigned char c, unsigned char d)
{
  unsigned char* p = *pp;
  *p++=c;
  *p++=d;
}

int mpLoadS3M(gmdmodule *m, binfile *file)
{
  mpReset(m);

  struct
  {
    char name[28];
    unsigned char end,type;
    unsigned short d1;
    short orders,ins,pats,flags,cwt,ffv;
    char magic[4];
    unsigned char mv,it,is,mm,uc,dp;
    unsigned long d2;
    unsigned long d3;
    unsigned short special;
    unsigned char channels[32];
  } hdr;

  bf_read(file, &hdr, sizeof(hdr));
    ims_swap16((unsigned short*)&hdr.d1);
    ims_swap16((unsigned short*)&hdr.orders);
    ims_swap16((unsigned short*)&hdr.ins);
    ims_swap16((unsigned short*)&hdr.pats);
    ims_swap16((unsigned short*)&hdr.flags);
    ims_swap16((unsigned short*)&hdr.cwt);
    ims_swap16((unsigned short*)&hdr.ffv);
    ims_swap32((unsigned long*)&hdr.d2);
    ims_swap32((unsigned long*)&hdr.d3);
    ims_swap16((unsigned short*)&hdr.special);

  if (memcmp(hdr.magic, "SCRM", 4))
    return errFormSig;

  memcpy(m->name, hdr.name, 28);
  m->name[28]=0;

  int t,i;
  m->channum=0;
  for (t=0; t<32; t++)
    if (hdr.channels[t]!=0xFF)
      m->channum=t+1;
  m->modsampnum=m->sampnum=m->instnum=hdr.ins;
  m->patnum=hdr.orders;
  m->tracknum=hdr.pats*(m->channum+1)+1;
  m->options=MOD_S3M|((((hdr.cwt&0xFFF)<=0x300)||(hdr.flags&64))?MOD_S3M30:0);
  m->loopord=0;

  for (t=0; t<m->channum; t++)
    if (((hdr.channels[t]&8)>>2)^((t+t+t)&2))
      break;
  if (t==m->channum)
    m->options|=MOD_MODPAN;

  unsigned char orders[256];
  unsigned short inspara[256];
  unsigned short patpara[256];
  unsigned long smppara[256];
  unsigned char defpan[32];

  bf_read(file, &orders, m->patnum);
  for (t=m->patnum-1; t>=0; t--)
  {
    if (orders[t]<254)
      break;
    m->patnum--;
  }
  if (!m->patnum)
    return errFormMiss;

  short t2=0;
  for (t=0; t<m->patnum; t++)
  {
    orders[t2]=orders[t];
    if (orders[t]!=254)
      t2++;
  }
  short oldpatnum=m->patnum;
  m->patnum=t2;

  m->ordnum=m->patnum;

  for (t=0; t<m->patnum; t++)
    if (orders[t]==255)
      break;
  m->endord=t;

  bf_read(file, &inspara, m->instnum*2);
  for (int i=0; i<m->instnum; i++) {
    ims_swap16(&inspara[i]);
  }

  bf_read(file, &patpara, hdr.pats*2);
  for (int i=0; i<hdr.pats; i++) {
    ims_swap16(&patpara[i]);
  }

//  hdr.mm|=0x80;
  for (i=0; i<32; i++)
    defpan[i]=(hdr.mm&0x80)?((hdr.channels[i]&8)?0x2F:0x20):0;
  if (hdr.dp==0xFC)
    bf_read(file, &defpan, 32);
  for (i=0; i<32; i++)
    defpan[i]=(defpan[i]&0x20)?((defpan[i]&0xF)*0x11):((hdr.mm&0x80)?((hdr.channels[i]&8)?0xCC:0x33):0x80);

  if (!mpAllocInstruments(m, m->instnum)||!mpAllocTracks(m, m->tracknum)||!mpAllocPatterns(m, m->patnum)||!mpAllocSamples(m, m->sampnum)||!mpAllocModSamples(m, m->modsampnum)||!mpAllocOrders(m, m->ordnum))
    return errAllocMem;

  for (i=0; i<m->ordnum; i++)
    m->orders[i]=(orders[i]==254)?0xFFFF:i;

  gmdpattern *pp;
  for (pp=m->patterns, t=0; t<m->patnum; pp++, t++)
  {
    if (orders[t]==254)
      continue;
    pp->patlen=64;
    if ((orders[t]!=255)&&(orders[t]<hdr.pats))
    {
      for (i=0; i<m->channum; i++)
        pp->tracks[i]=orders[t]*(m->channum+1)+i;
      pp->gtrack=orders[t]*(m->channum+1)+m->channum;
    }
    else
    {
      for (i=0; i<m->channum; i++)
        pp->tracks[i]=m->tracknum-1;
      pp->gtrack=m->tracknum-1;
    }
  }

  for (i=0; i<m->instnum; i++)
  {
    struct
    {
      unsigned char type;
      char dosname[12];
      unsigned char sampptrh;
      unsigned short sampptr;
      unsigned long length;
      unsigned long loopstart;
      unsigned long loopend;
      unsigned char volume;
      char d1;
      unsigned char pack;
      unsigned char flag;
      unsigned long c2spd;
      char d2[12];
      char name[28];
      long magic;
    } sins;

    //file.seek((long)inspara[i]*16);
    //file.read(&sins, sizeof(sins));
    bf_seek(file, (long)inspara[i]*16);
    bf_read(file, &sins, sizeof(sins));
    ims_swap16((unsigned short*)&sins.sampptr);
    ims_swap32((unsigned long*)&sins.length);
    ims_swap32((unsigned long*)&sins.loopstart);
    ims_swap32((unsigned long*)&sins.loopend);
    ims_swap32((unsigned long*)&sins.volume);
    ims_swap32((unsigned long*)&sins.c2spd);
    ims_swap32((unsigned long*)&sins.magic);

    if ((sins.magic!=0x53524353)&&(sins.magic!=0))
      return errFormStruc;
    smppara[i]=sins.sampptr+(sins.sampptrh<<16);

    if (sins.flag&4)
    {
      sins.length>>=1;
      sins.loopstart>>=1;
      sins.loopend>>=1;
    }

    gmdinstrument *ip=&(m->instruments[i]);
    gmdsample *sp=&(m->modsamples[i]);
    sampleinfo *sip=&(m->samples[i]);

    memcpy(ip->name, sins.name, 28);
    ip->name[28]=0;
    if (!sins.length)
      continue;
    if (sins.type!=1)
      continue;
    if (sins.pack)
      continue;
    if (sins.flag&2)
      continue;

    int j;
    sp->handle=i;
    for (j=0; j<128; j++)
      ip->samples[j]=i;
    memcpy(sp->name, sins.dosname, 12);
    sp->name[12]=0;
    sp->normnote=-mcpGetNote8363(sins.c2spd);
    sp->stdvol=(sins.volume>0x3F)?0xFF:(sins.volume<<2);
    sp->stdpan=-1;
    sp->opt=0;

    sip->length=sins.length;
    sip->loopstart=sins.loopstart;
    sip->loopend=sins.loopend;
    sip->samprate=8363;
    sip->type=((sins.flag&1)?mcpSampLoop:0)|((sins.flag&4)?mcpSamp16Bit:0)|((hdr.ffv==1)?0:mcpSampUnsigned);
  }

  int bufsize=1024;
  unsigned char *buffer=malloc(bufsize);
  unsigned char *temptrack=malloc(2000);
  if (!temptrack||!buffer)
    return errAllocMem;

  char chanused[32];
  memset(chanused, 0, 32);

  for (t=0; t<hdr.pats; t++)
  {
    bf_seek(file, patpara[t]*16);
    unsigned short patsize;
    bf_read(file, &patsize, 2);
    ims_swap16(&patsize);

    if (patsize>bufsize)
    {
      bufsize=patsize;
      free(buffer);
      buffer=malloc(bufsize);
      if (!buffer)
        return errAllocMem;
    }
    bf_read(file, buffer, patsize);

    short j;
    for (j=0; j<m->channum; j++)
    {
      unsigned char *bp=buffer;
      unsigned char *tp=temptrack;

      unsigned char *cp=tp+2;
      char setorgpan=t==orders[0];
      char setorgvwav=t==orders[0];
      char setorgpwav=t==orders[0];
      short row=0;
      while (row<64)
      {
	unsigned char c=*bp++;
	if (!c)
        {
          if (setorgpan)
          {
            putcmd(&cp, cmdPlayNote|cmdPlayPan, defpan[j]);
            putcmd(&cp, cmdVolVibratoSetWave, 0x10);
            putcmd(&cp, cmdPitchVibratoSetWave, 0x10);
            setorgpan=0;
          }

          if (cp!=(tp+2))
          {
            tp[0]=row;
            tp[1]=cp-tp-2;
            tp=cp;
            cp=tp+2;
          }
          row++;
          continue;
        }
	if (((c&0x1F)!=j)||(hdr.channels[j]==0xFF))
        {
          bp+=((c&0x20)>>4)+((c&0xC0)>>6);
          continue;
        }
        chanused[j]=1;
        signed short nte=-1;
        signed short ins=-1;
        signed short vol=-1;
        unsigned char command=0;
        unsigned char data=0;
        signed int pan=-1;
        unsigned char pansrnd=0;

        if (!row&&(t==orders[0]))
        {
          setorgpan=0;
          pan=defpan[j];
          putcmd(&cp, cmdVolVibratoSetWave, 0x10);
          putcmd(&cp, cmdPitchVibratoSetWave, 0x10);
        }

        if (c&0x20)
        {
          nte=*bp++;
          ins=*bp++-1;
          if (nte<254)
            nte=(nte>>4)*12+(nte&0x0F)+12;
          else
          {
            if (nte==254)
              putcmd(&cp, cmdNoteCut, 0);
            nte=-1;
          }
        }
        if (c&0x40)
        {
          vol=*bp++;
          vol=(vol>0x3F)?0xFF:(vol<<2);
        }
        if (c&0x80)
        {
          command=*bp++;
          data=*bp++;
        }

        if (command==0x18)
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

        if ((command==0x13)&&((data>>4)==0x8))
          pan=(data&0xF)+((data&0xF)<<4);

        if (((command==0x7)||(command==0xC))&&(nte!=-1))
          nte|=128;

        if ((ins!=-1)||(nte!=-1)||(vol!=-1)||(pan!=-1))
        {
          unsigned char *act=cp;
          *cp++=cmdPlayNote;
          if (ins!=-1)
          {
            *act|=cmdPlayIns;
            *cp++=ins;
          }
          if (nte!=-1)
          {
            *act|=cmdPlayNte;
            *cp++=nte;
          }
          if (vol!=-1)
          {
            *act|=cmdPlayVol;
            *cp++=vol;
          }
          if (pan!=-1)
          {
            *act|=cmdPlayPan;
            *cp++=pan;
          }
          if ((command==0x13)&&((data>>4)==0xD))
          {
            *act|=cmdPlayDelay;
            *cp++=data&0xF;
          }
        }

        if (pansrnd)
          putcmd(&cp, cmdPanSurround, 0);

        switch (command)
	{
        case 0x04:
          if (!data)
            putcmd(&cp, cmdSpecial, cmdContMixVolSlide);
          else
          if ((data&0x0F)==0x00)
            putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
          else
          if ((data&0xF0)==0x00)
            putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
          else
          if ((data&0x0F)==0x0F)
            putcmd(&cp, cmdRowVolSlideUp, (data>>4)<<2);
          else
          if ((data&0xF0)==0xF0)
            putcmd(&cp, cmdRowVolSlideDown, (data&0xF)<<2);
          break;
        case 0x05:
          if (!data)
            putcmd(&cp, cmdSpecial, cmdContMixPitchSlideDown);
          else
          if (data<0xE0)
            putcmd(&cp, cmdPitchSlideDown, data);
          else
          if (data<0xF0)
            putcmd(&cp, cmdRowPitchSlideDown, (data&0xF)<<2);
          else
            putcmd(&cp, cmdRowPitchSlideDown, (data&0xF)<<4);
          break;
        case 0x06:
          if (!data)
            putcmd(&cp, cmdSpecial, cmdContMixPitchSlideUp);
          else
          if (data<0xE0)
            putcmd(&cp, cmdPitchSlideUp, data);
          else
          if (data<0xF0)
            putcmd(&cp, cmdRowPitchSlideUp, (data&0xF)<<2);
          else
            putcmd(&cp, cmdRowPitchSlideUp, (data&0xF)<<4);
          break;
        case 0x07:
          putcmd(&cp, cmdPitchSlideToNote, data);
          break;
        case 0x08:
          putcmd(&cp, cmdPitchVibrato, data);
          break;
        case 0x09:
          putcmd(&cp, cmdTremor, data);
          break;
        case 0x0A:
          putcmd(&cp, cmdArpeggio, data);
          break;
        case 0x0B:
          putcmd(&cp, cmdPitchVibrato, 0);
          if (!data)
            putcmd(&cp, cmdSpecial, cmdContVolSlide);
          if ((data&0x0F)&&(data&0xF0))
            data=0;
          if (data&0xF0)
            putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
          else
          if (data&0x0F)
            putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
          break;
        case 0x0C:
          putcmd(&cp, cmdPitchSlideToNote, 0);
          if (!data)
            putcmd(&cp, cmdSpecial, cmdContVolSlide);
          if ((data&0x0F)&&(data&0xF0))
            data=0;
          if (data&0xF0)
            putcmd(&cp, cmdVolSlideUp, (data>>4)<<2);
          else
          if (data&0x0F)
            putcmd(&cp, cmdVolSlideDown, (data&0xF)<<2);
          break;
        case 0x0F:
          putcmd(&cp, cmdOffset, data);
          break;
        case 0x11:
          putcmd(&cp, cmdRetrig, data);
	  break;
	case 0x12:
	  putcmd(&cp, cmdVolVibrato, data);
	  break;
        case 0x13:
          command=data>>4;
	  data&=0x0F;
	  switch (command)
	  {
          case 0x1:
            putcmd(&cp, cmdSpecial, data?cmdGlissOn:cmdGlissOff);
            break;
          case 0x2:
            break; // SET FINETUNE not ok (see protracker)
          case 0x3:
            putcmd(&cp, cmdPitchVibratoSetWave, (data&3)+0x10);
            break;
          case 0x4:
            putcmd(&cp, cmdVolVibratoSetWave, (data&3)+0x10);
            break;
          case 0xC:
            putcmd(&cp, cmdNoteCut, data);
	    break;
          }
          break;
        case 0x15:
          putcmd(&cp, cmdPitchVibratoFine, data);
          break;
        case 0x18: //panning
          break;
        }
      }

      gmdtrack *trk=&m->tracks[t*(m->channum+1)+j];
      unsigned short len=tp-temptrack;

      if (!len)
        trk->ptr=trk->end=0;
      else
      {
        trk->ptr=malloc(len);
        trk->end=trk->ptr+len;
        if (!trk->ptr)
          return errAllocMem;
        memcpy(trk->ptr, temptrack, len);
      }
    }

    unsigned char *tp=temptrack;
    unsigned char *bp=buffer;
    unsigned char *cp=tp+2;

    if (t==orders[0])
    {
      if (hdr.it!=6)
        putcmd(&cp, cmdTempo, hdr.it);
      if (hdr.is!=125)
        putcmd(&cp, cmdSpeed, hdr.is);
      if (hdr.mv!=0x40)
        putcmd(&cp, cmdGlobVol, hdr.mv*4);
    }

    unsigned char row=0;
    while (row<64)
    {
      unsigned char c=*bp++;
      if (!c)
      {
        if (cp!=(tp+2))
        {
          tp[0]=row;
          tp[1]=cp-tp-2;
          tp=cp;
          cp=tp+2;
        }
        row++;
        continue;
      }
      unsigned char command=0;
      unsigned char data=0;
      if (c&0x20)
        bp+=2;
      if (c&0x40)
        bp++;
      if (c&0x80)
      {
        command=*bp++;
        data=*bp++;
      }

      int curchan=c&0x1F;
      if (curchan>=m->channum)
        continue;

      switch (command)
      {
      case 0x01:
        if (data)
          putcmd(&cp, cmdTempo, data);
        break;
      case 0x02:
        if (data<m->ordnum)
          putcmd(&cp, cmdGoto, data);
        break;
      case 0x03:
        if (data>=0x64)
          data=0;
        putcmd(&cp, cmdBreak, (data&0x0F)+(data>>4)*10);
        break;
      case 0x13:
        command=data>>4;
        data&=0x0F;
        switch (command)
        {
        case 0xB:
          putcmd(&cp, cmdSetChan, curchan);
          putcmd(&cp, cmdPatLoop, data);
          break;
        case 0xE:
          if (data)
            putcmd(&cp, cmdPatDelay, data);
          break;
        }
        break;
      case 0x14:
        if (data>=0x20)
          putcmd(&cp, cmdSpeed, data);
        break;
      case 0x16:
        data=(data>0x3F)?0xFF:(data<<2);
        putcmd(&cp, cmdGlobVol, data);
        break;
      }
    }

    gmdtrack *trk=&m->tracks[t*(m->channum+1)+m->channum];
    unsigned short len=tp-temptrack;

    if (!len)
      trk->ptr=trk->end=0;
    else
    {
      trk->ptr=malloc(len);
      trk->end=trk->ptr+len;
      if (!trk->ptr)
        return errAllocMem;
      memcpy(trk->ptr, temptrack, len);
    }
  }
  free(buffer);
  free(temptrack);

  for (i=0; i<m->instnum; i++)
  {
    gmdinstrument *ip=&m->instruments[i];
    gmdsample *sp=&m->modsamples[i];
    sampleinfo *sip=&m->samples[i];
    if (sp->handle==0xFFFF)
      continue;


    int l=((sip->type&mcpSamp16Bit)?2:1)*sip->length;
    bf_seek(file, smppara[i]*16);
    sip->ptr=malloc(l+16);
    if (!sip->ptr)
      return errAllocMem;
    bf_read(file, sip->ptr, l);
    if (sip->type & mcpSamp16Bit) {
        if (sip->type & mcpSampBigEndian) {
            sip->type &= ~mcpSampBigEndian;
        } else {
            sip->type |= mcpSampBigEndian;
        }
    }
  }

  for (i=m->channum-1; i>=0; i--)
  {
    if (chanused[i])
      break;
    m->channum--;
  }

  if (!m->channum)
    return errFormMiss;

  return errOk;
}
