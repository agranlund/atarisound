///////////////////////////////////////////////////////////////////////////////
// __      _______ __  __  _____ _             _ 
// \ \    / / ____|  \/  |/ ____| |           | |
//  \ \  / / |  __| \  / | (___ | | __ _ _ __ | |
//   \ \/ /| | |_ | |\/| |\___ \| |/ _` | '_ \| |   by Wafflenet, 2023-2024
//    \  / | |__| | |  | |____) | | (_| | |_) |_|       www.wafflenet.com
//     \/   \_____|_|  |_|_____/|_|\__,_| .__/(_)
//      (VGM Silly Little AdLib Player) | |      
//                                      |_|      
//
///////////////////////////////////////////////////////////////////////////////
//
// !! This file has been modified from its original implementation
//
///////////////////////////////////////////////////////////////////////////////


///////////////////////////////////////////////////////////////////////////////
// Standard includes
///////////////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <string.h>
#include <ext.h>
#include "../mxplay_utils.h"

///////////////////////////////////////////////////////////////////////////////
// Type definitions
///////////////////////////////////////////////////////////////////////////////

// Use these where possible to reduce bit/signing confusion
// Example, recall that "char" is default unsigned, but "int" is default signed

typedef unsigned char   uint8_t;
typedef unsigned short  uint16_t;
typedef unsigned long   uint32_t;
typedef short           wchar_t;

#define err_ok          0
#define err_init        0xff
#define err_load        0xfe


#define prgstate_null       0
#define prgstate_stopped    1
#define prgstate_playing    2
#define prgstate_paused     3
#define prgstate_done       4


#if 0
#define dbgprintf(...)       printf(__VA_ARGS__)
#else
#define dbgprintf(...) {}
#endif


///////////////////////////////////////////////////////////////////////////////
// Helper macros
///////////////////////////////////////////////////////////////////////////////

static inline uint16_t getUint16(void* ptr) {
    uint16_t r = *((uint16_t*)ptr);
    return ((r >> 8 ) | (r << 8));
}

static inline uint32_t getUint32(void* ptr) {
    uint32_t r = *((uint32_t*)ptr);
    return  ((r & 0xff000000) >> 24) |
            ((r & 0x00ff0000) >>  8) |
            ((r & 0x0000ff00) <<  8) |
            ((r & 0x000000ff) << 24);
}


///////////////////////////////////////////////////////////////////////////////
// Defines and function prototypes
///////////////////////////////////////////////////////////////////////////////

#define VGMSLAP_VERSION "R2"

// Macro definitions

// Default setting definitions
#define CONFIG_DEFAULT_PORT         0x388
#define CONFIG_DEFAULT_LOOPS        255
#define CONFIG_DEFAULT_DIVIDER      20
#define CONFIG_DEFAULT_STRUGGLE     0

// OPL control functions
uint8_t detectOPL(void);
void writeOPL(uint16_t, uint8_t);
void resetOPL(void);

// Timing functions
void timerHandler(void);
void initTimer(uint16_t);
void resetTimer (void);

// VGM parsing functions
uint8_t getNextCommandData(void);
wchar_t* getNextGd3String(void);
uint8_t loadVGM(uint8_t* buf);
void populateCurrentGd3(void);
void processCommands(void);

///////////////////////////////////////////////////////////////////////////////
// Global variable definitions
///////////////////////////////////////////////////////////////////////////////

#define OPL2_DELAY_REG      12      // 6
#define OPL2_DELAY_DAT      64      // 35

#define OPL3_DELAY_REG      4
#define OPL3_DELAY_DAT      4


// General program vars
static uint8_t programState = prgstate_null;				// Controls current state of program (init, main loop, exit, etc)
										// 0 = init, 1 = play, 2 = songEnd, 3 = quit
// File-related vars
static uint8_t vgmIdentifier[] = "Vgm ";			// VGM magic number
static uint8_t gzMagicNumber[2] = {0x1F, 0x8B}; 	// GZ magic number
static uint8_t* vgmFileData;
static uint8_t* vgmFileBuffer;
static uint32_t fileCursorLocation = 0; 		// Stores where we are in the file.
										// It's tracked manually to avoid expensive ftell calls when doing comparisons (for loops)

// OPL-related vars
static uint32_t isaBaseAddr;
static uint16_t oplBaseAddr;				// Base port for OPL synth.
static uint8_t detectedChip;				// What OPL chip we detect on the system
static uint8_t oplDelayReg = OPL2_DELAY_REG;			// Delay required for OPL register write (set for OPL2 by default)
static uint8_t oplDelayData = OPL2_DELAY_DAT;			// Delay required for OPL data write (set for OPL2 by default)
static uint8_t oplRegisterMap[0x1FF];			// Stores current state of OPL registers
static uint8_t oplChangeMap[0x1FF];			// Written alongside oplRegisterMap, tracks bytes that need interpreted/drawn
static uint8_t commandReg = 0; 			// Stores current OPL register to manipulate
static uint8_t commandData = 0; 			// Stores current data to put in OPL register
static uint8_t maxChannels = 9;			// When iterating channels, how many to go through (9 for OPL2, 18 for OPL3)

// Due to weird operator offsets to form a channel, this is a list of offsets from the base (0x20/0x40/0x60/0x80/0xE0) for each.  First half is OPL2 and second is OPL3, so OPL3 ones have 0x100 added to fit our data model.
// Original op order numbers: {0, 3, 1, 4, 2, 5, 6, 9, 7, 10, 8, 11, 12, 15, 13, 16, 14, 17, 18, 21, 19, 22, 20, 23, 24, 27, 25, 28, 26, 29, 30, 33, 31, 34, 32, 35};
const uint16_t oplOperatorOrder[] = { 			
	0x00, 0x03,    // Channel 1 (OPL2)
	0x01, 0x04,    // Channel 2
	0x02, 0x05,    // Channel 3
	0x08, 0x0B,    // Channel 4
	0x09, 0x0C,    // Channel 5
	0x0A, 0x0D,    // Channel 6
	0x10, 0x13,    // Channel 7
	0x11, 0x14,    // Channel 8
	0x12, 0x15,    // Channel 9
	0x100, 0x103,  // Channel 10 (OPL3)
	0x101, 0x104,  // Channel 11
	0x102, 0x105,  // Channel 12
	0x108, 0x10B,  // Channel 13
	0x109, 0x10C,  // Channel 14
	0x10A, 0x10D,  // Channel 15
	0x110, 0x113,  // Channel 16
	0x111, 0x114,  // Channel 17
	0x112, 0x115}; // Channel 18

// Offsets from base of a register category (for instance Attack/Decay at 0x60) and how it maps to a channel/op.
// OPL2 only, add 9 to channel number for OPL3
const uint8_t oplOperatorToChannel[] = {
0, 1, 2, 0, 1, 2, 0, 0, 3, 4, 5, 3, 4, 5, 0, 0, 6, 7, 8, 6, 7, 8
};
const uint8_t oplOffsetToOperator[] = {
0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1, 0, 0, 0, 0, 0, 1, 1, 1
};

// Timing-related vars
//void interrupt (*biosISR8)(void);			// Pointer to the BIOS interrupt service routine 8
volatile uint32_t tickCounter; 			// Counts the number of timer ticks elapsed
static uint16_t biosCounter = 0;					// Used to determine when to run the original BIOS ISR8
static uint32_t fastTickRate = 0;						// Divider to apply to the 8253 PIT
static uint8_t requestScreenDraw;					// Set to 1 when the screen needs to redraw
static uint16_t playbackFrequency = 44100;	        // Playback frequency (VGM files are set to 44100 Hz)
static uint8_t playbackFrequencyDivider = 1; 	    // playbackFrequency / playbackFrequencyDivider = timer hz
static uint32_t dataCurrentSample = 0;				// VGM sample we are on in the file

// VGM-related vars
static uint8_t commandID = 0; 		// Stores most recent VGM command interpreted
static uint8_t loopCount = 0; 		// Tracks what loop we are on during playback
static uint8_t loopMax = 1;		// How many times to loop
static uint8_t vgmChipType = 0; 	// What chip configuration has been determined from the VGM file
							// We have to deal with all permutations that a PC could theoretically play
							// 0 = No OPLs found
							// 1 = OPL1
							// 2 = OPL2
							// 3 = OPL3
							// 4 = 2xOPL1 (SBPro1/PAS only without in-flight modifications)
							// 5 = 2xOPL2 (SBPro1/PAS only without in-flight modifications)
							// 6 = OPL1+OPL2? (SBPro1/PAS only without in-flight modifications)idk, would someone do this??
							// 7 = Dual OPL3? (could you do this with two cards at different IO ports?  I've got no way of testing that!  YET!

///////////////////////////////////////////////////////////////////////////////
// Structs
///////////////////////////////////////////////////////////////////////////////

// Program settings struct
typedef struct
{
	uint16_t oplBase;
	uint8_t loopCount;
	uint8_t frequencyDivider; // Range should be 1-100
	uint8_t struggleBus;
} programSettings;

// VGM header struct
// Only uses the values we care about, not the entire header
typedef struct
{
	uint32_t fileIdentification;
	uint32_t eofOffset; 		// Relative offset from 0x04!
	uint32_t versionNumber;
	uint32_t gd3Offset;			// Relative offset from 0x14!
	uint32_t totalSamples;
	uint32_t loopOffset; 		// Relative offset from 0x1C!
	uint32_t loopNumSamples;	// Todo: Apply to loop count
	uint32_t recordingRate; 	// Could be used as a divider to cut down on the timing accuracy?
	uint32_t vgmDataOffset; 	// Relative offset from 0x34!
	uint32_t ym3812Clock; 		// Reminder: Dual chip is set by adding 0x40000000
	uint32_t ym3526Clock; 		// Should be able to convert dual OPL1 to dual OPL2
	uint32_t ymf262Clock; 		// Dual chip is like above, but there are no dual OPL3 cards on PC :)
	uint8_t loopBase;
	uint8_t loopModifier;
} vgmHeader;

// GD3 tag struct
typedef struct
{
	uint32_t tagLength;
	wchar_t* trackNameE;
	wchar_t* trackNameJ;
	wchar_t* gameNameE;
	wchar_t* gameNameJ;
	wchar_t* systemNameE;
	wchar_t* systemNameJ;
	wchar_t* originalAuthorE;
	wchar_t* originalAuthorJ;
	wchar_t* releaseDate;
	wchar_t* converter;
	wchar_t* notes;
} gd3Tag;

// Individual OPL operator struct, which are nested in an OPL channel struct
typedef struct
{
	uint8_t attackRate;
	uint8_t decayRate;
	uint8_t sustainLevel;
	uint8_t releaseRate;
	uint8_t flagTremolo;
	uint8_t flagFrequencyVibrato;
	uint8_t flagSoundSustaining;
	uint8_t flagKSR;
	uint8_t frequencyMultiplierFactor;
	uint8_t waveform;
	uint8_t keyScaleLevel;
	uint8_t outputLevel;
} oplOperator;

// Individual OPL channel struct, which are nested in an OPL chip struct
typedef struct
{
	oplOperator operators[2];
	uint16_t frequencyNumber;
	uint8_t blockNumber;
	uint8_t keyOn;
	uint8_t flag4Op;
	uint8_t panning; // Two bits - (0x03 = both channels, 0x02 = right, 0x01 = left, 0x00 = neither)
	uint8_t feedback;
	uint8_t synthesisType; 	// Aka "Algorithm"
	uint8_t displayX;		// Channel block location on screen
	uint8_t displayY;		// Channel block location on screen
} oplChannel;

// OPL chip struct
typedef struct
{
	oplChannel channels[18];
	uint8_t flagOPL3Mode;
	uint8_t tremoloDepth;
	uint8_t frequencyVibratoDepth;
	uint8_t percModeData;
} oplChip;

// Storage spot for program settings
static programSettings settings;

// Storage spot for split-out VGM header data
static vgmHeader currentVGMHeader;

// Storage spot for split-out GD3 tag data
static gd3Tag currentGD3Tag;

// Storage spot for interpreted OPL status
static oplChip oplStatus;


///////////////////////////////////////////////////////////////////////////////
// Misc Functions
///////////////////////////////////////////////////////////////////////////////
/*
static inline uint8_t inp(uint16_t port) {
    uint8_t data = *((volatile uint8*)(isaBaseAddr + port));
    return data;
}
static inline void outp(uint16_t port, uint8_t data) {
    *((volatile uint8*)(isaBaseAddr + port)) = data;
}
*/

static inline uint8_t readBytes(uint16_t numBytes)
{
    vgmFileBuffer = &vgmFileData[fileCursorLocation];
	fileCursorLocation = fileCursorLocation + numBytes;
	return 0;
}

static inline uint32_t seekSet(uint32_t pos)
{
    fileCursorLocation = pos;
    return fileCursorLocation;
}

static uint8_t killProgram(uint8_t errorCode)
{
    programState = prgstate_null;
	if (detectedChip != 0) {
		resetOPL();
	}
    return errorCode;
}

static inline void endSong()
{
    // this is called from within the interrupt so we can't mess with
    // the timer as it implies xbios calls inside jamLib
    //resetTimer();
    programState = prgstate_done;
    resetOPL();
}


///////////////////////////////////////////////////////////////////////////////
// Main Functions
///////////////////////////////////////////////////////////////////////////////

void vgmslap_info(
    uint8_t*    chipType,
    uint32_t*   songLength,
    wchar_t**   songName,
    wchar_t**   songAuthor)
{
    if (chipType) {
        *chipType = vgmChipType;
    }
    if (songLength) {
        *songLength = currentGD3Tag.tagLength;
    }
    if (songName) {
        *songName = currentGD3Tag.trackNameE;
    }
    if (songAuthor) {
        *songAuthor = currentGD3Tag.originalAuthorE;
    }
}


void vgmslap_pause(uint8_t pause)
{
    if (pause && (programState == prgstate_playing)) {
        resetTimer();
        resetOPL();
    } else if (!pause && (programState == prgstate_paused)) {
        resetOPL();
        initTimer(playbackFrequency);
    }
}


void vgmslap_play()
{
    dbgprintf("play : %d\r\n", programState);
    if (programState == prgstate_stopped) {
	    resetOPL();
        seekSet(currentVGMHeader.vgmDataOffset+0x34);
    	delay(100);
        if (detectedChip == 3 && vgmChipType == 5) {
            writeOPL(0x105,0x01);
        }
        tickCounter = 0;
        programState = prgstate_playing;
        initTimer(playbackFrequency);
    }
}

void vgmslap_stop()
{
    dbgprintf("stop : %d\r\n", programState);
    if (programState == prgstate_playing) {
        resetTimer();
        resetOPL();
    }
    programState = prgstate_stopped;
}

uint8_t vgmslap_load(uint8_t* buf)
{
    dbgprintf("load : %d\r\n", programState);
    vgmslap_stop();
    programState = prgstate_null;
    if (buf == 0) {
        return err_ok;
    }

    uint8 load_result = loadVGM(buf);
    if (load_result != err_ok) {
        return load_result;
    }

    programState = prgstate_stopped;
    return err_ok;
}

uint8_t vgmslap_init(uint32 isa_base, uint16 opl_base)
{
	dbgprintf("VGMSlap! %s by Wafflenet\n", VGMSLAP_VERSION);

	settings.oplBase = CONFIG_DEFAULT_PORT;
	settings.frequencyDivider = CONFIG_DEFAULT_DIVIDER;
	settings.loopCount = CONFIG_DEFAULT_LOOPS;
	settings.struggleBus = CONFIG_DEFAULT_STRUGGLE;
    if (settings.oplBase)
        settings.oplBase = opl_base;

    isaBaseAddr = isa_base;
	oplBaseAddr = settings.oplBase;

	playbackFrequencyDivider = settings.frequencyDivider;
	loopMax = settings.loopCount;
	
    uint8_t detect_result = detectOPL();
    return detect_result;
}


///////////////////////////////////////////////////////////////////////////////
// Timer Functions
///////////////////////////////////////////////////////////////////////////////

void timerHandler(void)
{
    if (programState == prgstate_playing) {
        tickCounter = tickCounter + playbackFrequencyDivider;
        //uint16 sr = jamDisableInterrupts();
        processCommands();
    }
}

void initTimer(uint16_t frequency)
{
    dbgprintf("inittimer\r\n");
    uint32_t hz = playbackFrequency / playbackFrequencyDivider;
    uint32_t hz_real = mxHookTimerA(timerHandler, hz);
    dbgprintf("inittimer done\r\n");
    // todo: set tick stepper based on hz_real
}

void resetTimer(void)
{
    mxUnhookTimerA();
}


///////////////////////////////////////////////////////////////////////////////
// OPL Control Functions
///////////////////////////////////////////////////////////////////////////////

void delayOPL(uint8_t count) {
    // todo: replace this with delay() or usleep()

    // original MS-DOS comment:
    // The OPL requires a minimum time between writes.
    // We can execute inp a certain number of times to ensure enough time has passed - why does that work?
    // Because the time it takes to complete an inp is based on the ISA bus timings!
    while(count--) {
        inp(0x80);
    }
}

// Send data to the OPL chip
void writeOPL(uint16_t reg, uint8_t data)
{
    // Second OPL2 and/or OPL3 secondary register set
    if (reg >= 0x100)
    {
        // First write to target register... 
        outp(oplBaseAddr+2, (reg - 0x100));
        delayOPL(oplDelayReg);
        
        // ...then go to +1 for the data
        outp(oplBaseAddr+3, data);
        delayOPL(oplDelayData);
    }
    // OPL2 and/or OPL3 primary register set
    else
    {
        // First write to target register... 
        outp(oplBaseAddr, reg);
        delayOPL(oplDelayReg);
        
        // ...then go to Base+1 for the data
        outp(oplBaseAddr+1, data);
        delayOPL(oplDelayData);
    }
    
    // Write the same data to our "register map", used for visualizing the OPL state, as well as the change map to denote that this bit needs to be interpreted and potentially drawn.
    oplRegisterMap[reg] = data;
    oplChangeMap[reg] = 1;
	
}

// Reset OPL to original state, including turning off OPL3 mode
void resetOPL(void)
{
    // Resetting the OPL has to be somewhat systematic - otherwise you run into issues with static sounds, squeaking, etc, not only when cutting off the sound but also when the sound starts back up again.
    
    uint16_t i;
    
    // For OPL3, turn on the NEW bit.  This ensures we can write to ALL registers on an OPL3.
    if (detectedChip == 3)
    {
        writeOPL(0x105,0x01);
    }
    
    // Clear out the channel level information
    
    // OPL2
    for (i=0; i<9; i++)
    {
        writeOPL(0xA0+i, 0x00); // Frequency number (LSB)
        writeOPL(0xB0+i, 0x00); // Key-On + Block + Frequency (MSB)
        writeOPL(0xC0+i, 0x30); // Panning, Feedback, Synthesis Type
    }
    // Dual OPL2 / OPL3
    if (detectedChip > 1)
    {
        for (i=0; i<9; i++)
        {
            writeOPL(0x1A0+i, 0x00); // Frequency number (LSB)
            writeOPL(0x1B0+i, 0x00); // Key-On + Block + Frequency (MSB)
            writeOPL(0x1C0+i, 0x30); // Panning, Feedback, Synthesis Type
        }
    }
    
    // Clear out the operators one by one.
    // Why do it this way instead of just writing 0's in order?
    // That would mean that we go through each operator property, by operator, in order, putting a gap between the changes for an operator that can manifest as an audio glitch or "click"
    // Doing it this way we don't get that clicking when we kill the audio.
    
    // OPL2
    for (i=0; i<18; i++)
    {
        writeOPL(0x20+oplOperatorOrder[i], 0x00);
        writeOPL(0x40+oplOperatorOrder[i], 0x3F); // Output attenuation is set to max
        writeOPL(0x60+oplOperatorOrder[i], 0xFF);
        writeOPL(0x80+oplOperatorOrder[i], 0xFF);
        writeOPL(0xE0+oplOperatorOrder[i], 0x00);
    }
    // Dual OPL2 / OPL3
    if (detectedChip > 1)
    {
        for (i=18; i<36; i++)
        {
            writeOPL(0x20+oplOperatorOrder[i], 0x00);
            writeOPL(0x40+oplOperatorOrder[i], 0x3F); // Output attenuation is set to max
            writeOPL(0x60+oplOperatorOrder[i], 0xFF);
            writeOPL(0x80+oplOperatorOrder[i], 0xFF);
            writeOPL(0xE0+oplOperatorOrder[i], 0x00);
        }
    }
    
    // Clear out percussion mode register
    writeOPL(0xBD,0x00);
    if (detectedChip > 1)
    {
        writeOPL(0x1BD,0x00);
    }
    
    // Return to the ADSR and set them to zero - we set them to F earlier to force a note decay
    // We also set the output attenuation AGAIN, this time to 0 (no attenuation) as some poorly coded playback engines just expect the OPL to already have 0 there
    // OPL2
    for (i=0; i<18; i++)
    {
        writeOPL(0x60+oplOperatorOrder[i], 0x00);
        writeOPL(0x80+oplOperatorOrder[i], 0x00);
        writeOPL(0x40+oplOperatorOrder[i], 0x00);
    }
    // Dual OPL2 / OPL3
    if (detectedChip > 1)
    {
        for (i=18; i<36; i++)
        {
            writeOPL(0x60+oplOperatorOrder[i], 0x00);
            writeOPL(0x80+oplOperatorOrder[i], 0x00);
            writeOPL(0x40+oplOperatorOrder[i], 0x00);
        }
    }
    
    // Finally clear the initial registers
    // OPL2 regs
    for (i = 0x00; i < 0x20; i++)
    {
        writeOPL(i,0x00);
    }
    
    // If Dual OPL2 just clear it like an OPL2
    if (detectedChip == 2)
    {
        for (i = 0x100; i < 0x120; i++)
        {
            writeOPL(i,0x00);
        }
    }
    
    // If OPL3, change plans...
    if (detectedChip == 3)
    {
        // OPL3 regs - works a bit differently.  We don't turn off 4-op mode until we have zeroed everything else out, and we must touch 0x105 (OPL3 enable / "NEW" bit) ABSOLUTELY LAST or our writes to OPL3 features will be completely ignored!  (Yes, that includes zeroing them out!!)
        for (i = 0x100; i <= 0x103; i++)
        {
            writeOPL(i,0x00);
        }
        for (i = 0x106; i < 0x120; i++)
        {
            writeOPL(i,0x00);
        }
        // For OPL3, turn off 4-Op mode (if it was on)
        writeOPL(0x104,0x00);
        // For OPL3, write the NEW bit back to 0.  We're now back in OPL2 mode.
        // VGMs should have their own write to this bit to re-enable it for OPL3 songs.
        writeOPL(0x105,0x00);
    }
}

// Detect what OPL chip is in the computer.  This determines what VGMs can be played.
uint8_t detectOPL(void)
{
	uint8_t statusRegisterResult1;
	uint8_t statusRegisterResult2;
	
	// Start with assuming nothing is detected
	detectedChip = 0;
	
	// Detect OPL2
	
	// Reset timer 1 and timer 2
	writeOPL(0x04, 0x60);
	// Reset IRQ
	writeOPL(0x04, 0x80);
	// Read status register
	statusRegisterResult1 = inp(oplBaseAddr);
	// Set timer 1
	writeOPL(0x02, 0xFF);
	// Unmask and start timer 1
	writeOPL(0x04, 0x21);
	// Wait at least 80 usec (0.08ms) - a 2ms delay should be enough
	delay(100);
	// Read status register
	statusRegisterResult2 = inp(oplBaseAddr);
	// Reset timer 1, timer 2, and IRQ again
	writeOPL(0x04, 0x60);
	writeOPL(0x04, 0x80);
	// Compare results of the status register reads.  First should be 0x00, second should be 0xC0.  Results are AND with 0xE0 because of unused bits in the chip
	if ((statusRegisterResult1 & 0xE0) == 0x00 && (statusRegisterResult2 & 0xE0) == 0xC0)
	{
		// OPL2 detected
		detectedChip = 1;
		// Continue to try detecting OPL3
		if ((statusRegisterResult2 & 0x06) == 0x00)
		{
			detectedChip = 3;
		}
		// If no OPL3, try detecting second OPL2 at OPL3 registers.
		// Dual OPL2 usually is at 220h/222h (left/right) and writes to 228 will go to both OPLs
		// Note that this detection works in Dosbox, but not in 86Box with Nuked.  It does work in 86box with YMFM.  Need to test on a real SB Pro 1 which I don't have.
		else
		{
			writeOPL(0x104, 0x60);
			writeOPL(0x104, 0x80);
			statusRegisterResult1 = inp(oplBaseAddr+2);
			writeOPL(0x102, 0xFF);
			writeOPL(0x104, 0x21);
			delay(10);
			statusRegisterResult2 = inp(oplBaseAddr+2);
			writeOPL(0x104, 0x60);
			writeOPL(0x104, 0x80);
			if ((statusRegisterResult1 & 0xE0) == 0x00 && (statusRegisterResult2 & 0xE0) == 0xC0)
			{
				detectedChip = 2;
			}
		}
	}
	// Set delays, print results
	switch (detectedChip)
	{
		case 0:
			return killProgram(7);
			break;
		case 1:
			oplDelayReg = OPL2_DELAY_REG;
			oplDelayData = OPL2_DELAY_DAT;
			dbgprintf("OPL2 detected at %Xh!\n", settings.oplBase);
			break;
		case 2:
			oplDelayReg = OPL2_DELAY_REG;
			oplDelayData = OPL2_DELAY_DAT;
			dbgprintf("Dual OPL2 detected at %Xh!\n", settings.oplBase);
			break;
		case 3:
			oplDelayReg = OPL3_DELAY_REG;
			oplDelayData = OPL3_DELAY_DAT;
			dbgprintf("OPL3 detected at %Xh!\n", settings.oplBase);
			break;
		
	}
	//sleep(1);
    return 0;
}

///////////////////////////////////////////////////////////////////////////////
// VGM Parsing Functions
///////////////////////////////////////////////////////////////////////////////

// Read from the specified VGM file and person some validity checks
uint8_t loadVGM(uint8_t* buf)
{
	// Ensure we are at the beginning of the file.
    vgmFileData = buf;
    vgmFileBuffer = vgmFileData;
    seekSet(0);
	dataCurrentSample = 0;
	
	// Read enough bytes to get the VGM header, then populate header struct
	readBytes(256);
	currentVGMHeader.fileIdentification = *((uint32_t*)&vgmFileBuffer[0x00]);
	currentVGMHeader.eofOffset = getUint32(&vgmFileBuffer[0x04]);
	currentVGMHeader.versionNumber = getUint32(&vgmFileBuffer[0x08]);
	currentVGMHeader.gd3Offset = getUint32(&vgmFileBuffer[0x14]);
	currentVGMHeader.totalSamples = getUint32(&vgmFileBuffer[0x18]);
	currentVGMHeader.loopOffset = getUint32(&vgmFileBuffer[0x1C]);
	currentVGMHeader.loopNumSamples = getUint32(&vgmFileBuffer[0x20]);
	currentVGMHeader.recordingRate = getUint32(&vgmFileBuffer[0x24]);
	currentVGMHeader.vgmDataOffset = getUint32(&vgmFileBuffer[0x34]);
	currentVGMHeader.ym3812Clock = getUint32(&vgmFileBuffer[0x50]);
	currentVGMHeader.ym3526Clock = getUint32(&vgmFileBuffer[0x54]);
	currentVGMHeader.ymf262Clock = getUint32(&vgmFileBuffer[0x5C]);
	currentVGMHeader.loopBase = *((uint8_t *)&vgmFileBuffer[0x7E]); 
	currentVGMHeader.loopModifier = *((uint8_t *)&vgmFileBuffer[0x7F]);
	
	// Check if it's actually a VGM after all that
	if (memcmp((uint8_t *)&currentVGMHeader.fileIdentification, vgmIdentifier, 4) != 0)
	{	
		return killProgram(3);
	}
	
	// If VGM version is < 1.51 just bail out cause they don't support OPL anyway
	if (currentVGMHeader.versionNumber < 0x151)
	{
		return killProgram(4);
	}
	
	// Check for OPL2
	if (currentVGMHeader.ym3812Clock > 0)
	{
		// OK, it's OPL2.  Is it dual OPL2?
		if (currentVGMHeader.ym3812Clock > 0x40000000)
		{
			vgmChipType = 5;
			maxChannels = 18;
		}
		// No, it's single OPL2.
		else
		{
			vgmChipType = 2;
			maxChannels = 9;
		}
	}
	
	// Check for OPL1
	if (currentVGMHeader.ym3526Clock > 0)
	{
		// OK, it's OPL1.  Is it dual OPL1?
		if (currentVGMHeader.ym3526Clock > 0x40000000)
		{
			vgmChipType = 4;
			maxChannels = 18;
		}
		// No, it's single OPL1.
		else
		{
			// Was a single OPL2 already found?  This could be OPL1+OPL2.  A weird combination, but theoretically valid.
			if (vgmChipType == 2)
			{
				vgmChipType = 6;
				maxChannels = 18;
			}
			// Nah, it's just an OPL1 on its own.
			else
			{
				vgmChipType = 1;
				maxChannels = 9;
			}
		}
	}
	
	// Check for OPL3
	if (currentVGMHeader.ymf262Clock > 0)
	{
		// OK, it's OPL3.  Is it dual OPL3?
		if (currentVGMHeader.ymf262Clock > 0x40000000)
		{
			// That's a problem (for now).  Let's back out.  Might look into supporting it later, but I can't test it.
			// Not even taking into account how the hell to display all that info on screen.
			vgmChipType = 0;
		}
		// No, it's single OPL3.
		else
		{
			vgmChipType = 3;
			maxChannels = 18;
		}
	}
	
	// If the detected chip type is still 0, then there's nothing we can play for this anyway.
	if (vgmChipType == 0)
	{
		return killProgram(5);
	}
	
	// Chip check was ok.  Now compare vs detected OPL chip to see if it's playable, and if not, kill the program.  We also setup the base IO due to Dual OPL2 shenanigans
	switch (vgmChipType)
	{
		// OPL1
		case 1:
			if (detectedChip == 1 || detectedChip == 3)
			{
				oplBaseAddr = settings.oplBase;
				break;
			}
			// If Dual OPL2 was detected we secretly shift the base IO to base+8 so that single OPL2 goes to both stereo channels
			if (detectedChip == 2)
			{
				oplBaseAddr = settings.oplBase+8;
				break;
			}
			else
			{
				return killProgram(5);
				break;
			}
		// OPL2
		case 2:
			if (detectedChip == 1 || detectedChip == 3)
			{
				oplBaseAddr = settings.oplBase;
				break;
			}
			// If Dual OPL2 was detected we secretly shift the base IO to base+8 so that single OPL2 goes to both stereo channels
			if (detectedChip == 2)
			{
				oplBaseAddr = settings.oplBase+8;
				break;
			}
			else
			{
				return killProgram(5);
				break;
			}
		// OPL3
		case 3:
			if (detectedChip == 3)
			{
				oplBaseAddr = settings.oplBase;
				break;
			}
			else
			{
				return killProgram(5);
				break;
			}
		// Dual OPL1
		case 4:
			if (detectedChip >= 2)
			{
				oplBaseAddr = settings.oplBase;
				break;
			}
			else
			{
				return killProgram(5);
				break;
			}
		// Dual OPL2
		case 5:
			if (detectedChip >= 2)
			{
				oplBaseAddr = settings.oplBase;
				break;
			}
			else
			{
				return killProgram(5);
				break;
			}
		// OPL1 + OPL2 (not ready yet...)
		case 6:
			if (detectedChip >= 2)
			{
				oplBaseAddr = settings.oplBase;
				return killProgram(5); // Remove this when feature is ready
				break;
			}
			else
			{
				return killProgram(5);
				break;
			}
	}
	
	// Everything else is okay, I say it's time to load the GD3 tag!
	populateCurrentGd3();
	
	// Success!
	return 0;
}

// Move through the file based on the commands encountered.  Loads in data for supported commands, to be processed during playback.
uint8_t getNextCommandData(void)
{
	uint32_t currentWait = 0;
	// Proper functioning of this function is based on the file already being seeked to the VGM data offset
	
	// Read the first byte to see what command we are looking at.
	// The byte length can be different depending on the command, so we have to decide how many bytes to read in.
	if (readBytes(1) == 0)
	{
		commandID = vgmFileBuffer[0];
	}
	// Read failed - probably ran out of bytes.  End song.
	else
	{
        endSong();
	}
	
	
	// Mega size switch case for all known VGM commands.  Most will be ignored, though, since they don't apply to OPL.
	// We still need to handle the right number of bytes though, especially for multichip VGMs that happen to have OPL in them too.
	// The good news is that all OPL commands are two bytes (reg/data) so we can return a command easily.
	// There are ways to shrink this down considerably (grouping all x-byte commands), but for now I'm being explicit and can optimize later.
	switch (commandID)
	{
		
		// SN76489 (2nd chip) write value (skip 1 byte)
		case 0x30:
			readBytes(1);
			break;
			
		// AY8910 stereo mask (skip 1 byte)
		case 0x31:
			readBytes(1);
			break;
			
		// Reserved/unused single byte commands (skip 1 byte)
		case 0x32:
		case 0x33:
		case 0x34:
		case 0x35:
		case 0x36:
		case 0x37:
		case 0x38:
		case 0x39:
		case 0x3A:
		case 0x3B:
		case 0x3C:
		case 0x3D:
		case 0x3E:
			readBytes(1);
			break;
			
			
		// Game Gear Stereo write value (skip 1 byte)
		case 0x3F:
			readBytes(1);
			break;

		// Reserved/unused two byte commands (skip 2 bytes)
		// Skip 1 byte if VGM version is < 1.60
		case 0x40:
		case 0x41:
		case 0x42:
		case 0x43:
		case 0x44:
		case 0x45:
		case 0x46:
		case 0x47:
		case 0x48:
		case 0x49:
		case 0x4A:
		case 0x4B:
		case 0x4C:
		case 0x4D:
		case 0x4E:
			if (currentVGMHeader.versionNumber < 0x160)
			{
				readBytes(1);
			}
			else
			{
				readBytes(2);
			}
			break;
			
		// Game Gear PSG stereo (skip 1 byte)
		case 0x4F:
			readBytes(1);
			break;
			
		// SN76489 write value (skip 1 byte)
		case 0x50:
			readBytes(1);
			break;
		
		// YM2413 write value to register (skip 2 bytes)
		case 0x51:
			readBytes(2);
			break;
		
		// YM2612 port 0 write value to register (skip 2 bytes)
		case 0x52:
			readBytes(2);
			break;
		
		// YM2612 port 1 write value to register (skip 2 bytes)
		case 0x53:
			readBytes(2);
			break;	
			
		// YM2151 write value to register (skip 2 bytes)
		case 0x54:
			readBytes(2);
			break;	
		
		// YM2203 write value to register (skip 2 bytes)
		case 0x55:
			readBytes(2);
			break;
		
		// YM2608 port 0 write value to register (skip 2 bytes)
		case 0x56:
			readBytes(2);
			break;
		
		// YM2608 port 1 write value to register (skip 2 bytes)
		case 0x57:
			readBytes(2);
			break;

		// YM2610 port 0 write value to register (skip 2 bytes)
		case 0x58:
			readBytes(2);
			break;	
			
		// YM2610 port 1 write value to register (skip 2 bytes)
		case 0x59:
			readBytes(2);
			break;
			
		// YM3812 write value to register (first chip)
		case 0x5A:
			readBytes(2);
			commandReg = vgmFileBuffer[0];
			commandData = vgmFileBuffer[1];
			break;	
			
		// YM3526 write value to register (first chip)
		// Add additional handling later for OPL1+OPL2 if someone uses it...
		case 0x5B:
			readBytes(2);
			commandReg = vgmFileBuffer[0];
			commandData = vgmFileBuffer[1];
			break;	
			
		// Y8950 write value to register (skip 2 bytes)
		case 0x5C:
			readBytes(2);
			break;		
			
		// YMZ280B write value to register (skip 2 bytes)
		case 0x5D:
			readBytes(2);
			break;		
			
		// YMF262 port 0 write value to register (first chip)
		case 0x5E:	
			readBytes(2);
			commandReg = vgmFileBuffer[0];
			commandData = vgmFileBuffer[1];
			break;		
		
		// YMF262 port 1 write value to register (first chip)
		case 0x5F:
			readBytes(2);
			commandReg = vgmFileBuffer[0];
			commandData = vgmFileBuffer[1];
			break;
			
		// Wait
		case 0x61:
			readBytes(2);
			currentWait = getUint16(&vgmFileBuffer[0]);
			break;

		// Wait shortcut - 735 samples - we just turn this into a normal Wait with preset value
		case 0x62:
			currentWait = 0x02DF;
			commandID = 0x61;
			break;
		
		// Wait shortcut - 882 samples - we just turn this into a normal Wait with preset value
		case 0x63:
			currentWait = 0x0372;
			commandID = 0x61;
			break;
			
		// End of sound data (to be handled elsewhere)
		case 0x66:
			break;
			
		// Data block
		case 0x67:
            {
			// Read in data block header
			readBytes(6);
			// Use the last 4 bytes to find out how much to skip
			// Use fseek because it may be greater than our buffer size.
            seekSet(fileCursorLocation + getUint32(&vgmFileBuffer[2]));
            }
			break;
			
		// PCM RAM write (skip 11 bytes)
		case 0x68:
			readBytes(11);
			break;
		
		// Wait 1-16 samples
		case 0x70:
		case 0x71:
		case 0x72:
		case 0x73:
		case 0x74:
		case 0x75:
		case 0x76:
		case 0x77:
		case 0x78:
		case 0x79:
		case 0x7A:
		case 0x7B:
		case 0x7C:
		case 0x7D:
		case 0x7E:
		case 0x7F:
			currentWait = (commandID-0x6F);
			commandID = 0x61;
			break;
			
		// YM2612 port 0 address 2A write from data bank + wait (ignore)
		case 0x80:
		case 0x81:
		case 0x82:
		case 0x83:
		case 0x84:
		case 0x85:
		case 0x86:
		case 0x87:
		case 0x88:
		case 0x89:
		case 0x8A:
		case 0x8B:
		case 0x8C:
		case 0x8D:
		case 0x8E:
		case 0x8F:
			break;	
			
		// DAC stream control - setup stream control (skip 4 bytes)
		case 0x90:
			readBytes(4);
			break;
		
		// DAC stream control - set stream data (skip 4 bytes)
		case 0x91:
			readBytes(4);
			break;
		
		// DAC stream control - set stream frequency (skip 5 bytes)
		case 0x92:
			readBytes(5);
			break;
		
		// DAC stream control - start stream (skip 10 bytes)
		case 0x93:
			readBytes(10);
			break;
			
		// DAC stream control - stop stream (skip 1 byte)
		case 0x94:
			readBytes(1);
			break;
		
		// DAC stream control - start stream fast call (skip 4 bytes)
		case 0x95:
			readBytes(4);
			break;
		
		// AY8910 write value to register (skip 2 bytes)
		case 0xA0:
			readBytes(2);
			break;
		
		// YM2413 write value to register (second chip) (skip 2 bytes)
		case 0xA1:
			readBytes(2);
			break;
		
		// YM2612 port 0 write value to register (second chip) (skip 2 bytes)
		case 0xA2:
			readBytes(2);
			break;
		
		// YM2612 port 1 write value to register (second chip) (skip 2 bytes)
		case 0xA3:
			readBytes(2);
			break;	
			
		// YM2151 write value to register (second chip) (skip 2 bytes)
		case 0xA4:
			readBytes(2);
			break;	
		
		// YM2203 write value to register (second chip) (skip 2 bytes)
		case 0xA5:
			readBytes(2);
			break;
		
		// YM2608 port 0 write value to register (second chip) (skip 2 bytes)
		case 0xA6:
			readBytes(2);
			break;
		
		// YM2608 port 1 write value to register (second chip) (skip 2 bytes)
		case 0xA7:
			readBytes(2);
			break;

		// YM2610 port 0 write value to register (second chip) (skip 2 bytes)
		case 0xA8:
			readBytes(2);
			break;	
			
		// YM2610 port 1 write value to register (second chip) (skip 2 bytes)
		case 0xA9:
			readBytes(2);
			break;
			
		// YM3812 write value to register (second chip)
		case 0xAA:
			readBytes(2);
			commandReg = vgmFileBuffer[0];
			commandData = vgmFileBuffer[1];
			break;	
			
		// YM3526 write value to register (second chip)
		// Add additional handling later for OPL1+OPL2 if someone uses it...
		case 0xAB:
			readBytes(2);
			commandReg = vgmFileBuffer[0];
			commandData = vgmFileBuffer[1];
			break;	
			
		// Y8950 write value to register (second chip) (skip 2 bytes)
		case 0xAC:
			readBytes(2);
			break;		
			
		// YMZ280B write value to register (second chip) (skip 2 bytes)
		case 0xAD:
			readBytes(2);
			break;		
			
		// YMF262 port 0 write value to register (second chip) (skip 2 bytes)
		case 0xAE:	
			readBytes(2);
			break;		
		
		// YMF262 port 1 write value to register (second chip) (skip 2 bytes)
		case 0xAF:
			readBytes(2);
			break;
		
		// RF5C68 write value to register (skip 2 bytes)
		case 0xB0:
			readBytes(2);
			break;
		
		// RF5C164 write value to register (skip 2 bytes)
		case 0xB1:
			readBytes(2);
			break;
			
		//PWM write value to register (skip 2 bytes)
		case 0xB2:
			readBytes(2);
			break;
		
		// GameBoy DMG write value to register (skip 2 bytes)
		case 0xB3:
			readBytes(2);
			break;
			
		// NES APU write value to register (skip 2 bytes)
		case 0xB4:
			readBytes(2);
			break;
		
		// MultiPCM write value to register (skip 2 bytes)
		case 0xB5:
			readBytes(2);
			break;
			
		// uPD7759 write value to register (skip 2 bytes)
		case 0xB6:
			readBytes(2);
			break;
		
		// OKIM6258 write value to register (skip 2 bytes)		
		case 0xB7:
			readBytes(2);
			break;
			
		// OKIM6295 write value to register (skip 2 bytes)			
		case 0xB8:
			readBytes(2);
			break;
			
		// HuC6280 write value to register (skip 2 bytes)		
		case 0xB9:
			readBytes(2);
			break;
			
		// K053260 write value to register (skip 2 bytes)			
		case 0xBA:
			readBytes(2);
			break;
			
		// POKEY write value to register (skip 2 bytes)		
		case 0xBB:
			readBytes(2);
			break;
			
		// WonderSwan write value to register (skip 2 bytes)		
		case 0xBC:
			readBytes(2);
			break;
			
		// SAA1099 write value to register (skip 2 bytes)		
		case 0xBD:
			readBytes(2);
			break;
			
		// ES5506 write value to register (skip 2 bytes)		
		case 0xBE:
			readBytes(2);
			break;
			
		// GA20 write value to register (skip 2 bytes)		
		case 0xBF:
			readBytes(2);
			break;
		
		// SegaPCM write value to memory offset (skip 3 bytes)		
		case 0xC0:
			readBytes(3);
			break;
		
		// RF5C68 write value to memory offset (skip 3 bytes)		
		case 0xC1:
			readBytes(3);
			break;
			
		// RF5C164 write value to memory offset (skip 3 bytes)		
		case 0xC2:
			readBytes(3);
			break;
			
		// MultiPCM write set bank offset (skip 3 bytes)		
		case 0xC3:
			readBytes(3);
			break;
			
		// QSound write value to register (skip 3 bytes)		
		case 0xC4:
			readBytes(3);
			break;
			
		// SCSP write value to memory offset (skip 3 bytes)		
		case 0xC5:
			readBytes(3);
			break;
			
		// WonderSwan write value to memory offset (skip 3 bytes)		
		case 0xC6:
			readBytes(3);
			break;
			
		// VSU write value to memory offset (skip 3 bytes)		
		case 0xC7:
			readBytes(3);
			break;
			
		// X1-010 write value to memory offset (skip 3 bytes)		
		case 0xC8:
			readBytes(3);
			break;
			
		// Reserved/unused three byte commands (skip 3 bytes)
		case 0xC9:
		case 0xCA:
		case 0xCB:
		case 0xCC:
		case 0xCD:
		case 0xCE:
		case 0xCF:
			readBytes(3);
			break;
			
		// YMF278B write value to register (skip 3 bytes)		
		case 0xD0:
			readBytes(3);
			break;
		
		// YMF271 write value to register (skip 3 bytes)		
		case 0xD1:
			readBytes(3);
			break;
		
		// SCC1 write value to register (skip 3 bytes)		
		case 0xD2:
			readBytes(3);
			break;
		
		// K054539 write value to register (skip 3 bytes)		
		case 0xD3:
			readBytes(3);
			break;
		
		// C140 write value to register (skip 3 bytes)		
		case 0xD4:
			readBytes(3);
			break;
		
		// ES5503 write value to register (skip 3 bytes)		
		case 0xD5:
			readBytes(3);
			break;
		
		// ES5506 write value to register (skip 3 bytes)		
		case 0xD6:
			readBytes(3);
			break;
		
		// Reserved/unused three byte commands (skip 3 bytes)
		case 0xD7:
		case 0xD8:
		case 0xD9:
		case 0xDA:
		case 0xDB:
		case 0xDC:
		case 0xDD:
		case 0xDE:
		case 0xDF:
			readBytes(3);
			break;
		
		// YM2612 seek to offset in PCM data bank of data block type 0 (skip 4 bytes)
		case 0xE0:
			readBytes(4);
			break;
		
		// C352 write value to register (skip 4 bytes)		
		case 0xE1:
			readBytes(4);
			break;
			
		// Reserved/unused four byte commands (skip 4 bytes)
		case 0xE2:
		case 0xE3:
		case 0xE4:
		case 0xE5:
		case 0xE6:
		case 0xE7:
		case 0xE8:
		case 0xE9:
		case 0xEA:
		case 0xEB:
		case 0xEC:
		case 0xED:
		case 0xEE:
		case 0xEF:
		case 0xF0:
		case 0xF1:
		case 0xF2:
		case 0xF3:
		case 0xF4:
		case 0xF5:
		case 0xF6:
		case 0xF7:
		case 0xF8:
		case 0xF9:
		case 0xFA:
		case 0xFB:
		case 0xFC:
		case 0xFD:
		case 0xFE:
		case 0xFF:
			readBytes(4);
			break;	
		
		// We found something else.
		// If the file pointer has advanced into the GD3 header, just treat it like the end of the song.  (That is to say that we probably have a malformed VGM missing the "end of song data" command.
		// On the other hand, if we are somewhere in the middle of the song, we can't just skip an invalid command because we don't know how many bytes that command should be, so error out.
		default:
			if (currentVGMHeader.gd3Offset > 0 && (fileCursorLocation >= (currentVGMHeader.gd3Offset+0x14)))
			{
                endSong();
				return 1;
			}
			else
			{
                endSong();
				//killProgram(6);
                return 6;
			}
	}
	// Based on whatever the last wait value was, set what sample we "should" be on.
	// There can be multiple commands per sample, so this won't advance the sample counter in that case.
	dataCurrentSample = dataCurrentSample + currentWait;
	// Successful, return good status
	return 0;
}

// Extract the next null terminated string from the overall GD3 tag
// This is adapted from the original VGMPlay's ReadWStrFromFile
wchar_t* getNextGd3String(void)
{
	uint32_t currentPosition;
	uint32_t eofLocation;
	wchar_t* text;
	wchar_t* tempText;
	uint32_t stringLength;
	uint16_t grabbeduint8_tacter;
	
	// Set an outer boundary on our reads.
	eofLocation = currentVGMHeader.eofOffset+0x04;
	// Track where we are now (should be the start of the GD3 based on populateCurrentGd3)
	currentPosition = fileCursorLocation;
	
	// Allocate memory the same size as the entire GD3 tag.
	text = (wchar_t*)malloc(currentGD3Tag.tagLength);
	// If there is nothing in the tag, return nothing!  (Admittedly, you shouldn't end up here at all... but just in case!)
	if (text == NULL)
	{
		return NULL;
	}
	
	tempText = text - 1;
	stringLength = 0x00;
	
	// Count uint8_tacters in order until we encounter a null terminator.
	do
	{
		tempText++;
		// Read 2 bytes, the size of a wide uint8_tacter
		readBytes(2);
		// Store what we just read, interpret it as a wide uint8_tacter
		grabbeduint8_tacter = (uint16_t)vgmFileBuffer[0x00];
		*tempText = (wchar_t)grabbeduint8_tacter;
		// Move our position
		currentPosition+=0x02;
		// Add one uint8_tacter to the length of this string
		stringLength+=0x01;
		// If we overrun the end of the file for some reason, "add" a null terminator so we stop reading
		if (currentPosition >= eofLocation)
		{
			*tempText = L'\0';
			break;
		}
	} while (*tempText != L'\0');
	
	// Now that we know how long the actual single string is, reallocate the memory to the smaller size.
	text = (wchar_t *)realloc(text, stringLength * sizeof(wchar_t));
	// Send that resized allocation back.  That is the string.
	return text;
}

// Calls getNextGd3String to populate each GD3 tag value
void populateCurrentGd3(void)
{
    static wchar_t emptyName[2] = {0, 0};

    // Fill in default values
    currentGD3Tag.tagLength = 0;
    currentGD3Tag.trackNameE = emptyName;
    currentGD3Tag.trackNameJ = emptyName;
    currentGD3Tag.gameNameE = emptyName;
    currentGD3Tag.gameNameJ = emptyName;
    currentGD3Tag.systemNameE = emptyName;
    currentGD3Tag.systemNameJ = emptyName;
    currentGD3Tag.originalAuthorE = emptyName;
    currentGD3Tag.originalAuthorJ = emptyName;
    currentGD3Tag.releaseDate = emptyName;
    currentGD3Tag.converter = emptyName;
    currentGD3Tag.notes = emptyName;

    // There is a GD3, fill it in
    if (currentVGMHeader.gd3Offset != 0 )
    {
        // Ensure file seeks to the offset the GD3 sits at.
        seekSet(currentVGMHeader.gd3Offset+0x14);
        // Then start reading in tag values.  As we get to the end of each one, it should be in place for the next string.
        // Jump 12 bytes forward (covering GD3 identifier + version tag + data length)
        // Also I'm ignoring the version tag cause there seems to only be one version for now... ;)
        readBytes(12);
        currentGD3Tag.tagLength = getUint32(&vgmFileBuffer [0x08]);
        // Read in all fields in order.  This makes it easy as the file seek is always in the right place.
        currentGD3Tag.trackNameE = getNextGd3String();
        currentGD3Tag.trackNameJ = getNextGd3String();
        currentGD3Tag.gameNameE = getNextGd3String();
        currentGD3Tag.gameNameJ = getNextGd3String();
        currentGD3Tag.systemNameE = getNextGd3String();
        currentGD3Tag.systemNameJ = getNextGd3String();
        currentGD3Tag.originalAuthorE = getNextGd3String();
        currentGD3Tag.originalAuthorJ = getNextGd3String();
        currentGD3Tag.releaseDate = getNextGd3String();
        currentGD3Tag.converter = getNextGd3String();
        currentGD3Tag.notes = getNextGd3String();
    }
}

// Function is called during the timer loop to actually interpret the loaded VGM command and act accordingly
void processCommands(void)
{		
	// Read commands until we are on the same sample as the timer expects.
	// This isn't -great- due to the high timer resolution we are always lagging a bit and it basically locks the app up on slow CPUs.
	while (dataCurrentSample < tickCounter)
	{
		// Get the next command data, but stop processing if it didn't work
		if (getNextCommandData() != 0)
		{
			break;
		}

		// If end of song data, check for loop, or end song
		// Properly formatted VGMs use command 0x66 for this.
		// If this command is missing, readBytes via getNextCommandData should have caught that we overran the end of the file.
		if (commandID == 0x66)
		{
			if (loopCount < loopMax && currentVGMHeader.loopOffset > 0)
			{
                seekSet(currentVGMHeader.loopOffset+0x1C);
                if (loopMax < 255) {
				    loopCount++;
                }
				getNextCommandData();
			}
			else
			{
                endSong();
				return;
			}
		}
		// Interpret which command we are looking at - data or wait?
		switch (commandID)
		{	
			// OPL2 write
			case 0x5A:
				// Dual-OPL2 on OPL3 hack
				// First chip (left pan)
				if (detectedChip == 3 && vgmChipType == 5)
				{
					if ((commandReg & 0xC0) == 0xC0)
					{
						// Zero the stereo bits and write new panning
						commandData = ((commandData & 0x0F) | 0x10);
					}
				}
				writeOPL(commandReg,commandData);
				break;
			// OPL1 write
			case 0x5B:
				writeOPL(commandReg,commandData);
				break;
			// OPL3 write (port 1)
			case 0x5E:
				writeOPL(commandReg,commandData);
				break;
			// OPL3 write (port 2) - modify destination register en-route to indicate secondary registers.
			case 0x5F:
				writeOPL(commandReg+0x100,commandData);
				break;
			// Second OPL2 write - modify destination register en-route to indicate secondary registers.
			case 0xAA:
				// Dual-OPL2 on OPL3 hack
				// Modify incoming data if needed to force panning
				if (detectedChip == 3 && vgmChipType == 5)
				{
					// Second chip (right pan)
					if ((commandReg & 0xC0) == 0xC0)
					{
						// Zero the stereo bits and write new panning
						commandData = ((commandData & 0x0F) | 0x20);
					}
					// Absolutely under no circumstances try to write OPL2 Waveform Select to the OPL3 high block
					// This will cause the OPL3 to stop outputting sound
					if (commandReg == 0x01)
					{
						break;
					}
				}
				writeOPL(commandReg+0x100,commandData);
				break;
			// Second OPL1 write - modify destination register en-route to indicate secondary registers.
			case 0xAB:
				writeOPL(commandReg+0x100,commandData);
				break;	
			default:
				break;
		}
		
	}
}
