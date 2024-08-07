// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// IRQ handlers
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release

// modified for c99 and Atari by agranlund 2024

#include "string.h"

// todo: this is called by GUS code if built to use GUS instead of MFP timer

static unsigned char irqNum;
static void* irqOldInt;
static unsigned char irqOldMask;
static void (*irqRoutine)();
static unsigned char irqPreEOI;
static char* stack;
static unsigned long stacksize;
static unsigned char stackused;
static void* oldssesp;

void* getvect(unsigned char intno)
{
    // todo:
    return 0;
}

void setvect(unsigned char intno, void* vect)
{
    // todo:
}

static void stackcall(void* func)
{
    // todo:
    // save old ssp
    // set new ssp 
    // call func
    // restore spp   
}

__attribute__((interrupt)) void irqInt()
{
    // todo:
    if (!stackused)
    {
        stackused++;
        if (irqPreEOI)
        {
            // todo: allow irq reentrant
        }
        stackcall(irqRoutine);
        stackused--;
    }
    // todo: signal end of interrupt
}

int irqInit(int inum, void (*routine)(), int pre, int stk)
{
    // todo:
    stacksize=stk;
    stack=malloc(stacksize);
    if (!stack)
        return 0;
    stack+=stacksize;

    irqPreEOI=pre;
    inum&=15;
    irqNum=inum;
    irqOldInt=getvect(irqNum);

    irqRoutine=routine;
    setvect(irqNum, irqInt);
    // todo: enable irq6
    return 1;
}

void irqClose()
{
    // todo:
    setvect(irqNum, irqOldInt);
    free(stack-stacksize);
}

void irqReInit()
{
    // todo:
}


