// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// XMPlay .MOD module loader
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release
//  -kb980717   Tammo Hinrichs <opencp@gmx.net>
//    -removed all references to gmd structures to make this more flexible
//    -added module flag "ismod" to handle some protracker quirks
//    -enabled and added loaders for the auxiliary MOD formats (and removed
//     them from playgmd)
//    -added MODf file type for FastTracker-made MODs
//  -kbwhenever Tammo Hinrichs <opencp@gmx.net>
//    -pattern loading fixed, the old one also looked for patterns in
//     orders which were not used (thanks to submissive's mod loader for
//     helping debugging this)
//    -set VBL timing as default for noisetracker and signature-less mods

// modified for c99 and Atari by agranlund 2024

#include <string.h>
#include "mcp.h"
#include "binfile.h"
#include "imsrtns.h"
#include "xmplay.h"
#include "err.h"

static unsigned short modnotetab[85]=
{
  0xCFF, 0xC44, 0xB94, 0xAED, 0xA50, 0x9BC, 0x930, 0x8AC, 0x830, 0x7BA, 0x74B, 0x6E2,
  0x67F, 0x622, 0x5CA, 0x577, 0x528, 0x4DE, 0x498, 0x456, 0x418, 0x3DD, 0x3A5, 0x371,
  0x340, 0x311, 0x2E5, 0x2BB, 0x294, 0x26F, 0x24C, 0x22B, 0x20C, 0x1EE, 0x1D3, 0x1B9,
  0x1A0, 0x188, 0x172, 0x15E, 0x14A, 0x138, 0x126, 0x116, 0x106, 0x0F7, 0x0E9, 0x0DC,
  0x0D0, 0x0C4, 0x0B9, 0x0AF, 0x0A5, 0x09C, 0x093, 0x08B, 0x083, 0x07C, 0x075, 0x06E,
  0x068, 0x062, 0x05D, 0x057, 0x053, 0x04E, 0x04A, 0x045, 0x041, 0x03E, 0x03A, 0x037,
  0x034, 0x031, 0x02E, 0x02C, 0x029, 0x027, 0x025, 0x023, 0x021, 0x01F, 0x01D, 0x01C, 0
};

static inline unsigned long swapb2(unsigned short a)
{
  return ((a&0xFF)<<9)|((a&0xFF00)>>7);
}

static int loadmod(xmodule *m, binfile *file, int chan, int sig, int opt)
{
  m->envelopes=0;
  m->samples=0;
  m->instruments=0;
  m->sampleinfos=0;
  m->patlens=0;
  m->patterns=0;
  m->orders=0;
  m->nenv=0;
  m->linearfreq=0;
  m->ismod=!(opt&4);

  //unsigned long l=file[1080].getl();
  bf_seek(file,1080);
  unsigned long l=bf_getl(file);

  m->ninst=31;
  m->nchan=0;

  switch (l)
  {
  case 0x2E4B2E4D: // m->K.
  case 0x214B214D: // M!K!
  case 0x34544C46: // FLT4
    m->nchan=4;
    m->ninst=31;
    break;
  case 0x2E542E4E: // N.T.
    m->nchan=4;
    m->ninst=15;
    opt|=2;
    break;
  case 0x31384443: m->nchan=8; break; // CD81

  case 0x315A4454: m->nchan=1; break; // TDZ1
  case 0x325A4454: m->nchan=2; break;
  case 0x335A4454: m->nchan=3; break;
  case 0x345A4454: m->nchan=4; break;
  case 0x355A4454: m->nchan=5; break;
  case 0x365A4454: m->nchan=6; break;
  case 0x375A4454: m->nchan=7; break;
  case 0x385A4454: m->nchan=8; break;
  case 0x395A4454: m->nchan=9; break;

  case 0x4E484331: m->nchan=1; break; // 1CHN...
  case 0x4E484332: m->nchan=2; break;
  case 0x4E484333: m->nchan=3; break;
  case 0x4E484334: m->nchan=4; break;
  case 0x4E484335: m->nchan=5; break;
  case 0x4E484336: m->nchan=6; break;
  case 0x4E484337: m->nchan=7; break;
  case 0x4E484338: m->nchan=8; break;
  case 0x4E484339: m->nchan=9; break;
  case 0x48433031: m->nchan=10; break; // 10CH...
  case 0x48433131: m->nchan=11; break;
  case 0x48433231: m->nchan=12; break;
  case 0x48433331: m->nchan=13; break;
  case 0x48433431: m->nchan=14; break;
  case 0x48433531: m->nchan=15; break;
  case 0x48433631: m->nchan=16; break;
  case 0x48433731: m->nchan=17; break;
  case 0x48433831: m->nchan=18; break;
  case 0x48433931: m->nchan=19; break;
  case 0x48433032: m->nchan=20; break;
  case 0x48433132: m->nchan=21; break;
  case 0x48433232: m->nchan=22; break;
  case 0x48433332: m->nchan=23; break;
  case 0x48433432: m->nchan=24; break;
  case 0x48433532: m->nchan=25; break;
  case 0x48433632: m->nchan=26; break;
  case 0x48433732: m->nchan=27; break;
  case 0x48433832: m->nchan=28; break;
  case 0x48433932: m->nchan=29; break;
  case 0x48433033: m->nchan=30; break;
  case 0x48433133: m->nchan=31; break;
  case 0x48433233: m->nchan=32; break;
  case 0x38544C46: // FLT8
    return errFormSupp;
//    m->nchan=8;
//    break;
  default:
    if (sig==1)
      return errFormSig;
    m->ninst=(sig==2)?31:15;
    opt|=2;
    break;
  }

  if (chan)
    m->nchan=chan;

  if (!m->nchan)
    return errFormSig;

  m->nsampi=m->ninst;
  m->nsamp=m->ninst;
  m->instruments=malloc(sizeof(xmpinstrument)*m->ninst);
  m->samples=malloc(sizeof(xmpsample)*m->ninst);
  m->sampleinfos=malloc(sizeof(sampleinfo)*m->ninst);
  if (!m->instruments||!m->samples||!m->sampleinfos)
    return errAllocMem;
  memset(m->samples, 0, sizeof(*m->samples)*m->ninst);
  memset(m->sampleinfos, 0, sizeof(*m->sampleinfos)*m->ninst);

  //file[0].read(m->name, 20);
  bf_seek(file,0);
  bf_read(file,m->name,20);
  m->name[20]=0;

  int i;

  for (i=0; i<m->nchan; i++) {
    m->panpos[i]=((i*3)&2)?0xFF:0x00;
  }

  for (i=0; i<m->ninst; i++)
  {
    struct //__attribute((packed))
    {
      char name[22];
      unsigned short length;
      signed char finetune;
      unsigned char volume;
      unsigned short loopstart;
      unsigned short looplength;
    } mi;

    bf_read(file, &mi, sizeof(mi));
    ims_swap16(&mi.length);
    ims_swap16(&mi.loopstart);
    ims_swap16(&mi.looplength);
    unsigned long length=swapb2(mi.length);
    unsigned long loopstart=swapb2(mi.loopstart);
    unsigned long looplength=swapb2(mi.looplength);

    if (length<4)
      length=0;
    if (looplength<4)
      looplength=0;
    if (!looplength||(loopstart>=length))
      looplength=0;
    else
      if ((loopstart+looplength)>length)
        looplength=length-loopstart;
    if (mi.finetune&0x08)
      mi.finetune|=0xF0;

    xmpinstrument *ip=&m->instruments[i];
    xmpsample *sp=&m->samples[i];
    sampleinfo *sip=&m->sampleinfos[i];

    memcpy(ip->name, mi.name, 22);
    int j;
    for (j=21; j>=0; j--)
      if (ip->name[j]>=0x20)
        break;
    ip->name[j+1]=0;
    memset(ip->samples, -1, 256);
    sp->name[0]=0;
    sp->handle=0xFFFF;
    sp->stdpan=-1;
    sp->opt=0;
    sp->normnote=-mi.finetune*32;
    sp->normtrans=0;
    sp->stdvol=(mi.volume>0x3F)?0xFF:(mi.volume<<2);
    sp->volenv=0xFFFF;
    sp->panenv=0xFFFF;
    sp->pchenv=0xFFFF;
    sp->volfade=0;
    sp->vibspeed=0;
    sp->vibrate=0;
    for (j=0; j<128; j++)
      ip->samples[j]=i;
    sp->handle=i;
    if (!length)
      continue;

    sip->length=length;
    sip->loopstart=loopstart;
    sip->loopend=loopstart+looplength;
    sip->samprate=8363;
    sip->type=looplength?mcpSampLoop:0;
  }

  unsigned char orders[128];
  unsigned char ordn, loopp;

  bf_read(file, &ordn,1);
  bf_read(file, &loopp,1);

  bf_read(file, orders, 128);
  if (loopp>=ordn)
    loopp=0;

  short pn=0;
  short t;
  for (t=0; t<ordn; t++)
    if (pn<orders[t])
      pn=orders[t];
  pn++;

  m->nord=ordn;
  m->loopord=loopp;

  m->npat=pn;

  m->initempo=6;
  m->inibpm=125;

  if (sig)
    bf_seekcur(file, 4);

  m->orders=malloc(sizeof(unsigned short)*m->nord);
  m->patlens=malloc(sizeof(unsigned short)*m->npat);
  m->patterns=(unsigned char (**)[5])malloc(sizeof(void*)*m->npat);
  unsigned char *temppat=malloc(4*64*m->nchan);
  if (!m->orders||!m->patlens||!m->patterns||!temppat)
    return errAllocMem;

  for (i=0; i<m->nord; i++)
    m->orders[i]=orders[i];

  memset(m->patterns, 0, sizeof(*m->patterns)*m->npat);

  for (i=0; i<m->npat; i++)
  {
    m->patlens[i]=64;
    m->patterns[i]=malloc(64*m->nchan*5);
    if (!m->patterns[i])
      return errAllocMem;
  }

  for (i=0; i<pn; i++)
  {
    unsigned char *dp=(unsigned char *)(m->patterns[i]);
    unsigned char *sp=temppat;
    bf_read(file, temppat, 256*m->nchan);
    int j;
    for (j=0; j<(64*m->nchan); j++)
    {
      unsigned short nvalue=((short)(sp[0]&0xF)<<8)+sp[1];
      dp[0]=0;
      if (nvalue)
      {
        int k;
        for (k=0; k<85; k++)
          if (modnotetab[k]<=nvalue)
            break;
        dp[0]=k+13;
      }
      dp[1]=(sp[2]>>4)|(sp[0]&0x10);
      dp[2]=0;
      dp[3]=sp[2]&0xF;
      dp[4]=sp[3];

      if (dp[3]==0xE)
      {
        dp[3]=36+(dp[4]>>4);
        dp[4]&=0xF;
      }
      if (opt&1 && dp[3]==0x08) // DMP panning
      {
        unsigned char pan=dp[4];
        if (pan==164)
          pan=0xC0;
        if (pan>0x80)
          pan=0x100-pan;
        pan=(pan==0x80)?0xFF:(pan<<1);
        dp[4]=pan;
      }
      if (opt&2 && dp[3]==0x0f) // old "set tempo"
      {
        dp[3]=xmpCmdMODtTempo;
      }

      sp+=4;
      dp+=5;
    }
  }
  free(temppat);

  for (i=0; i<m->ninst; i++)
  {
    xmpinstrument *ip=&m->instruments[i];
    xmpsample *sp=&m->samples[i];
    sampleinfo *sip=&m->sampleinfos[i];
    if (sp->handle==0xFFFF)
      continue;
    sip->ptr=malloc(sip->length+8);
    if (!sip->ptr)
      return errAllocMem;
    memset(sip->ptr, 0, sip->length);
    bf_read(file, sip->ptr, sip->length);
    //dbgprintf("sample %04x : %d : %d", sip->type, sip->samprate, sip->length);
    sp->handle=i;
  }

  return errOk;
}

int xmpLoadMOD(xmodule *m, binfile *file)
{
  return loadmod(m, file, 0, 1, 0);
}

int xmpLoadMODt(xmodule *m, binfile *file)
{
  return loadmod(m, file, 0, 1, 2);
}

int xmpLoadMODd(xmodule *m, binfile *file)
{
  return loadmod(m, file, 0, 1, 1);
}

int xmpLoadM31(xmodule *m, binfile *file)
{
  return loadmod(m, file, 4, 2, 0);
}

int xmpLoadM15(xmodule *m, binfile *file)
{
  return loadmod(m, file, 4, 0, 0);
}

int xmpLoadM15t(xmodule *m, binfile *file)
{
  return loadmod(m, file, 4, 0, 2);
}

int xmpLoadWOW(xmodule *m, binfile *file)
{
  return loadmod(m, file, 8, 1, 0);
}

int xmpLoadMODf(xmodule *m, binfile *file)
{
  return loadmod(m, file, 0, 1, 4);
}