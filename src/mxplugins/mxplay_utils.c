#include <mint/osbind.h>
#include "mxplay_utils.h"

static int mfpParamsFromHz(int32 hz, uint16* ctrl, uint16* data) {
    const uint32 dividers[7] = {4, 10, 16, 50, 64, 100, 200};
    const uint32 baseclk = 2457600;
    int mindiff = 0xFFFFFF;
    int result = hz;
    *ctrl = 1; *data = 1;
    for (int i=7; i!=0; i--) {
        uint32 val0 = baseclk / dividers[i];
        for (int j=1; j<256; j++) {
            int val = val0 / j;
            int diff = (hz > val) ? (hz - val) : (val - hz);
            if (diff < mindiff) {
                result = val;
                mindiff = diff;
                *ctrl = (i + 1);
                *data = j;
            }
            if (val < hz) {
                break;
            }
        }
    }
    return result;
}

uint32 mxHookTimerA(void(*func)(void), uint32 hz)
{
    mxUnhookTimerA();
    mxTimerALock = 1;
    Jdisint(MFP_TIMERA);
    // todo: save all relevant mfp regs?
    mxTimerAOld = (uint32)Setexc(0x134>>2, -1);
    mxTimerAFunc = func;
    uint16 ctrl, data;
    uint32 real_hz = mfpParamsFromHz(hz, &ctrl, &data);
    Xbtimer(XB_TIMERA, ctrl, data, mxTimerAVec);
    Jenabint(MFP_TIMERA);
    mxTimerALock = 0;
    return real_hz;
}


void mxUnhookTimerA()
{
    if (mxTimerAFunc != null) {
        mxTimerALock = 1;
        Jdisint(MFP_TIMERA);
        (void)Setexc(0x134>>2, mxTimerAOld);
        mxTimerAFunc = 0;
        mxTimerAOld = 0;
    }
}

