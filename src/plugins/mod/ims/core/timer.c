// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// Systemtimer handlers
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release
//  -doj990328  Dirk Jagdmann  <doj@cubic.org>
//    -changed interrupt access calls to calls from irq.h
//  -fd990518   Felix Domke  <tmbinc@gmx.net>
//    -added CLD after the tmOldTimer-call.
//     this removed the devwmix*-STRANGEBUG. (finally, i hope)
//  -fd990817   Felix Domke  <tmbinc@gmx.net>
//    -added tmSetSecure/tmReleaseSecure to ensure that timer is only
//     called when not "indos". needed for devpVXD (and some other maybe).

// modified for c99 and Atari by agranlund 2024

#include "imsrtns.h"
#include "irq.h"
#include "mint/osbind.h"

static void (*tmOldTimer)();
static void (*tmTimerRoutine)();
static volatile long tmCounter;
static volatile long tmTimerRate;
static volatile long tmTicker;


static inline unsigned long DosTimerToHz(int timerval) {
    return 1193180UL / timerval;
}

void tmTimerHandler() {
    tmTicker += tmTimerRate;
    tmTimerRoutine();
}

int tmInit(void (*rout)(), int timerval, int stk)
{
    tmTimerRoutine=rout;
    tmTicker=-timerval;
    tmTimerRate=timerval;

    int hz = DosTimerToHz(timerval);
    mxHookTimerA(tmTimerHandler, hz);
    return 1;
}

void tmSetNewRate(int timerval)
{
    unsigned short sr = mxDisableInterrupts();
    tmCounter = 0;
    tmTicker = -timerval;
    tmTimerRate = timerval;
    int hz = DosTimerToHz(timerval);
    mxChangeTimerA(hz);
    mxRestoreInterrupts(sr);
}

int tmGetTicker()
{
  return tmTicker;
}

void tmSetTicker(int t)
{
  tmTicker+=t-tmGetTicker();
}

int tmGetTimer()
{
    // the original DOS code was able to return the timervalue here.
    // we probably cannot do the same unless we oversample the interrupt
    // frequency and put some counter in there..
    // for now, we can only return at 0 or triggered.
    unsigned long tm = tmTimerRate + tmTicker;
    return umulshr16(tm, 3600);
}

void tmClose()
{
    mxUnhookTimerA();
}
