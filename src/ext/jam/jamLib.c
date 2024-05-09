//-------------------------------------------------------------------------------------
// Jam plugin interface
// 2024, anders.granlund
//-------------------------------------------------------------------------------------
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
//-------------------------------------------------------------------------------------

#include "jam_sdk.h"
#include "string.h"
#include "stdarg.h"
#include "mint/osbind.h"

static char buf[128];
extern uint32 jamLibInited;
extern uint32 jamHookLog;
extern uint32 jamHookAlert;
extern uint32 jamHookNextSong;

uint8* songData;
jamSongInfo* songInfo;

extern void jamCallHookNextSong();
extern void jamCallHookLog(char* msg);
extern void jamCallHookAlert(char* msg);

void (*jamVblFunc)(void);
extern uint32 jamTimerAOld;
extern void jamTimerAVec();
void (*jamTimerAFunc)(void);
extern int16 jamTimerALock;

//-------------------------------------------------------------
// Initialization
//-------------------------------------------------------------
void jamInitLib()
{
    songData = 0;
    songInfo = 0;
    jamVblFunc = null;
    jamTimerAFunc = null;
    jamTimerALock = 0;
    jamTimerAOld = 0;
    jamLibInited = 1;
}

//-------------------------------------------------------------
// Call-outs
//-------------------------------------------------------------
void jamSongFinished()
{
    // todo...
    if (jamHookNextSong) {
        *((volatile uint16*)jamHookNextSong) = 1;
    }
}

void jamLog(jamLogType type, const char* msg, ...)
{
#ifdef DEBUG
    if (!jamLibInited)
        return;

    if ((type == JAM_LOG_DEBUG) && jamHookLog)
    {
        sprintf(buf, "DBG: ");
        va_list va;
        va_start(va, msg);
        vsprintf(&buf[5], msg, va);
        int len = strlen(buf);
        buf[len+0] = '\n';
        buf[len+1] = 0;
        jamCallHookLog(buf);
        va_end(va);
    }
    else if ((type == JAM_LOG_ALERT) && jamHookLog)
    {
        sprintf(buf, "ERR: ");
        va_list va;
        va_start(va, msg);
        vsprintf(&buf[5], msg, va);
        int len = strlen(buf);
        buf[len+0] = '\n';
        buf[len+1] = 0;
        jamCallHookLog(buf);
        va_end(va);
    }
#endif
}

//-------------------------------------------------------------
//
// Message handler
//
//-------------------------------------------------------------
uint32 jamMessageHandler(int32 param, int32 msg, void* data2, void* data1)
{
    switch (msg)
    {
          case JAM_INFO:
        {
            return (uint32) jamOnPluginInfo();
        } break;

          case JAM_INIT:
        {
            jamInitLib();
        } break;

          case JAM_ACTIVATE:
        {
            jamOnPluginStart();
        } break;

          case JAM_SONGSELECT:
        {
            // data1 = pointer to the file content
            songData = (uint8*) data1;
            jamOnLoad(songData);
        } break;

          case JAM_SONGINFO:
        {
            // data1, data2
            // data1 is pointer to song info struct
            // data2 is same as JAM_SONGSELECT:data1 (file content)
            songInfo = (jamSongInfo*) data1;
            jamOnInfo(songInfo);
        } break;

          case JAM_PLAY:
        {
            if (songData && songInfo) {
                jamOnPlay();
            }
        } break;

          case JAM_STOP:
        {
            jamOnStop();
        } break;

          case JAM_DEACTIVATE:
        {
            jamOnPluginStop();
        } break;

          case JAM_DEINIT:
        {
            //jamReleaseLib();
        } break;

          case JAM_SONG_NEXT_HOOK:
        {
            jamHookNextSong = (uint32)data1;
        } break;

          case JAM_LOG_HOOK:
        {
            jamHookLog = (uint32)data1;
        } break;

          case JAM_ALERT_HOOK:
        {
            jamHookAlert = (uint32)data1;
        } break;

        case JAM_UPDATE:
        {
            jamOnUpdate();
        } break;

        default:
        {
        } break;
    }
    return 0;
}


//-------------------------------------------------------------
//
// Timer interrupt
//
//-------------------------------------------------------------


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

uint32 jamHookTimerA(void(*func)(void), uint32 hz)
{
    jamUnhookTimerA();
    jamTimerALock = 1;
    Jdisint(MFP_TIMERA);
    // todo: save all relevant mfp regs?
    jamTimerAOld = (uint32)Setexc(0x134>>2, -1);
    jamTimerAFunc = func;
    uint16 ctrl, data;
    uint32 real_hz = mfpParamsFromHz(hz, &ctrl, &data);
    Xbtimer(XB_TIMERA, ctrl, data, jamTimerAVec);
    Jenabint(MFP_TIMERA);
    jamTimerALock = 0;
    return real_hz;
}

void jamUnhookTimerA()
{
    if (jamTimerAFunc != null) {
        jamTimerALock = 1;
        Jdisint(MFP_TIMERA);
        (void)Setexc(0x134>>2, jamTimerAOld);
        jamTimerAFunc = 0;
        jamTimerAOld = 0;
    }
}

static uint16 disableIrq() {
    uint16 sr;
    __asm__ __volatile__(
        " move.w    sr,%0\n\t"
        " or.w        #0x0700,sr\n\t"
    : "=d"(sr) : : "cc" );
    return sr;
}

static void restoreIrq(uint16 sr) {
    __asm__ __volatile__(
        " move.w    sr,d0\n\t"
    : : "d"(sr) : "cc" );
}


void jamUnhookVbl() {
    if (jamVblFunc == null)
        return;
    uint16 sr = disableIrq();
    volatile uint32* vblq = *((volatile uint32**)0x456);
    uint32  vblc = *((volatile uint32*)0x454);
    for (int i=0; i<vblc; i++) {
        if (vblq[i] == (uint32)jamVblFunc) {
            vblq[i] = 0;
            break;
        }
    }
    jamVblFunc = null;
    restoreIrq(sr);
}


void jamHookVbl(void(*func)(void))
{
    jamUnhookVbl();
    uint16 sr = disableIrq();

    int16 idx = -1;
    volatile uint32* vblq = *((volatile uint32**)0x456);
    uint32  vblc = *((volatile uint32*)0x454);
    for (int i=0; (i<vblc) && (idx < 0); i++) {
        if (vblq[i] == 0) {
            idx = i;
            break;
        }
    }

    // todo: resize vbl queue
    if (idx < 0) {
    }

    if (idx >= 0) {
        vblq[idx] = (uint32)func;
        jamVblFunc = func;
    }

    restoreIrq(sr);
}
