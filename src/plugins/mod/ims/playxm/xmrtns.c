// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// XMPlay auxiliary routines
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release
//  -kb980717   Tammo Hinrichs <opencp@gmx.net>
//    -removed all references to gmd structures to make this more flexible

// modified for c99 and Atari by agranlund 2024

#include <string.h>
#include "mcp.h"
#include "xmplay.h"
#include "err.h"

void xmpFreeModule(xmodule *m)
{
  int i;
  if (m->sampleinfos)
    for (i=0; i<m->nsampi; i++)
      free(m->sampleinfos[i].ptr);
  free(m->sampleinfos);
  free(m->samples);
  if (m->envelopes)
    for (i=0; i<m->nenv; i++)
      free(m->envelopes[i].env);
  free(m->envelopes);
  free(m->instruments);
  if (m->patterns)
    for (i=0; i<m->npat; i++)
      free(m->patterns[i]);
  free(m->patterns);
  free(m->patlens);
  free(m->orders);
}

void xmpOptimizePatLens(xmodule *m)
{
  unsigned char *lastrows=malloc(m->npat);
  if (!lastrows)
    return;
  memset(lastrows, 0, m->npat);
  int i,j,k;
  for (i=0; i<m->nord; i++)
  {
    if (m->orders[i]==0xFFFF)
      continue;
    int first=0;
    for (j=0; j<m->patlens[m->orders[i]]; j++)
    {
      int neword=-1;
      int newrow=-1;
      for (k=0; k<m->nchan; k++)
        switch (m->patterns[m->orders[i]][m->nchan*j+k][3])
        {
        case xmpCmdJump:
          neword=m->patterns[m->orders[i]][m->nchan*j+k][4];
          newrow=0;
          break;
        case xmpCmdBreak:
          if (neword==-1)
            neword=i+1;
          newrow=m->patterns[m->orders[i]][m->nchan*j+k][4];
          break;
        }
      if (neword!=-1)
      {
        while ((neword<m->nord)&&(m->orders[neword]==0xFFFF))
          neword++;
        if (neword>=m->nord)
        {
          neword=0;
          newrow=0;
        }
        if ((newrow>=m->patlens[m->orders[neword]]))
        {
          neword++;
          newrow=0;
        }
        if (neword>=m->nord)
          neword=0;
        if (newrow)
          lastrows[m->orders[neword]]=m->patlens[m->orders[neword]]-1;
        if (!first)
        {
          first=1;
          if (!lastrows[m->orders[i]])
            lastrows[m->orders[i]]=j;
        }
      }
    }
    if (!first)
      lastrows[m->orders[i]]=m->patlens[m->orders[i]]-1;
  }

  for (i=0; i<m->npat; i++)
    m->patlens[i]=lastrows[i]+1;
  free(lastrows);
}
