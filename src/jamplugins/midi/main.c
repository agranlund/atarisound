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

// TimerA or VBL playback
#define ENABLE_TIMER_A      1
#define TIMERA_HZ_FAST      1000
#define TIMERA_HZ_SLOW      200

// Midi drivers
#ifndef ENABLE_MIDI_ACIA
#define ENABLE_MIDI_ACIA    1
#endif
#ifndef ENABLE_MIDI_BIOS
#define ENABLE_MIDI_BIOS    1
#endif
#ifndef ENABLE_MIDI_ISA
#define ENABLE_MIDI_ISA     0
#endif


// -----------------------------------------------------------------------
static MD_MIDIFile* midi;
static void (*midiWrite)(uint8* buf, uint16 size);
static uint32 midiTimerTargetHz;
static uint32 midiTimerRealHz;
static uint32 midiMicrosPerTick;
static bool midiTimerA;
static uint16 savBuf[23*2*12];

// -----------------------------------------------------------------------

static void midiWrite_null(uint8* buf, uint16 size) {
}

#if ENABLE_MIDI_ACIA
static void midiWrite_acia(uint8* buf, uint16 size) {
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
#endif

// bios
#if ENABLE_MIDI_BIOS
static int32 (*biosMidiOut)(uint32 d) = 0;
static void midiWrite_bios(uint8* buf, uint16 size) {
    uint8* end = &buf[size];
    while (buf != end) {
        int16 c = (int8)*buf++;
        biosMidiOut((3<<16) | c);
    }
}
#endif

// isa
#if ENABLE_MIDI_ISA

typedef struct
{
    unsigned short  version;
    unsigned int    iobase;
    unsigned int    membase;
    unsigned short  irqmask;
    unsigned char   drqmask;
    unsigned char   endian;

    void            (*outp)(unsigned short port, unsigned char data);
    void            (*outpw)(unsigned short port, unsigned short data);
    unsigned char   (*inp)(unsigned short port);
    unsigned short  (*inpw)(unsigned short addr);
} isa_t;

static isa_t* isa = 0;

static inline isa_t* isaInit() {
    #define C__ISA  0x5F495341 /* '_ISA' */
    return (Getcookie(C__ISA, (long*)&isa) == C_FOUND) ? isa : 0;
}

unsigned short mpu401_port = 0x330;
static inline bool mpu401_wait_stat(uint8 mask) {
    int32 timeout = 100000;
    while(timeout) {
        if ((isa->inp(mpu401_port+1) & mask) == 0) {
            return true;
        }
        timeout--;
    }
    return false;
}
static inline bool mpu401_wait_ack() {
    while(1) {
        if (!mpu401_wait_stat(0x80)) {
            return false;
        }
        if (isa->inp(mpu401_port+0) == 0xFE) {
            return true;
        }
    }
}
static inline bool mpu401_out(unsigned short p, unsigned char v) {
    if (mpu401_wait_stat(0x40)) {
        isa->outp(p, v);
        return true;
    }
    return false;
}

static bool midiOpen_isa() {
    if (isaInit()) {
        mpu401_port = 0x330;
        mpu401_out(mpu401_port+1, 0xff);    // reset
        mpu401_wait_ack();                  // ack
        mpu401_out(mpu401_port+1, 0x3f);    // uart mode
        return true;
    }
    return false;
}

static void midiWrite_isa(uint8* buf, uint16 size) {
    for (uint16 i=0; i<size; i++) {
        if (!mpu401_out(mpu401_port, buf[i])) {
            break;
        }
    }
}
#endif

// -----------------------------------------------------------------------

void midiEventHandler(MD_midi_event *pev) {
    midiWrite(pev->data, pev->size);
}
void midiSysexHandler(MD_sysex_event *pev) {
    midiWrite(pev->data, pev->size);
}

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
    midiWrite = 0;

    // pick a timer frequency based on cpu model
    uint32 cookie = 0;
    midiTimerTargetHz = TIMERA_HZ_SLOW;
    if ((Getcookie(C__CPU, (long*)&cookie) == C_FOUND) && (cookie >= 30)) {
        midiTimerTargetHz = TIMERA_HZ_FAST;
    }

    // pick machine dependent output routine
    uint32 c_mch = 0;
    Getcookie(C__MCH, (long int*)&c_mch);
    bool is_t40 = ((c_mch & 0xffff) == 0x4d34);
    bool is_clone = (c_mch >= 0x00040000) || is_t40;
    //midiTimerA = !is_clone && (bool)ENABLE_TIMER_A;
    midiTimerA = (bool)ENABLE_TIMER_A;

#if ENABLE_MIDI_ISA
    if ((midiWrite == 0) && midiOpen_isa()) {
        midiWrite = midiWrite_isa;
        return true;
    }
#endif

#if ENABLE_MIDI_ACIA
    if ((midiWrite == 0) && !is_clone) {
        midiWrite = midiWrite_acia;
        return true;
    }
#endif

#if ENABLE_MIDI_BIOS
    if (midiWrite == 0) {
        biosMidiOut = (int32(*)(uint32)) *(volatile uint32*)(0x57e + (3 * 4));
        midiWrite = midiWrite_bios;
        return true;
    }
#endif

    midiWrite = midiWrite_null;
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

