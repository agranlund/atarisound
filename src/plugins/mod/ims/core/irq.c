// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// IRQ handlers
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release

// modified for c99 and Atari by agranlund 2024

#include "imsrtns.h"

static unsigned char irqNum;
static void (*irqRoutine)();

void irqHandler() {
    if (irqRoutine) {
        irqRoutine();
    }
}

int irqInit(int inum, void (*routine)(), int pre, int stk) {
    irqRoutine=routine;
    irqNum = inum & 15;
    if (!mxHookIsaInterrupt(irqHandler, irqNum)) {
        irqRoutine = 0;
        irqNum = 0;
        return 0;
    }
    return 1;
}

void irqClose() {
    if (irqRoutine) {
        mxUnhookIsaInterrupt();
        irqRoutine = 0;
        irqNum = 0;
    }
}

