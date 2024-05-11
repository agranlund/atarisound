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
#define ENABLE_TIMER_A      1
#define TIMER_A_HZ          1000

// Enable direct hardware access for known machines
#define ENABLE_DIRECT_MIDI  1

// Enable midi-out through bios
#define ENABLE_BIOS_MIDI    1



// -----------------------------------------------------------------------
jamPluginInfo info = {
    JAM_INTERFACE_VERSION,      // interfaceVersion
    0x0004,                     // pluginVersion
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

void (*midiWrite)(uint8* buf, uint16 size);
int32 (*biosMidiOut)(uint32 d);

uint32 midiTimerTargetHz;
uint32 midiTimerRealHz;
uint32 midiMicrosPerTick;
bool midiTimerA;
uint16 savBuf[23*2*12];

// -----------------------------------------------------------------------
static inline void midiWrite_acia(uint8* buf, uint16 size) {
    for (short i=0; i<size; i++) {
        while(1) {
            volatile uint8 status = *((volatile uint8*)(0xfffffc04UL));
            if (status & 2) {
                *((volatile uint8*)(0xfffffc06UL)) = *buf++;
                break;
            }
        }
    }
}

static inline void midiWrite_bios(uint8* buf, uint16 size) {
#if ENABLE_BIOS_MIDI

#if 1
    // safe in both TOS and MiNT
    uint8* end = &buf[size];
    while (buf != end) {
        int16 c = (int8)*buf++;
        biosMidiOut((3<<16) | c);
    }
#else
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

    //
    // !! NOTE: This does not work under MiNT. Its bconout implementation does a
    // of things not suitable for being called from interrupts.
    //

    if (size < 1)
        return;

    uint16 sr = jamDisableInterrupts();
    #define savptr *((volatile uint32*)0x4a2)
    #define savamt (23 * 2)
    #if 0
        savptr -= savamt;
    #else    
        uint32 oldsav = savptr;
        savptr = (uint32)&savBuf[savamt*3];
    #endif
    Midiws(size - 1, buf);
    #if 0
        savptr += savamt;
    #else
        savptr = oldsav;
    #endif
    jamRestoreInterrupts(sr);
#endif
#endif // ENABLE_BIOS_MIDI
}


// -----------------------------------------------------------------------
void(*midiEventHandler)(MD_midi_event*);
void(*midiSysexHandler)(MD_sysex_event*);

void midiEventHandler_acia(MD_midi_event *pev)      { midiWrite_acia(pev->data, pev->size); }
void midiSysexHandler_acia(MD_sysex_event *pev)     { midiWrite_acia(pev->data, pev->size); }
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
        uint32 timerCTicks = *((volatile uint32*)0x4ba);
        MD_Update(midi, timerCTicks * midiMicrosPerTick);
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
    biosMidiOut = (int32(*)(uint32)) *(volatile uint32*)(0x57e + (3 * 4));

    // todo: adjust for cpu speed?
    midiTimerA = (bool)ENABLE_TIMER_A;
    midiTimerTargetHz = TIMER_A_HZ;

    // machine dependent output routine
    uint32 c_mch = 0;
    Getcookie(C__MCH, (long int*)&c_mch);
    bool is_t40 = ((c_mch & 0xffff) == 0x4d34);
    bool is_clone = (c_mch >= 0x00040000) || is_t40;
    if (!is_clone && ENABLE_DIRECT_MIDI) {
        midiEventHandler = midiEventHandler_acia;
        midiSysexHandler = midiSysexHandler_acia;
    }
    else {
        midiEventHandler = midiEventHandler_bios;
        midiSysexHandler = midiSysexHandler_bios;
        midiTimerA = false;
    }

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

