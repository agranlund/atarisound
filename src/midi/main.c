//---------------------------------------------------------------------
// Jam midi plugin
// 2024, anders.granlund
//---------------------------------------------------------------------
//
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
//---------------------------------------------------------------------
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "jam_sdk.h"
#include "md_midi.h"
#include "mint/osbind.h"
#include "mint/cookie.h"

// TimerA or VBL playback
#define ENABLE_TIMER_A
#define TIMER_A_HZ      1000

// can't seem to get Midiws working realiably from interrupts in MiNT.
// would be nice with a 'MIDI' cookie with an input/output api
// that each clone system, or PCI/ISA/etc device drivers, could implement.
// todo: use ADI when/if that system is done and has midi routing.
//#define ENABLE_BIOS_MIDI



// -----------------------------------------------------------------------
jamPluginInfo info = {
    JAM_INTERFACE_VERSION,      // interfaceVersion
    0x0003,                     // pluginVersion
    "2024.05.09",               // date
    ".MID",                     // ext0
    ".SMF",                     // ext1
    " ",                        // ext2
    " ",                        // ext3
    " ",                        // ext4
    " ",                        // ext5
    " ",                        // ext6
    " ",                        // ext7
    "MIDI",                     // pluginName
    "Anders Granlund",          // authorName
    "granlund23@yahoo.se",      // authorEmail
    "www.happydaze.se",         // authorUrl
    "",                         // authorComment
    0,                          // isDsp
    0x1F,                       // support
    0,                          // datastart
    1,                          // supportsNextSongHook
    0,                          // supportsName
    0,                          // supportsComposer
    1,                          // supportsSongCount
    1,                          // supportsPreselect
    0,                          // supportsComment
    1,                          // supportsFastram
};


// -----------------------------------------------------------------------
MD_MIDIFile* midi;
uint32 midiTimerTargetHz;
uint32 midiTimerRealHz;
uint32 midiMicrosPerTick;
bool midiTimerA;
uint16 savBuf[23*2*12];

// -----------------------------------------------------------------------
#define BASE_ACIA           0xfffffc04UL
#define BASE_RAVEN_MFP2     0xa0000a00UL
#define C_RAVN              0x5241564EUL        // 'RAVN'


void(*midiWrite)(uint8* buf, uint16 size);

static inline void midiWrite_acia(uint8* buf, uint16 size) {
    for (short i=0; i<size; i++) {
        while(1) {
            volatile uint8 status = *((volatile uint8*)(BASE_ACIA));
            if (status & 2) {
                *((volatile uint8*)(BASE_ACIA+2)) = *buf++;
                break;
            }
        }
    }
}

static inline void midiWrite_raven(uint8* buf, uint16 size) {
    for (short i=0; i<size; i++) {
        while(1) {
            if (*((volatile uint8*)(BASE_RAVEN_MFP2 + 45)) & 0x80) {        // mfp2->tsr
                *((volatile uint8*)(BASE_RAVEN_MFP2 + 47)) = *buf++;        // mfp2->udr
                break;
            }
        }
    }
}

static inline void midiWrite_vampire(uint8* buf, uint16 size) {
    // todo...
}


static inline void midiWrite_bios(uint8* buf, uint16 size) {
#ifdef ENABLE_BIOS_MIDI
    // Hitchhikers Guide to the Bios.
    //
    // It is possible to do a BIOS call from an interrupt handler.
    // More specifically, it is possible for exactly one interrupt handler to call
    // the BIOS at a time.
    //
    // The basic problem is a critical section in the BIOS trap handler code. The
    // critical section occurs when the registers are being saved or restored in the
    // register save area; the variable savptr must be maintained correctly.
    //
    // -- DANGER --
    // Only ONE interrupt handler may do this. That is, two interrupt handlers
    // cannot nest and do BIOS calls in this manner.
    //
    #define savptr *((volatile uint32*)0x4a2)
    #define savamt (23 * 2)
    if (size < 1)
        return;
    //savptr -= savamt;
    uint32 oldsav = savptr;
    savptr = (uint32)&savBuf[23*2*3];
    Midiws(size - 1, buf);
    savptr = oldsav;
    //savptr += savamt;
#endif
}


// -----------------------------------------------------------------------
void(*midiEventHandler)(MD_midi_event*);
void(*midiSysexHandler)(MD_sysex_event*);

void midiEventHandler_acia(MD_midi_event *pev)      { midiWrite_acia(pev->data, pev->size); }
void midiSysexHandler_acia(MD_sysex_event *pev)     { midiWrite_acia(pev->data, pev->size); }
void midiEventHandler_raven(MD_midi_event *pev)     { midiWrite_raven(pev->data, pev->size); }
void midiSysexHandler_raven(MD_sysex_event *pev)    { midiWrite_raven(pev->data, pev->size); }
void midiEventHandler_vampire(MD_midi_event *pev)   { midiWrite_vampire(pev->data, pev->size); }
void midiSysexHandler_vampire(MD_sysex_event *pev)  { midiWrite_vampire(pev->data, pev->size); }
void midiEventHandler_bios(MD_midi_event *pev)      { midiWrite_bios(pev->data, pev->size); }
void midiSysexHandler_bios(MD_sysex_event *pev)     { midiWrite_bios(pev->data, pev->size); }
void midiEventHandler_null(MD_midi_event *pev)      { }
void midiSysexHandler_null(MD_sysex_event *pev)     { }



// -----------------------------------------------------------------------
void midiUpdate_timerA() {
    if (midi) {
        MD_Update(midi, jamTimerATicks * midiMicrosPerTick);
    }
}

void midiUpdate_vbl() {
    if (midi) {
        uint16 sr;
        __asm__ volatile ( "    move.w sr,%0" : "=d"(sr) : : "cc");
        __asm__ volatile ( "    or.w #0x0700,sr" : : : "cc");
        uint32 timerCTicks = *((volatile uint32*)0x4ba);
        MD_Update(midi, timerCTicks * midiMicrosPerTick);
        __asm__ volatile ( "    move.w %0,sr" : : "d"(sr) : "cc");
    }
}

static void midiUnload() {
    if (midi) {
        jamUnhookTimerA();
        MD_Close(midi);
        midi = null;
    }
}

static bool midiLoad(uint8* buf) {
    midiUnload();
    midi = MD_OpenBuffer(buf);
    if (midi) {
        midi->_midiHandler = midiEventHandler;
        midi->_sysexHandler = midiSysexHandler;
        if (midiTimerA) {
            midiTimerRealHz = jamHookTimerA(midiUpdate_timerA, midiTimerTargetHz);
            midiMicrosPerTick = 1000000 / midiTimerRealHz;
        } else {
            midiTimerRealHz = 200;
            midiMicrosPerTick = 1000000 / midiTimerRealHz;
            jamHookVbl(midiUpdate_vbl);
        }
    }
    return midi ? true : false;
}


// -----------------------------------------------------------------------
jamPluginInfo* jamOnPluginInfo() {
    return &info;
}

bool jamOnPluginStart() {
    midi = 0;
    midiTimerTargetHz = TIMER_A_HZ; // todo: adjust for cpu speed?
    midiTimerA = true;

    uint32 c_mch = 0;
    uint32 c_milan = 0;
    uint32 c_hades = 0;
    uint32 c_raven = 0;
    Getcookie(C__MCH, (long int*)&c_mch);
    bool is_t40        = ((c_mch & 0xffff) == 0x4d34);
    bool is_milan    = (Getcookie(C__MIL, (long int*)&c_milan) == C_FOUND);
    bool is_hades    = (Getcookie(C_hade, (long int*)&c_hades) == C_FOUND);
    bool is_raven    = (Getcookie(C_RAVN, (long int*)&c_raven) == C_FOUND);
    bool is_clone    = (c_mch >= 0x00040000) || is_hades || is_t40 || is_milan;
    bool is_aranym    = (c_mch == 0x00050000);
    bool is_vampire    = (c_mch == 0x00060000);

    if (!is_clone) {
        midiEventHandler = midiEventHandler_acia;
        midiSysexHandler = midiSysexHandler_acia;
    }
    else if (is_raven) {
        midiEventHandler = midiEventHandler_raven;
        midiSysexHandler = midiSysexHandler_raven;
    }
    else if (is_vampire) {
        midiEventHandler = midiEventHandler_vampire;
        midiSysexHandler = midiSysexHandler_vampire;
    }
    else {
        midiEventHandler = midiEventHandler_bios;
        midiSysexHandler = midiSysexHandler_bios;
    }

#ifdef ENABLE_TIMER_A
    midiTimerA = (midiEventHandler == midiEventHandler_bios) ? false : true;
#else
    midiTimerA = false;
#endif
    return true;
}

void jamOnPluginStop() {
    midiUnload();
}

void jamOnLoad(uint8* songData)
{
    // same song?
    if (midi && (midi->_fd._data == songData)) {
        MD_Restart(midi);
        MD_Pause(midi, true);
        return;
    }
    // new song
    midiLoad(songData);
}

void jamOnInfo(jamSongInfo* songInfo)
{
    songInfo->isYMsong = 1;
    songInfo->songCount = 0;
    if (midi) {
        songInfo->songCount = 1;
    }
}

void jamOnPlay()
{
    MD_Restart(midi);
}

void jamOnStop() {
    if (midi) {
        MD_Pause(midi, true);
    }
}

void jamOnUpdate() {
    if (midi) {
        if (MD_isEOF(midi)) {
#if 0
            midiUnload();
            jamSongFinished();
#else
            MD_Restart(midi);
#endif
        }
    }
}

