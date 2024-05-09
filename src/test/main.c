//---------------------------------------------------------------------
//
// jam midi plugin
//
//---------------------------------------------------------------------
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "jam_sdk.h"
#include "md_midi.h"
#include "mint/osbind.h"
#include "mint/cookie.h"

// -----------------------------------------------------------------------
jamPluginInfo info = {
	JAM_INTERFACE_VERSION,		// interfaceVersion
	0x0001,						// pluginVersion
	"05.05.2024",				// date
	".MID",						// ext0
	".SMF",						// ext1
	" ",						// ext2
	" ",						// ext3
	" ",						// ext4
	" ",						// ext5
	" ",						// ext6
	" ",						// ext7
	"MIDI",						// pluginName
	"Anders Granlund",			// authorName
	"granlund23@yahoo.se",		// authorEmail
	"www.happydaze.se",			// authorUrl
	"",							// authorComment
  	0,							// isDsp					0
 	0x1F,						// support					0x1F : 00011111
  	0,							// datastart				0
  	1,							// supportsNextSongHook		0
  	0,							// supportsName				1
  	0,							// supportsComposer			1
  	1,							// supportsSongCount		1
  	1,							// supportsPreselect		1
  	0,							// supportsComment			1
  	1,							// supportsFastram			1
};


// -----------------------------------------------------------------------
MD_MIDIFile* midi;
uint32 midiTimerTargetHz;
uint32 midiTimerRealHz;
uint32 midiMicrosPerTick;


// -----------------------------------------------------------------------
#define MIDI_BIOS_ALWAYS		1
#define MIDI_BIOS_PARANOID		1


void(*midiWrite)(uint8* buf, uint16 size);

static inline void midiWrite_acia(uint8* buf, uint16 size) {
	for (short i=0; i<size; i++) {
		while(1) {
			volatile uint8 status = *((volatile uint8*)0xfffffc04);
			if (status & 2) {
				*((volatile uint8*)0xfffffc06) = *buf++;
				break;
			}
		}
	}
}

static inline void midiWrite_bios(uint8* buf, uint16 size) {
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

	// note: we have other interrupts running at this time, disable all during bios call
	#if MIDI_BIOS_PARANOID
		uint16 sr;
		__asm__ volatile ( "    move.w sr,%0" 		: "=d"(sr) : : "cc");
		__asm__ volatile ( "    or.w   #0x0700,sr" 	: : : "cc");
	#endif

	savptr -= savamt;
	for (short i=0; i<size; i++) {
		uint8 c = *buf++;
		Bconout(3, c);
	}
	savptr += savamt;

	#if MIDI_BIOS_PARANOID
		__asm__ volatile ( "    move.w %0,sr" 		: : "d"(sr) : "cc");
	#endif	
}

static inline void midiWrite_vampire(uint8* buf, uint16 size) {
	// todo
}

static inline void midiWrite_raven(uint8* buf, uint16 size) {
	// todo	
}


// -----------------------------------------------------------------------
void(*midiEventHandler)(MD_midi_event*);
void(*midiSysexHandler)(MD_sysex_event*);
void midiEventHandler_bios(MD_midi_event *pev)	{ midiWrite_bios(pev->data, pev->size); }
void midiSysexHandler_bios(MD_sysex_event *pev) { midiWrite_bios(pev->data, pev->size); }
void midiEventHandler_acia(MD_midi_event *pev)	{ midiWrite_acia(pev->data, pev->size); }
void midiSysexHandler_acia(MD_sysex_event *pev) { midiWrite_acia(pev->data, pev->size); }
void midiEventHandler_null(MD_midi_event *pev)	{ }
void midiSysexHandler_null(MD_sysex_event *pev) { }

// -----------------------------------------------------------------------
void midiUpdate() {
	if (midi) {
		MD_Update(midi, jamTimerATicks * midiMicrosPerTick);
	}
}

static void midiUnload() {
	if (midi) {
		jamUnhookTimerA(midiUpdate);
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
		midiTimerRealHz = jamHookTimerA(midiUpdate, midiTimerTargetHz);
		midiMicrosPerTick = 1000000 / midiTimerRealHz;
	}
	return midi ? true : false;
}


// -----------------------------------------------------------------------
jamPluginInfo* jamOnPluginInfo() {
	return &info;
}

bool jamOnPluginStart() {
	midi = 0;
	midiTimerTargetHz = 1000;		// todo: adjust for cpu speed?

#if MIDI_BIOS_ALWAYS
	midiEventHandler = midiEventHandler_bios;
	midiSysexHandler = midiSysexHandler_bios;

#else
	uint32 c_mch = 0;
	uint32 c_milan = 0;
	uint32 c_hades = 0;
	Getcookie(C__MCH, (long int*)&c_mch);
	bool is_t40    	= ((c_mch & 0xffff) == 0x4d34);
	bool is_milan 	= (Getcookie(C__MIL, (long int*)&c_milan) == C_FOUND);
	bool is_hades 	= (Getcookie(C_hade, (long int*)&c_hades) == C_FOUND);
	bool is_clone	= (c_mch >= 0x00040000) || is_hades || is_t40 || is_milan;
	bool is_aranym	= (c_mch == 0x00050000);
	bool is_vampire	= (c_mch == 0x00060000);

	if (is_vampire) {
		midiEventHandler = midiEventHandler_bios;
		midiSysexHandler = midiSysexHandler_bios;
	}
	else if (is_clone) {
		midiEventHandler = midiEventHandler_bios;
		midiSysexHandler = midiSysexHandler_bios;
	}
	else {
		midiEventHandler = midiEventHandler_acia;
		midiSysexHandler = midiSysexHandler_acia;
	}
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

