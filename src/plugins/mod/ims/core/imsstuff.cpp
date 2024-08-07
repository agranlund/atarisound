// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// some empty routines to ship around the lack of symbols for the players 
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release

#include "plinkman.h"

const char *cfGetProfileString(const char *, const char *, const char *)
{
  return 0;
}

const char *cfGetProfileString2(const char *, const char *, const char *, const char *)
{
  return 0;
}

int cfGetProfileInt(const char *, const char *, int, int)
{
  return 0;
}

int cfGetProfileInt2(const char *, const char *, const char *, int, int)
{
  return 0;
}

int cfGetProfileBool(const char *, const char *, int, int)
{
  return 0;
}

int cfGetProfileBool2(const char *, const char *, const char *, int, int)
{
  return 0;
}


int cfCountSpaceList(const char *, int)
{
  return 0;
}

int cfGetSpaceListEntry(char *, const char *&, int)
{
  return 0;
}

void *lnkGetSymbol(const char *)
{
  return 0;
}

int (*plrProcessKey)(unsigned);

void lnkGetAddressInfo(linkaddressinfostruct &, void *)
{
 return;
}

int indosCheck()
{
  return 0;
}
