// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// IMS test program
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release
//  -fd981206   Felix Domke    <tmbinc@gmx.net>
//    -edited for new binfile

#include <string.h>
#include <conio.h>
#include <stdio.h>
#include "binfstd.h"
#include "xmplay.h"
#include "gmdplay.h"
#include "mcp.h"
#include "ims.h"
#include "itplay.h"
#include <iostream.h>

void main(int argc, char **argv)
{
  imsinitstruct is;
  imsFillDefaults(is);
  is.bufsize=65536; // 1sec buffer
  is.pollmin=61440; // use polling only after 0.9375sec
//  is.usersetup=0;
  int i;
  for (i=1; i<argc; i++)
    if (!stricmp(argv[i], "nosound"))
      is.usequiet=1;
  if (!imsInit(is))
  {
    printf("could not init\n");
    return;
  }

/*
  gmdmodule mod;
  sbinfile fil;
  fil.open("h:\\mods\\2nd_pm.s3m", fil.openro);
  mpLoadS3M(mod, fil);
  fil.close();
  mpReduceSamples(mod);
  mpLoadSamples(mod);
  mpPlayModule(mod);

  while (!kbhit());
  while (kbhit())
    getch();

  mpStopModule();
  mpFree(mod);
*/


  xmodule mod;
  sbinfile fil;
  if (fil.open("ambpower.mod", fil.openro))
  {
    printf("could not open\n");
    return;
  }
  if (xmpLoadMOD(mod, fil))
  {
    printf("could not load\n");
    return;
  }
  fil.close();

  if (!xmpLoadSamples(mod))
  {
    printf("could not upload\n");
    return;
  }
  if (!xmpPlayModule(mod))
  {
    printf("could not play\n");
    return;
  }

  xmpSetEvPos(0, 0x20000, 2, 8);
  xmpSetEvPos(1, 0x20400, 2, 8);
  while (!kbhit())
  {
    int time1,time2;
    if (mcpIdle)
      mcpIdle();
    while (inp(0x3da)&8);
    while (!(inp(0x3da)&8));
    cerr << "\r" << hex << xmpGetRealPos();
    xmpGetEvPos(0, time1);
    xmpGetEvPos(1, time2);
    outp(0x3c8,0);
    outp(0x3c9,(time1<4096)?(63-(time1/64)):0);
    outp(0x3c9,0);
    outp(0x3c9,(time2<8192)?(63-(time2/128)):0);
  }
  while (kbhit())
    getch();

  xmpStopModule();
  xmpFreeModule(mod);


/*
  static itplayerclass itplayer;
  itplayerclass::module mod;

  sbinfile fil;
  if (fil.open("jeff71d.it", fil.openro))
  {
    printf("could not open\n");
    return;
  }
  if (mod.load(fil))
  {
    printf("could not load\n");
    return;
  }
  fil.close();

  if (!itplayer.loadsamples(mod))
  {
    printf("could not upload\n");
    return;
  }
  if (!itplayer.play(mod, 64))
  {
    printf("could not play\n");
    return;
  }

  itplayer.setevpos(0, 0, 2, 4);
  while (!kbhit())
  {
    int time;
    cerr << "\r" << hex << itplayer.getevpos(0, time) << "\t" << hex << time;
  }

  while (kbhit())
    getch();

  itplayer.stop();
  mod.free();
*/

  imsClose();
}