//------------------------------------------------------------------------------
// mxPlay Midi plugin
// 2024, anders.granlund
//------------------------------------------------------------------------------
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
//------------------------------------------------------------------------------
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "time.h"
#include "mint/osbind.h"
#include "mint/cookie.h"
#include "md_midi.h"
#include "plugin.h"


// -----------------------------------------------------------------------

// Timer frequency
#define TIMERA_HZ_FAST     1000
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
static MD_MIDIFile* midi = 0;
static void (*midiWrite)(uint8* buf, uint16 size) = 0;
static uint32 midiTimerTargetHz;
static uint32 midiTimerRealHz;
static uint32 midiMicrosPerTick;
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
static unsigned short mpu401_port = 0x330;
static inline bool mpu401_wait_stat(uint8 mask) {
    int32 timeout = 100000;
    while(timeout) {
        if ((inp(mpu401_port+1) & mask) == 0) {
            return true;
        }
        mxDelay(5);
        timeout--;
    }
    return false;
}
static inline bool mpu401_wait_ack() {
    while(1) {
        if (!mpu401_wait_stat(0x80)) {
            return false;
        }
        if (inp(mpu401_port+0) == 0xFE) {
            return true;
        }
    }
}
static inline bool mpu401_out(unsigned short p, unsigned char v) {
    if (mpu401_wait_stat(0x40)) {
        outp(p, v);
        return true;
    }
    return false;
}

static bool midiOpen_isa() {
    mxCalibrateDelay();
    if (mxIsaInit()) {
        mpu401_port = mxIsaPort("PNPB006", 0, 0x330);
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


void midiEventHandler(MD_midi_event *pev) {
    midiWrite(pev->data, pev->size);
}
void midiSysexHandler(MD_sysex_event *pev) {
    midiWrite(pev->data, pev->size);
}

static void midiUpdate_timerA() {
    if (midi) {
        MD_Update(midi, mxTimerATicks * midiMicrosPerTick);
    }
}

static void midiUnload() {
    if (midi) {
        mxUnhookTimerA();
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
    }
    return midi ? true : false;
}


static bool pluginInit() {
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


// -----------------------------------------------------------------------
//
// mxPlay
//
// -----------------------------------------------------------------------
#ifdef PLUGIN_MXP

extern struct SAudioPlugin mx_plugin;

const struct SInfo mx_info =
{
	"Anders Granlund",
	"1.0",
	"MD_MIDIFile",
	"Marco Colli",
	"1.0",
    MXP_FLG_XBIOS
};

const struct SExtension mx_extensions[] = {
	{ "MID", "Midi" },
	{ "SMF", "Midi" },
	{ NULL, NULL }
};

const struct SParameter mx_settings[] = {
    { NULL, 0, NULL, NULL }
};


int mx_init() {
    return pluginInit() ? MXP_OK : MXP_ERROR;
}

int mx_register_module() {
    uint8* data = (uint8*) mx_plugin.inBuffer.pModule->p;
    size_t size = mx_plugin.inBuffer.pModule->size;
    return midiLoad(data) ? MXP_OK : MXP_ERROR;
}

int mx_unregister_module() {
    midiUnload();
    return MXP_OK;
}

int mx_get_songs() {
    mx_plugin.inBuffer.value = 1;
    return MXP_OK;
}

int mx_set() {
    if (midi) {
        midiTimerRealHz = mxHookTimerA(midiUpdate_timerA, midiTimerTargetHz);
        midiMicrosPerTick = 1000000 / midiTimerRealHz;
        MD_Restart(midi);
        return MXP_OK;
    }
    return MXP_ERROR;
}

int mx_unset() {
    if (midi) {
        MD_Pause(midi, true);
        mxUnhookTimerA();
        return MXP_OK;
    }
    return MXP_ERROR;
}

int mx_feed() {
    // stop song when reaching end of file
    if (midi && MD_isEOF(midi)) {
        MD_Pause(midi, 1);
        return MXP_ERROR;
    }
    return MXP_OK;
}

int mx_pause() {
    if (midi) {
        MD_Pause(midi, mx_plugin.inBuffer.value ? true : false);
        return MXP_OK;
    }
    return MXP_ERROR;
}

int mx_mute() {
    return MXP_ERROR;
}

int mx_get_playtime() {
    // we don't know the song length up front so just set a very high number
    mx_plugin.inBuffer.value = 60 * 60 * 1000;
    return MXP_OK;
}

#endif //PLUGIN_MXP




// -----------------------------------------------------------------------
//
// Jam
//
// -----------------------------------------------------------------------
#ifdef PLUGIN_JAM


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

jamPluginInfo* jamOnPluginInfo() {
    return &info;
}

bool jamOnPluginStart() {
    return pluginInit();
}

void jamOnPluginStop() {
    midiUnload();
}

void jamOnLoad(uint8* songData) {
    midiLoad(songData);
}

void jamOnInfo(jamSongInfo* songInfo) {
    songInfo->isYMsong = 1;
    songInfo->songCount = 0;
    if (midi) {
        songInfo->songCount = 1;
    }
}

void jamOnPlay() {
    if (midi) {
        midiTimerRealHz = mxHookTimerA(midiUpdate_timerA, midiTimerTargetHz);
        midiMicrosPerTick = 1000000 / midiTimerRealHz;
        MD_Restart(midi);
    }
}

void jamOnStop() {
    if (midi) {
        MD_Pause(midi, true);
        mxUnhookTimerA();
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




#endif
