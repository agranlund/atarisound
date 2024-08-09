// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// XMPlay .XM module loader
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release
//  -kb980717   Tammo Hinrichs <opencp@gmx.net>
//    -removed all references to gmd structures to make this more flexible
//    -added module flag "ismod" to handle some protracker quirks

// modified for c99 and Atari by agranlund 2024

#include <string.h>
#include "mcp.h"
#include "binfile.h"
#include "imsrtns.h"
#include "xmplay.h"
#include "err.h"


int xmpLoadModule(xmodule *m, binfile *file)
{

  m->envelopes=0;
  m->samples=0;
  m->instruments=0;
  m->sampleinfos=0;
  m->patlens=0;
  m->patterns=0;
  m->orders=0;
  m->ismod=0;

  struct //__attribute((packed))
  {
    char sig[17];
    char name[20];
    char eof;
    char tracker[20];
    unsigned short ver;
    unsigned long hdrsize;
  } head1;

  struct //__attribute((packed))
  {
    unsigned short nord;
    unsigned short loopord;
    unsigned short nchan;
    unsigned short npat;
    unsigned short ninst;
    unsigned short freqtab;
    unsigned short tempo;
    unsigned short bpm;
    unsigned char ord[256];
  } head2;

  bf_read(file, &head1, sizeof(head1));
  ims_swap16(&head1.ver);
  ims_swap32(&head1.hdrsize);

  if (memcmp(head1.sig, "Extended Module: ", 17))
    return errFormStruc;
  if (head1.eof!=26)
    return errFormStruc;
  if (head1.ver<0x104)
    return errFormOldVer;

  bf_read(file, &head2, sizeof(head2));
  ims_swap16(&head2.nord);
  ims_swap16(&head2.loopord);
  ims_swap16(&head2.nchan);
  ims_swap16(&head2.npat);
  ims_swap16(&head2.ninst);
  ims_swap16(&head2.freqtab);
  ims_swap16(&head2.tempo);
  ims_swap16(&head2.bpm);

  bf_seekcur(file, head1.hdrsize-4-sizeof(head2));

  if (!head2.ninst)
    return errFormMiss;

  memcpy(m->name, head1.name, 20);
  m->name[20]=0;

  m->linearfreq=!!(head2.freqtab&1);
  m->nchan=head2.nchan;
  m->ninst=head2.ninst;
  m->nenv=head2.ninst*2;
  m->npat=head2.npat+1;
  m->nord=head2.nord;
  m->loopord=head2.loopord;
  m->inibpm=head2.bpm;
  m->initempo=head2.tempo;

  m->orders=(unsigned short*) malloc(sizeof(unsigned short) * head2.nord);
  m->patterns=(unsigned char (**)[5]) malloc(sizeof(void*) * (head2.npat+1));
  m->patlens=(unsigned short*) malloc(sizeof(unsigned short) * (head2.npat+1));
  m->instruments=(xmpinstrument*) malloc(sizeof(xmpinstrument) * head2.ninst);
  m->envelopes=(xmpenvelope*) malloc(sizeof(xmpenvelope) * (head2.ninst*2));
  sampleinfo **smps=(sampleinfo **)malloc(sizeof(sampleinfo*) * head2.ninst);
  xmpsample **msmps=(xmpsample **)malloc(sizeof(xmpsample*) * head2.ninst);
  int *instsmpnum=(int *)malloc(sizeof(int) *  head2.ninst);

  if (!smps||!msmps||!instsmpnum||!m->instruments||!m->envelopes||!m->patterns||!m->orders||!m->patlens)
    return errAllocMem;

  memset(m->patterns, 0, (head2.npat+1)*sizeof(void*));
  memset(m->envelopes, 0, (head2.ninst*2)*sizeof(xmpenvelope));

  int i,j,k;

  for (i=0; i<m->nchan; i++)
    m->panpos[i]=0x80;

  for (i=0; i<head2.nord; i++)
    m->orders[i]=(head2.ord[i]<head2.npat)?head2.ord[i]:head2.npat;

  m->patlens[head2.npat]=64;
  m->patterns[head2.npat]=malloc(64*head2.nchan*5);
  if (!m->patterns[head2.npat])
    return errAllocMem;
  memset(m->patterns[head2.npat], 0, 64*5*head2.nchan);

  for (i=0; i<head2.npat; i++)
  {
    struct //__attribute((packed))
    {
      unsigned long len;
      unsigned char ptype;
      unsigned short rows;
      unsigned short patdata;
    } pathead;

    // read each entry individually since this stuff is not aligned and would break 68000
    #define sizeof_pathead (4+1+2+2)
    bf_read(file, &pathead.len,     4); 
    bf_read(file, &pathead.ptype,   1);
    bf_read(file, &pathead.rows,    2);
    bf_read(file, &pathead.patdata, 2);

    ims_swap32(&pathead.len);
    ims_swap16(&pathead.rows);
    ims_swap16(&pathead.patdata);

    bf_seekcur(file, (pathead.len-sizeof_pathead));
    m->patlens[i]=pathead.rows;
    m->patterns[i]=malloc(pathead.rows*head2.nchan*5);
    if (!m->patterns[i])
      return errAllocMem;
    memset(m->patterns[i], 0, pathead.rows*head2.nchan*5);
    if (!pathead.patdata)
      continue;
    unsigned char *pbuf=(unsigned char*)malloc(pathead.patdata);
    if (!pbuf)
      return errAllocMem;
    bf_read(file, pbuf, pathead.patdata);
    unsigned char *pbp=pbuf;
    unsigned char *cur=(unsigned char*)(m->patterns[i]);
    for (j=0; j<(pathead.rows*head2.nchan); j++)
    {
      unsigned char pack=(*pbp&0x80)?(*pbp++):0x1F;
      for (k=0; k<5; k++)
      {
        *cur++=(pack&1)?*pbp++:0;
        pack>>=1;
      }
      if (cur[-2]==0xE)
      {
        cur[-2]=36+(cur[-1]>>4);
        cur[-1]&=0xF;
      }
    }
    free(pbuf);
  }


  m->nsampi=0;
  m->nsamp=0;
  for (i=0; i<m->ninst; i++)
  {
    xmpinstrument *ip=&m->instruments[i];
    xmpenvelope *env=m->envelopes+2*i;
    smps[i]=0;
    msmps[i]=0;

    struct //__attribute((packed))
    {
      unsigned long size;
      char name[22];
      char type;
      unsigned short samp;
    } ins1;

    // read each entry individually since this stuff is not aligned and would break 68000
    #define sizeof_ins1 (4+22+1+2)
    bf_read(file, &ins1.size, 4);
    bf_read(file, &ins1.name, 22);
    bf_read(file, &ins1.type, 1);
    bf_read(file, &ins1.samp, 2);

    ims_swap32(&ins1.size);
    ims_swap16(&ins1.samp);
    memcpy(ip->name, ins1.name, 22);
    ip->name[22]=0;
    memset(ip->samples, 0xff, 2*128);

    instsmpnum[i]=ins1.samp;
    if (!ins1.samp)
    {
      bf_seekcur(file, (ins1.size-sizeof_ins1));
      continue;
    }

    struct //__attribute((packed))
    {
      unsigned long shsize;
      unsigned char snum[96];
      unsigned short venv[12][2];
      unsigned short penv[12][2];
      unsigned char vnum, pnum;
      unsigned char vsustain, vloops, vloope, psustain, ploops, ploope;
      unsigned char vtype, ptype;
      unsigned char vibtype, vibsweep, vibdepth, vibrate;
      unsigned short volfade;
      unsigned short res;
    } ins2;
    #define sizeof_ins2 sizeof(ins2)
    bf_read(file, &ins2, sizeof_ins2);
    ims_swap32(&ins2.shsize);
    ims_swap16(&ins2.volfade);
    ims_swap16(&ins2.res);
    for (int j=0; j<12;j++) {
        for(int k=0; k<2; k++) {
            ims_swap16(&ins2.venv[j][k]);
            ims_swap16(&ins2.penv[j][k]);
        }
    }

    bf_seekcur(file, (ins1.size-sizeof_ins1-sizeof_ins2));

    smps[i]=malloc(sizeof(sampleinfo)*ins1.samp);
    msmps[i]=malloc(sizeof(xmpsample)*ins1.samp);
    if (!smps[i]||!msmps[i])
      return errAllocMem;
    memset(msmps[i], 0, sizeof(xmpsample)*ins1.samp);
    memset(smps[i], 0, sizeof(sampleinfo)*ins1.samp);

    for (j=0; j<96; j++)
      if (ins2.snum[j]<ins1.samp)
        ip->samples[j]=m->nsamp+ins2.snum[j];
    unsigned short volfade=0xFFFF;
    volfade=ins2.volfade;

    if (ins2.vtype&1)
    {
      env[0].speed=0;
      env[0].type=0;
      env[0].env=malloc(ins2.venv[ins2.vnum-1][0]+1);
      if (!env[0].env)
        return errAllocMem;
      short k, p=0, h=ins2.venv[0][1]*4;
      for (j=1; j<ins2.vnum; j++)
      {
        short l=ins2.venv[j][0]-p;
        short dh=ins2.venv[j][1]*4-h;
        for (k=0; k<l; k++)
        {
          short cv=h+dh*k/l;
          env[0].env[p++]=(cv>255)?255:cv;
        }
        h+=dh;
      }
      env[0].len=p;
      env[0].env[p]=(h>255)?255:h;
      if (ins2.vtype&2)
      {
        env[0].type|=xmpEnvSLoop;
        env[0].sustain=ins2.venv[ins2.vsustain][0];
      }
      if (ins2.vtype&4)
      {
        env[0].type|=xmpEnvLoop;
        env[0].loops=ins2.venv[ins2.vloops][0];
        env[0].loope=ins2.venv[ins2.vloope][0];
      }
    }

    if (ins2.ptype&1)
    {
      env[1].speed=0;
      env[1].type=0;
      env[1].env=malloc(ins2.penv[ins2.pnum-1][0]+1);
      if (!env[1].env)
        return errAllocMem;
      short k, p=0, h=ins2.penv[0][1]*4;
      for (j=1; j<ins2.pnum; j++)
      {
        short l=ins2.penv[j][0]-p;
        short dh=ins2.penv[j][1]*4-h;
        for (k=0; k<l; k++)
        {
          short cv=h+dh*k/l;
          env[1].env[p++]=(cv>255)?255:cv;
        }
        h+=dh;
      }
      env[1].len=p;
      env[1].env[p]=(h>255)?255:h;
      if (ins2.ptype&2)
      {
        env[1].type|=xmpEnvSLoop;
        env[1].sustain=ins2.penv[ins2.psustain][0];
      }
      if (ins2.ptype&4)
      {
        env[1].type|=xmpEnvLoop;
        env[1].loops=ins2.penv[ins2.ploops][0];
        env[1].loope=ins2.penv[ins2.ploope][0];
      }
    }
    for (j=0; j<ins1.samp; j++)
    {
      struct //__attribute((packed))
      {
        unsigned long samplen;
        unsigned long loopstart;
        unsigned long looplen;
        unsigned char vol;
        signed char finetune;
        unsigned char type;
        unsigned char pan;
        signed char relnote;
        unsigned char res;
        unsigned char name[22];
      } samp;
      bf_read(file, &samp, sizeof (samp));
      ims_swap32(&samp.samplen);
      ims_swap32(&samp.loopstart);
      ims_swap32(&samp.looplen);

      bf_seekcur(file, (ins2.shsize-sizeof(samp)));
      if (samp.type&16)
      {
        samp.samplen>>=1;
        samp.loopstart>>=1;
        samp.looplen>>=1;
      }

      xmpsample *sp=&msmps[i][j];
      memcpy(sp->name, samp.name, 22);
      sp->name[22]=0;
      sp->handle=0xFFFF;
      sp->normnote=-samp.relnote*256-samp.finetune*2;
      sp->normtrans=-samp.relnote*256;
      sp->stdvol=(samp.vol>0x3F)?0xFF:(samp.vol<<2);
      sp->stdpan=samp.pan;
      sp->opt=0;
      sp->volfade=volfade;
      sp->vibtype=ins2.vibtype;
      sp->vibdepth=ins2.vibdepth<<2;
      sp->vibspeed=0;
      sp->vibrate=ins2.vibrate<<8;
      sp->vibsweep=0xFFFF/(ins2.vibsweep+1);
      sp->volenv=env[0].env?(2*i+0):0xFFFF;
      sp->panenv=env[1].env?(2*i+1):0xFFFF;
      sp->pchenv=0xFFFF;

      sampleinfo *sip=&smps[i][j];
      sip->length=samp.samplen;
      sip->loopstart=samp.loopstart;
      sip->loopend=samp.loopstart+samp.looplen;
      sip->samprate=8363;
      sip->type=mcpSampDelta|((samp.type&16)?mcpSamp16Bit:0)|((samp.type&3)?(((samp.type&3)>=2)?(mcpSampLoop|mcpSampBiDi):mcpSampLoop):0);
#if 1
      if (sip->type & mcpSamp16Bit) {
        if (sip->type & mcpSampBigEndian) {
            sip->type &= ~mcpSampBigEndian;
        } else {
            sip->type |= mcpSampBigEndian;
        }
      }
#endif      
    }

    for (j=0; j<ins1.samp; j++)
    {
      xmpsample *sp=&msmps[i][j];
      sampleinfo *sip=&smps[i][j];
      unsigned long l = (sip->type & mcpSamp16Bit) ? (sip->length << 1) : sip->length;
      if (!l)
        continue;
      sip->ptr=malloc(l+528);
      if (!sip->ptr)
        return errAllocMem;
      bf_read(file, sip->ptr, l);
      //dbgprintf("sample: %04x", sip->type);
#if 0
      if (sip->type & mcpSamp16Bit) {
        unsigned short* sd = (unsigned short*)sip->ptr;
        for (int k=0; k<sip->length; k++) {
            ims_swap16(&sd[k]);
        }
      }
#endif
      sp->handle=m->nsampi++;
    }
    m->nsamp+=ins1.samp;
  }

  m->samples=malloc(sizeof(xmpsample)*m->nsamp);
  m->sampleinfos=malloc(sizeof(sampleinfo)*m->nsampi);
  if (!m->samples||!m->sampleinfos)
    return errAllocMem;

  m->nsampi=0;
  m->nsamp=0;
  for (i=0; i<m->ninst; i++)
  {
    for (j=0; j<instsmpnum[i]; j++)
    {
      m->samples[m->nsamp++]=msmps[i][j];
      if (smps[i][j].ptr)
        m->sampleinfos[m->nsampi++]=smps[i][j];
    }
    free(smps[i]);
    free(msmps[i]);
  }
  free(smps);
  free(msmps);
  free(instsmpnum);

  return errOk;
}


