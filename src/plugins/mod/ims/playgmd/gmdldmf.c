// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// GMDPlay loader for X-Tracker modules
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release

#include <string.h>
#include "binfile.h"
#include "mcp.h"
#include "gmdplay.h"
#include "err.h"

static const unsigned char *ibuf;
static unsigned long bitbuf;
static char bitnum;

static inline unsigned short readbitsdmf(char n)
{
  unsigned short v=bitbuf&((1L<<n)-1);
  bitbuf>>=n;
  bitnum-=n;
  if (bitnum<=24)
  {
    bitbuf|=(long)*ibuf++<<bitnum;
    bitnum+=8;
  }
  return v;
}

static unsigned short nodenum, lastnode;
static short huff[255][3];

static void readtree()
{
  huff[nodenum][2]=readbitsdmf(7);
  short (&node)[3]=huff[lastnode];
  unsigned char left=readbitsdmf(1);
  unsigned char right=readbitsdmf(1);
  lastnode=++nodenum;
  if (left)
  {
    node[0]=lastnode;
    readtree();
  }
  else
    node[0]=-1;
  lastnode=nodenum;
  if (right)
  {
    node[1]=lastnode;
    readtree();
  }
  else
    node[1]=-1;
}

static void unpack0(unsigned char *ob, const void *ib, unsigned long len)
{
  ibuf=(const unsigned char*)ib;
  bitbuf=*(long*)ibuf;
  ibuf+=4;
  bitnum=32;

  nodenum=lastnode=0;
  readtree();

  unsigned long i;
  for (i=0; i<len; i++)
  {
    unsigned char sign=readbitsdmf(1)?0xFF:0;
    unsigned short pos=0;
    while ((huff[pos][0]!=-1)&&(huff[pos][1]!=-1))
      pos=huff[pos][readbitsdmf(1)];
    *ob++=huff[pos][2]^sign;
  }
}

static inline void putcmd(unsigned char *&p, unsigned char c, unsigned char d)
{
  *p++=c;
  *p++=d;
}

static void calctempo(unsigned short rpm, unsigned char &tempo, unsigned char &bpm)
{
  for (tempo=30; tempo>1; tempo--)
    if ((rpm*tempo/24)<256)
      break;
  bpm=rpm*tempo/24;
}

extern "C" int mpLoadDMF(gmdmodule &m, binfile &file)
{
  mpReset(m);

  struct
  {
    unsigned long sig;
    unsigned char ver;
    char tracker[8];
    char name[30];
    char composer[20];
    char date[3];
  } hdr;

  file.read(&hdr, sizeof(hdr));
  if (hdr.sig!=*(unsigned long*)"DDMF")
    return errFormSig;

  if (hdr.ver<5)
    return errFormOldVer;

  m.options=MOD_TICK0|MOD_EXPOFREQ;

  memcpy(m.name, hdr.name, 30);
  m.name[30]=0;

  memcpy(m.composer, hdr.composer, 20);
  m.composer[20]=0;

  unsigned long sig;
  unsigned long next;

  sig=file.getl();
  next=file.getl();

  if (sig==*(unsigned long*)"INFO")
  {
    file.seekcur(next);
    sig=file.getl();
    next=file.getl();
  }

  if (sig==*(unsigned long*)"CMSG")
  {
    file.getc();
    unsigned short msglen=(next-1)/40;

    if (msglen)
    {
      m.message=new char *[msglen+1];
      if (!m.message)
        return errAllocMem;
      *m.message=new char [msglen*41];
      if (!*m.message)
        return errAllocMem;
      short t;
      for (t=0; t<msglen; t++)
      {
        m.message[t]=*m.message+t*41;
        file.read(m.message[t], 40);
        short i;
        for (i=0; i<40; i++)
          if (!m.message[t][i])
            m.message[t][i]=' ';
        m.message[t][40]=0;
      }
      m.message[msglen]=0;
    }

    sig=file.getl();
    next=file.getl();
  }



  if ((sig!=*(unsigned long*)"SEQU")||(next&1))
    return errFormStruc;

  unsigned short ordloop,ordnum;
  unsigned short *orders=new unsigned short [next-4]; // maybe too much...
  if (!orders)
    return errAllocMem;
  ordloop=file.gets();
  ordnum=file.gets();
  file.read(orders, next-4);
  ordnum++;

  if (2*ordnum>(next-4))
    ordnum=(next-4)/2;
  if (ordloop>=ordnum)
    ordloop=0;

  sig=file.getl();
  next=file.getl();

  if (sig!=*(unsigned long*)"PATT")
    return errFormStruc;

  unsigned short patnum;
  unsigned char chnnum;
  patnum=file.gets();
  chnnum=file.getc();
  m.channum=chnnum;

  unsigned char *patbuf=new unsigned char [next-3];
  unsigned char **patadr=new unsigned char *[patnum];
  unsigned char (*temptrack)[3000]=new unsigned char[m.channum+1][3000];
  if (!patbuf||!patadr||!temptrack)
    return errAllocMem;
  file.read(patbuf, next-3);

// get the pattern start adresses
  unsigned char *curadr=patbuf;
  short i;
  for (i=0; i<patnum; i++)
  {
    patadr[i]=curadr;
    curadr+=8+*(unsigned long*)(curadr+4);
  }

//get the new order number
  unsigned short nordnum=0;
  for (i=0; i<ordnum; i++)
    nordnum+=(*(unsigned short*)(patadr[orders[i]]+2)>256)?2:1;

//relocate orders
  unsigned short curord=nordnum;
  for (i=ordnum-1; i>=0; i--)
  {
    if (*(unsigned short*)(patadr[orders[i]]+2)>256)
    {
      curord-=2;
      orders[curord]=orders[i];
      orders[curord+1]=orders[i]|0x8000;
    }
    else
      orders[--curord]=orders[i];
    if (i==ordloop)
      ordloop=curord;
  }
  ordnum=nordnum;

  m.patnum=ordnum;
  m.ordnum=ordnum;
  m.endord=m.patnum;
  m.loopord=ordloop;
  m.tracknum=ordnum*(m.channum+1);

  if (!mpAllocTracks(m, m.tracknum)||!mpAllocPatterns(m, m.patnum)||!mpAllocOrders(m, m.ordnum))
    return errAllocMem;

  for (i=0; i<m.ordnum; i++)
    m.orders[i]=i;

  unsigned char speed=125;
  unsigned char ttype=1;
  unsigned char pbeat=8;
  unsigned char *pp;
  unsigned char voc;
  unsigned short len;
  unsigned char nextinfobyte[33];

// convert patterns
  for (i=0; i<ordnum; i++)
  {
//    if (orders[i]&0x8000)
//      return errFormStruc;
    short j,row;
    short rownum;

    if (!(orders[i]&0x8000))
    {
      pp=patadr[orders[i]&~0x8000];
      voc=*pp++;
      pbeat=(*pp++)>>4;
      if (!pbeat)
        pbeat=8;
      len=*(unsigned short*)pp;
      pp+=6;
      memset(nextinfobyte, 0, 33);
      if (len>256)
        rownum=256;
      else
        rownum=len;
    }
    else
      rownum=len-256;

    m.patterns[i].patlen=rownum;

    unsigned char *(tp[33]);
    for (j=0; j<=m.channum; j++)
      tp[j]=temptrack[j];

    for (j=voc; j<m.channum; j++)
    {
      *tp[j]++=0;
      *tp[j]++=2;
      *tp[j]++=cmdKeyOff;
      *tp[j]++=0;
    }

    for (row=0; row<rownum; row++)
    {
      if (!nextinfobyte[m.channum])
      {
        unsigned char info=*pp++;
        if (info&0x80)
          nextinfobyte[m.channum]=*pp++;
        info&=~0x80;
        unsigned char data;
        if (info)
          data=*pp++;

        unsigned char *cp=tp[m.channum]+2;

        unsigned char tempochange=!row&&ttype&&!(orders[i]&0x8000);

        switch (info)
        {
        case 1:
          ttype=0;
          speed=data;
          tempochange=1;
          break;
        case 2:
          ttype=1;
          speed=data;
          tempochange=1;
          break;
        case 3:
          pbeat=data>>4;
          tempochange=ttype;
          break;
        }

        if (tempochange)
        {
          unsigned char tempo;
          unsigned char bpm;
          if (ttype&&pbeat)
            calctempo(speed*pbeat, tempo, bpm);
          else
            calctempo((speed+1)*15, tempo, bpm);
          putcmd(cp, cmdTempo, tempo);
          putcmd(cp, cmdSpeed, bpm);
        }

        if (cp!=(tp[m.channum]+2))
        {
          tp[m.channum][0]=row;
          tp[m.channum][1]=cp-tp[m.channum]-2;
          tp[m.channum]=cp;
        }
      }
      else
        nextinfobyte[m.channum]--;

      for (j=0; j<voc; j++)
        if (!nextinfobyte[j])
        {
          unsigned char cmds[9];
          unsigned char info=*pp++;
          if (info&0x80)
            nextinfobyte[j]=*pp++;
          if (info&0x40)
            cmds[0]=*pp++;
          else
            cmds[0]=0;
          if (info&0x20)
            cmds[1]=*pp++;
          else
            cmds[1]=0;
          if (info&0x10)
            cmds[2]=*pp++;
          else
            cmds[2]=0;
          if (info&0x08)
          {
            cmds[3]=*pp++;
            cmds[4]=*pp++;
          }
          else
            cmds[3]=cmds[4]=0;
          if (info&0x04)
          {
            cmds[5]=*pp++;
            cmds[6]=*pp++;
          }
          else
            cmds[5]=cmds[6]=0;
          if (info&0x02)
          {
            cmds[7]=*pp++;
            cmds[8]=*pp++;
          }
          else
            cmds[7]=cmds[8]=0;

          unsigned char *cp=tp[j]+2;

          if (cmds[0]||(cmds[1]&&(cmds[1]!=255))||cmds[2]||(cmds[7]==7))
          {
            unsigned char &act=*cp;
             *cp++=cmdPlayNote;
            if (cmds[0])
            {
              act|=cmdPlayIns;
              *cp++=cmds[0]-1;
            }
            if (cmds[1]&&(cmds[1]!=255))
            {
              act|=cmdPlayNte;
              *cp++=cmds[1]+23;
            }
            if (cmds[2])
            {
              act|=cmdPlayVol;
              *cp++=cmds[2]-1;
            }
            if (cmds[7]==7)
            {
              act|=cmdPlayPan;
              *cp++=cmds[8];
            }
          }
          if (cmds[1]==255)
            putcmd(cp, cmdKeyOff, 0);

          switch (cmds[3])
          {
          case 1:
            putcmd(cp, cmdKeyOff, 0); // falsch!
            break;
          case 2:
            putcmd(cp, cmdSetLoop, 0);
            break;
          case 6:
            putcmd(cp, cmdOffset, cmds[4]);
            break;
          }

          switch (cmds[5])
          {
          case 1:
            putcmd(cp, cmdRowPitchSlideDMF, cmds[6]);
            break;
          case 3:
            putcmd(cp, cmdArpeggio, cmds[6]);
            break;
          case 4:
            putcmd(cp, cmdPitchSlideUDMF, cmds[6]);
            break;
          case 5:
            putcmd(cp, cmdPitchSlideDDMF, cmds[6]);
            break;
          case 6:
            putcmd(cp, cmdPitchSlideNDMF, cmds[6]);
            break;
          case 8:
            putcmd(cp, cmdPitchVibratoSinDMF, cmds[6]);
            break;
          case 9:
            putcmd(cp, cmdPitchVibratoTrgDMF, cmds[6]);
            break;
          case 10:
            putcmd(cp, cmdPitchVibratoRecDMF, cmds[6]);
            break;
          case 12:
            putcmd(cp, cmdKeyOff, 0); // falsch!
            break;
          }

          switch (cmds[7])
          {
          case 1:
            putcmd(cp, cmdVolSlideUDMF, cmds[8]);
            break;
          case 2:
            putcmd(cp, cmdVolSlideDDMF, cmds[8]);
            break;
          case 4:
            putcmd(cp, cmdVolVibratoSinDMF, cmds[8]);
            break;
          case 5:
            putcmd(cp, cmdVolVibratoTrgDMF, cmds[8]);
            break;
          case 6:
            putcmd(cp, cmdVolVibratoRecDMF, cmds[8]);
            break;
          case 8:
            putcmd(cp, cmdPanSlideLDMF, cmds[8]);
            break;
          case 9:
            putcmd(cp, cmdPanSlideRDMF, cmds[8]);
            break;
          case 10:
            putcmd(cp, cmdPanVibratoSinDMF, cmds[8]);
            break;
          }

          if (cp!=(tp[j]+2))
          {
            tp[j][0]=row;
            tp[j][1]=cp-tp[j]-2;
            tp[j]=cp;
          }
        }
        else
          nextinfobyte[j]--;
    }

    for (j=0; j<m.channum; j++)
    {
      m.patterns[i].tracks[j]=i*(m.channum+1)+j;

      gmdtrack &trk=m.tracks[i*(m.channum+1)+j];
      unsigned short tlen=tp[j]-temptrack[j];

      if (!tlen)
        trk.ptr=trk.end=0;
      else
      {
        trk.ptr=new unsigned char[tlen];
        trk.end=trk.ptr+tlen;
        if (!trk.ptr)
          return errAllocMem;
        memcpy(trk.ptr, temptrack[j], tlen);
      }
    }

    m.patterns[i].gtrack=i*(m.channum+1)+m.channum;

    gmdtrack &trk=m.tracks[i*(m.channum+1)+m.channum];
    unsigned short tlen=tp[m.channum]-temptrack[m.channum];

    if (!tlen)
      trk.ptr=trk.end=0;
    else
    {
      trk.ptr=new unsigned char[tlen];
      trk.end=trk.ptr+tlen;
      if (!trk.ptr)
        return errAllocMem;
      memcpy(trk.ptr, temptrack[m.channum], tlen);
    }
  }

  delete temptrack;
  delete patbuf;
  delete patadr;
  delete orders;

  sig=file.getl();
  next=file.getl();

// inst!!

  if ((sig!=*(unsigned long*)"SMPI"))
    return errFormStruc;

  m.modsampnum=m.sampnum=m.instnum=file.getc();

  if (!mpAllocInstruments(m, m.instnum)||!mpAllocSamples(m, m.sampnum)||!mpAllocModSamples(m, m.modsampnum))
    return errAllocMem;

  unsigned char smppack[256];

  for (i=0; i<m.instnum; i++)
  {
    gmdinstrument &ip=m.instruments[i];
    gmdsample &sp=m.modsamples[i];
    sampleinfo &sip=m.samples[i];

    unsigned char namelen;
    namelen=file.getc();
    if (namelen>31)
    {
      file.read(ip.name, 31);
      file.seekcur(namelen-31);
      namelen=31;
    }
    else
      file.read(ip.name, namelen);
    ip.name[namelen]=0;
    struct
    {
      unsigned long length;
      unsigned long loopstart;
      unsigned long loopend;
      unsigned short freq;
      unsigned char vol;
      unsigned char type;
      unsigned char filler[10];
      unsigned long crc32;
    } smp;
    file.read(&smp, sizeof(smp)-((hdr.ver<8)?8:0));
    smppack[i]=!!(smp.type&0x04);
    unsigned char bit16=!!(smp.type&0x02);
    if (smp.type&0x88)
      return errFormSupp; // can't do this
    if (bit16&&smppack[i])
      return errFormSupp; // don't want 16 bit packed samples..
    sip.length=smp.length>>bit16;
    sip.loopstart=smp.loopstart>>bit16;
    sip.loopend=smp.loopend>>bit16;
    sip.samprate=smp.freq;
    sip.type=((smp.type&4)?mcpSampDelta:0)|(bit16?mcpSamp16Bit:0)|((smp.type&1)?mcpSampLoop:0);

    if (!smp.length)
      continue;

    int j;
    for (j=0; j<128; j++)
      ip.samples[j]=i;
    *sp.name=0;
    sp.handle=i;
    sp.normnote=0;
    sp.stdvol=smp.vol?smp.vol:-1;
    sp.stdpan=-1;
    sp.opt=bit16?MP_OFFSETDIV2:0;
  }

  sig=file.getl();
  next=file.getl();

  if ((sig!=*(unsigned long*)"SMPD"))
    return errFormStruc;

  for (i=0; i<m.instnum; i++)
  {
    gmdinstrument &ip=m.instruments[i];
    gmdsample &sp=m.modsamples[i];
    sampleinfo &sip=m.samples[i];

    unsigned long len;
    len=file.getl();

    if (sp.handle==0xFFFF)
    {
      file.seekcur(len);
      continue;
    }

    unsigned char *smpp=new unsigned char[len];
    if (!smpp)
      return errAllocMem;
    file.read(smpp, len);
    if (smppack[i])
    {
      unsigned char *dbuf=new unsigned char[sip.length+16];
      if (!dbuf)
        return errAllocMem;
      unpack0(dbuf, smpp, sip.length);
      delete smpp;
      smpp=dbuf;
    }

    sip.ptr=smpp;
  }

  return errOk;
}