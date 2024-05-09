/*
  This code is derived work of MD_MIDIFile Standard MIDI File Interpreter Library:
    https://github.com/MajicDesigns/MD_MIDIFile
    Copyright (C) 2012 Marco Colli

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2.1 of the License, or (at your option) any later version.

  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.

  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA

  2024, anders.granlund
*/

#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "md_midi.h"


// ---------------------------------------------------------------------------------------------
// constants
// ---------------------------------------------------------------------------------------------
#define DUMP_DATA 			0
#define SHOW_UNUSED_META 	0
#define MTHD_HDR      		"MThd"    	///< SMF marker
#define MTHD_HDR_SIZE 		4         	///< SMF marker length
#define MTRK_HDR      		"MTrk"    	///< SMF track header marker
#define MTRK_HDR_SIZE 		4         	///< SMF track header marker length
#define MB_LONG 			4   		///< readMultibyte() parameter specifying expected 4 byte value
#define	MB_TRYTE 			3  			///< readMultibyte() parameter specifying expected 3 byte value
#define MB_WORD 			2   		///< readMultibyte() parameter specifying expected 2 byte value
#define MB_BYTE 			1   		///< readMultibyte() parameter specifying expected 1 byte value

// ---------------------------------------------------------------------------------------------
// macros
// ---------------------------------------------------------------------------------------------
#define min(x,y) 		(x < y) ? x : y

#define BUF_SIZE(x)   	(sizeof(x)/sizeof(x[0]))

#define ARRAY_SIZE(a) \
    ((sizeof(struct { int isnt_array : \
     ((const void *)&(a) == &(a)[0]); }) * 0) + \
     (sizeof(a) / sizeof(*(a))))

#if 0
	#define DUMP(...)	dbg(__VA_ARGS__)
#else
	#define DUMP(...)	{ }
#endif



// ---------------------------------------------------------------------------------------------
// helpers
// ---------------------------------------------------------------------------------------------
static uint32 currentMicros;
static inline uint32 micros() {
	return currentMicros;
//	uint32 hz200 = *((volatile uint32*)0x4ba);
//	return hz200 * 5000UL;
}


// ---------------------------------------------------------------------------------------------
// filebuffer
// ---------------------------------------------------------------------------------------------

static inline uint32 fd_seekSet(MD_MFBuf* fd, uint32 pos) {
	fd->_pos = pos;
	return fd->_pos;
}

static inline uint32 fd_seekCur(MD_MFBuf* fd, int32 offs) {
	fd->_pos += offs;
	return fd->_pos;
}

static inline uint32 fd_pos(MD_MFBuf* fd) {
	return fd->_pos;
}

static inline uint8 fd_readByte(MD_MFBuf* fd) {
	return fd->_data[fd->_pos++];
}

char* fd_fgets(MD_MFBuf* fd, char* buf, uint32 buflen) {
	char* dst = buf;
	for (uint32 i=0; i < buflen-1; i++) {
		char c = fd_readByte(fd);
		*dst++ = c;
		if (c == '\n')
			break;
	}
	*dst++ = 0;
	return buf;
}

static inline uint32 fd_read(MD_MFBuf* fd, uint8* buf, uint32 len) {
	memcpy(buf, fd->_data, len);
	fd->_pos += len;
	return len;
}

static inline uint32 fd_readMultiByte(MD_MFBuf* fd, uint8 nLen)
{
	uint32 value = 0;
	for (uint8 i=0; i<nLen; i++) {
		value = (value << 8) + fd_readByte(fd);
	}
	return(value);
}

static inline uint32 fd_readVarLen(MD_MFBuf* fd)
{
  char c;
  uint32 value = 0;
  do {
    c = fd_readByte(fd);
    value = (value << 7) + (c & 0x7f);
  } while (c & 0x80);
  return(value);
}



// ---------------------------------------------------------------------------------------------
// midi file helpers
// ---------------------------------------------------------------------------------------------

static inline uint32 mf_getTickTime(MD_MIDIFile* mf) { return (mf->_tickTime); }
static inline uint16 mf_getTempo(MD_MIDIFile* mf) { return(mf->_tempo); }
static inline int16  mf_getTempoAdjust(MD_MIDIFile* mf) { return(mf->_tempoDelta); }
static inline uint16 mf_getTicksPerQuarterNote(MD_MIDIFile* mf) { return(mf->_ticksPerQuarterNote); }
static inline uint16 mf_getTimeSignature(MD_MIDIFile* mf) { return((mf->_timeSignature[0]<<8) + mf->_timeSignature[1]); }

uint16 mf_tickClock(MD_MIDIFile* mf) {
	// check if enough time has passed for a MIDI tick and work out how many!
	uint16  ticks = 0;
	uint32 elapsedTime = mf->_lastTickError + micros() - mf->_lastTickCheckTime;
	if (elapsedTime >= mf->_tickTime) {
		ticks = elapsedTime/mf->_tickTime;
		mf->_lastTickError = elapsedTime - (mf->_tickTime * ticks);
		mf->_lastTickCheckTime = micros();    // save for next round of checks
	}
	return(ticks);
}

static void mf_calcTickTime(MD_MIDIFile* mf) {
	// 1 tick = microseconds per beat / ticks per Q note
	// The variable "microseconds per beat" is specified by a MIDI event carrying 
	// the set tempo meta message. If it is not specified then it is 500,000 microseconds 
	// by default, which is equivalent to 120 beats per minute. 
	// If the MIDI time division is 60 ticks per beat and if the microseconds per beat 
	// is 500,000, then 1 tick = 500,000 / 60 = 8333.33 microseconds.
	if ((mf->_tempo + mf->_tempoDelta != 0) && mf->_ticksPerQuarterNote != 0 && mf->_timeSignature[1] != 0) {
		mf->_tickTime = (60 * 1000000L) / (mf->_tempo + mf->_tempoDelta); // microseconds per beat
		mf->_tickTime /= mf->_ticksPerQuarterNote;
	}
}

static void mf_setMicrosecondPerQuarterNote(MD_MIDIFile* mf, uint32 m) {
	// This is the value given in the META message setting tempo
	// work out the tempo from the delta by reversing the calcs in
	// calctickTime - m is already per quarter note
	mf->_tempo = (60 * 1000000L) / m;
	mf_calcTickTime(mf);
}
static void mf_setTempo(MD_MIDIFile* mf, uint16 t) {
	if ((mf->_tempoDelta + t) > 0) mf->_tempo = t;
	mf_calcTickTime(mf);
}
static void mf_setTempoAdjust(MD_MIDIFile* mf, int16 t) {
	if ((t + mf->_tempo) > 0) mf->_tempoDelta = t;
	mf_calcTickTime(mf);
}
static void mf_setTicksPerQuarterNote(MD_MIDIFile* mf, uint16 ticks) {
	mf->_ticksPerQuarterNote = ticks;
	mf_calcTickTime(mf);
}
static void mf_setTimeSignature(MD_MIDIFile* mf, uint8 n, uint8 d) {
	mf->_timeSignature[0] = n;
	mf->_timeSignature[1] = d;
	mf_calcTickTime(mf);
}

static void mf_synchTracks(MD_MIDIFile* mf)
{
	for (uint16 i = 0; i < mf->_trackCount; i++)
		mf->_track[i]._elapsedTicks = 0;
	mf->_lastTickCheckTime = micros();
	mf->_lastTickError = 0;
}


// ---------------------------------------------------------------------------------------------
// midi track helpers
// ---------------------------------------------------------------------------------------------

static void mt_restart(MD_MFTrack* mt)
{
  mt->_currOffset = 0;
  mt->_endOfTrack = false;
  mt->_elapsedTicks = 0;
}

static void mt_reset(MD_MFTrack* mt)
{
	mt->_length = 0;        // length of track in bytes
	mt->_startOffset = 0;   // start of the track in bytes from start of file
	mt_restart(mt);
	mt->_trackId = 255;
}


static MD_sysex_event mt_temp_sev;
void mt_parseEvent(MD_MIDIFile* mf, MD_MFTrack* mt) {
	uint32 mLen;
	uint8 eType = fd_readByte(&mf->_fd);
	switch (eType)
	{
		// ---------------------------- MIDI
		// midi_event = any MIDI channel message, including running status
		// Midi events (status bytes 0x8n - 0xEn) The standard Channel MIDI messages, where 'n' is the MIDI channel (0 - 15).
		// This status byte will be followed by 1 or 2 data bytes, as is usual for the particular MIDI message. 
		// Any valid Channel MIDI message can be included in a MIDI file.
		case 0x80 ... 0xBf: // MIDI message with 2 parameters
		case 0xe0 ... 0xef:
		{
			mt->_mev.size = 3;
			mt->_mev.data[0] = eType;
			mt->_mev.channel = mt->_mev.data[0] & 0xf;  // mask off the channel
			//mt->_mev.data[0] = mt->_mev.data[0] & 0xf0; // just the command byte
			mt->_mev.data[1] = fd_readByte(&mf->_fd);
			mt->_mev.data[2] = fd_readByte(&mf->_fd);
			#if DUMP_DATA
				DUMP("[MID2] Ch: %d Data: %02x %02x %02x", mt->_mev.channel, mt->_mev.data[0], mt->_mev.data[1], mt->_mev.data[2]);
			#else
				if (mf->_midiHandler != null)
					(mf->_midiHandler)(&mt->_mev);
			#endif
		}
		break;

		case 0xc0 ... 0xdf: // MIDI message with 1 parameter
		{
			mt->_mev.size = 2;
			mt->_mev.data[0] = eType;
			mt->_mev.channel = mt->_mev.data[0] & 0xf;  // mask off the channel
			//mt->_mev.data[0] = mt->_mev.data[0] & 0xf0; // just the command byte
			mt->_mev.data[1] = fd_readByte(&mf->_fd);

			#if DUMP_DATA
				DUMP("[MID1] Ch: %d Data: %02x %02x", mt->_mev.channel, mt->_mev.data[0], mt->_mev.data[1]);
			#else
				if (mf->_midiHandler != null)
					(mf->_midiHandler)(&mt->_mev);
			#endif
		}
		break;

		case 0x00 ... 0x7f: // MIDI run on message
		{
			// If the first (status) byte is less than 128 (0x80), this implies that MIDI 
			// running status is in effect, and that this byte is actually the first data byte 
			// (the status carrying over from the previous MIDI event). 
			// This can only be the case if the immediately previous event was also a MIDI event, 
			// ie SysEx and Meta events clear running status. This means that the _mev structure 
			// should contain the info from the previous message in the structure's channel member 
			// and data[0] (for the MIDI command). 
			// Hence start saving the data at byte data[1] with the byte we have just read (eType) 
			// and use the size member to determine how large the message is (ie, same as before).
			mt->_mev.data[1] = eType;
			for (uint8 i = 2; i < mt->_mev.size; i++)
				mt->_mev.data[i] = fd_readByte(&mf->_fd);  // next byte

			#if DUMP_DATA
				DUMP("[MID+] Ch: %d Data: ", mt->_mev.channel);
				for (uint8 i = 0; i<mt->_mev.size; i++)
					DUMP(" %02x", mt->_mev.data[i]);
			#else
				if (mf->_midiHandler != null) {
					(mf->_midiHandler)(&mt->_mev);
				}
			#endif
		}
		break;

		// ---------------------------- SYSEX
		case 0xf0:  // sysex_event = 0xF0 + <len:1> + <data_bytes> + 0xF7 
		case 0xf7:  // sysex_event = 0xF7 + <len:1> + <data_bytes> + 0xF7 
		{
			// collect all the bytes until the 0xf7 - boundaries are included in the message
			uint16 index = 0;
			mf->_sev.track = mt->_trackId;
			mf->_sev.size = fd_readVarLen(&mf->_fd);
			if (eType==0xF0) {
				mf->_sev.data[index++] = eType;
				mf->_sev.size++;
			}

			// The length parameter includes the 0xF7 but not the start boundary.
			// However, it may be bigger than our buffer will allow us to store.
			if ((mf->_sysexHandler == null) || (mf->_sev.size > ARRAY_SIZE(mf->_sev.data))) {
				fd_seekCur(&mf->_fd, mf->_sev.size - index);
			} else {
				fd_read(&mf->_fd, &mf->_sev.data[index], mf->_sev.size - index);
				(mf->_sysexHandler)(&mf->_sev);
			}
		}
		break;

		// ---------------------------- META
		case 0xff:  // meta_event = 0xFF + <meta_type:1> + <length:v> + <event_data_bytes>
		{
			eType = fd_readByte(&mf->_fd);
			mLen =  fd_readVarLen(&mf->_fd);
			uint32 pos = fd_pos(&mf->_fd);
			DUMP("[META] Type: 0x Len %d", eType, mLen);

			mf->_mev.track = mt->_trackId;
			mf->_mev.size = mLen;
			mf->_mev.type = eType;

			switch (eType)
			{
				case 0x2f:  // End of track
				{
					mt->_endOfTrack = true;
					DUMP("END OF TRACK");
				}
				break;

				case 0x51:  // set Tempo - really the microseconds per tick
				{
					uint32 value = fd_readMultiByte(&mf->_fd, MB_TRYTE);
					mf_setMicrosecondPerQuarterNote(mf, value);
					mf->_mev.data[0] = (value >> 16) & 0xFF;
					mf->_mev.data[1] = (value >> 8) & 0xFF;
					mf->_mev.data[2] = value & 0xFF;
					DUMP("SET TEMPO to %d us/tick or %d bpm", mf_getTickTime(mf), mf_getTempo(mf));
				}
				break;

				case 0x58:  // time signature
				{
					uint8 n = fd_readByte(&mf->_fd);
					uint8 d = fd_readByte(&mf->_fd);
					mf_setTimeSignature(mf, n, 1 << d);  // denominator is 2^n
					mf->_mev.data[0] = n;
					mf->_mev.data[1] = d;
					mf->_mev.data[2] = 0;
					mf->_mev.data[3] = 0;
					DUMP("SET TIME SIGNATURE to %d/%d", mf_getTimeSignature(mf) >> 8, mf_getTimeSignature(mf) & 0xf);
				}
				break;

				#if DUMP_DATA
				case 0x59:  // Key Signature
				{
						int8 sf = fd_readByte(&mf->_fd);
						uint8 mi = fd_readByte(&mf->_fd);
						mf->_mev.data[0] = sf;
						mf->_mev.data[1] = mi;

						DUMP("KEY SIGNATURE");
						const char* aaa[] = {"Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "D#", "A#"};
						if (sf >= -7 && sf <= 7) 
						{
							switch(mi)
							{
							case 0:
								strcpy(mev.chars, aaa[sf+7]);
								strcat(mev.chars, "M");
								break;
							case 1:
								strcpy(mev.chars, aaa[sf+10]);
								strcat(mev.chars, "m");
								break;
							default:
								strcpy(mev.chars, "Err"); // error mi
							}
						}
						else
						{
							strcpy(mev.chars, "Err"); // error sf
						}
						mev.size = strlen(mev.chars); // change META length
						DUMP("[%s]", mev.chars);
				}
				break;

				case 0x00:  // Sequence Number
				{
					uint16 x = fd_readMultiByte(&mf->_fd, MB_WORD);
					mev.data[0] = (x >> 8) & 0xFF;
					mev.data[1] = x & 0xFF;
					DUMP("SEQUENCE NUMBER %02x %02x", mev.data[0], mev.data[1]);
				}
				break;

				case 0x20:  // Channel Prefix
				{
					mev.data[0] = fd_readMultiByte(&mf->_fd, MB_BYTE);
					DUMP("CHANNEL PREFIX %02x", mev.data[0]);
				}
				break;

				case 0x21:  // Port Prefix
				{
					mev.data[0] = fd_readMultiByte(&mf->_fd, MB_BYTE);
					DUMP("PORT PREFIX %02x", mev.data[0]);
				}
				break;
				#endif

				#if SHOW_UNUSED_META
				case 0x01:  // Text
					DUMPS("TEXT ");
					for (uint8_t i=0; i<mLen; i++)
						DUMP("", (char)mf->_fd.read());
					break;
				case 0x02:  // Copyright Notice
					DUMPS("COPYRIGHT ");
					for (uint8_t i=0; i<mLen; i++)
						DUMP("", (char)mf->_fd.read());
					break;
				case 0x03:  // Sequence or Track Name
					DUMPS("SEQ/TRK NAME ");
					for (uint8_t i=0; i<mLen; i++)
						DUMP("", (char)mf->_fd.read());
					break;
				case 0x04:  // Instrument Name
					DUMPS("INSTRUMENT ");
					for (uint8_t i=0; i<mLen; i++)
						DUMP("", (char)mf->_fd.read());
					break;

				case 0x05:  // Lyric
					DUMPS("LYRIC ");
					for (uint8_t i=0; i<mLen; i++)
						DUMP("", (char)mf->_fd.read());
					break;

				case 0x06:  // Marker
					DUMPS("MARKER ");
					for (uint8_t i=0; i<mLen; i++)
						DUMP("", (char)mf->_fd.read());
					break;

				case 0x07:  // Cue Point
					DUMPS("CUE POINT ");
					for (uint8_t i=0; i<mLen; i++)
						DUMP("", (char)mf->_fd.read());
					break;

				case 0x54:  // SMPTE Offset
					DUMPS("SMPTE OFFSET");
					for (uint8_t i=0; i<mLen; i++)
						DUMP(" ", mf->_fd.read());
					break;

				case 0x7F:  // Sequencer Specific Metadata
					DUMPS("SEQ SPECIFIC");
					for (uint8_t i=0; i<mLen; i++)
						DUMPX(" ", mf->_fd.read());
					break;
				#endif // SHOW_UNUSED_META

				default:
				{
					if (mf->_metaHandler) {
						uint8 minLen = min(ARRAY_SIZE(mf->_mev.data)-1, mLen);
						fd_read(&mf->_fd, mf->_mev.data, minLen);
						mf->_mev.data[minLen] = 0; // in case it is a string
					}
				}
				break;
			}

			if (mf->_metaHandler) {
				(mf->_metaHandler)(&mf->_mev);
			}
			fd_seekSet(&mf->_fd, pos + mLen);
		}
		break;

		// ---------------------------- UNKNOWN
		default:
		{
			// stop playing this track as we cannot identify the eType
			mt->_endOfTrack = true;
			DUMP("[UKNOWN 0x%02x] Track aborted", eType);
		}
		break;
	}
}

static bool mt_getNextEvent(MD_MIDIFile *mf, MD_MFTrack *mt, uint16 tickCount) {
	// track_event = <time:v> + [<midi_event> | <meta_event> | <sysex_event>]

	// is there anything to process?
	if (mt->_endOfTrack)
		return(false);

	// move the file pointer to where we left off
	fd_seekSet(&mf->_fd, mt->_startOffset+mt->_currOffset);

	// Work out new total elapsed ticks - include the overshoot from
	// last event.
	mt->_elapsedTicks += tickCount;

	// Get the DeltaT from the file in order to see if enough ticks have
	// passed for the event to be active.
	uint32 deltaT = fd_readVarLen(&mf->_fd);

	// If not enough ticks, just return without saving the file pointer and 
	// we will go back to the same spot next time.
	if (mt->_elapsedTicks < deltaT)
		return(false);

	// Adjust the total elapsed time to the error against actual DeltaT to avoid 
	// accumulation of errors, as we only check for _elapsedTicks being >= ticks,
	// giving positive biased errors every time.
	mt->_elapsedTicks -= deltaT;

	DUMP("dT: %d + %d", deltaT, mt->_elapsedTicks);

	mt_parseEvent(mf, mt);

	// remember the offset for next time
	mt->_currOffset = fd_pos(&mf->_fd) - mt->_startOffset;

	// catch end of track when there is no META event  
	mt->_endOfTrack = mt->_endOfTrack || (mt->_currOffset >= mt->_length);
	if (mt->_endOfTrack) { DUMP(" - OUT OF TRACK"); }

	return(true);
}





// ---------------------------------------------------------------------------------------------
// midi file internal
// ---------------------------------------------------------------------------------------------

static inline bool mf_getNextEvent(MD_MIDIFile* mf) {

	// if we are paused we are paused!
	if (mf->_paused) 
		return false;

	// sync start all the tracks if we need to
	if (!mf->_synchDone)
	{
		mf_synchTracks(mf);
		mf->_synchDone = true;
	}

	// check if enough time has passed for a MIDI tick
	uint16 ticks = mf_tickClock(mf);

//	dbg("tick = %d", ticks);
//	return true;

	if (ticks == 0)
		return false;

	for (uint16 i = 0; i < mf->_trackCount; i++) {
		// Limit n to be a sensible number of events in the loop counter
		// When there are no more events, just break out
		// Other than the first event, the others have an elapsed time of 0 (ie occur simultaneously)
		for (uint8 n=0; n < 100; n++) {
			if (!mt_getNextEvent(mf, &mf->_track[i], n==0 ? ticks : 0))
				break;
		}
	}

	return(true);
}

static bool mf_init(MD_MIDIFile* mf) {
	mf->_paused = true;
	mf_setTicksPerQuarterNote(mf, 48); 				// 48 ticks per quarter note
	mf_setTempo(mf, 120);              				// 120 beats per minute
	mf_setTempoAdjust(mf, 0);          				// 0 beats per minute adjustment
	mf_setMicrosecondPerQuarterNote(mf, 500000);  	// 500,000 microseconds per quarter note
	mf_setTimeSignature(mf, 4, 4);     				// 4/4 time
	for (uint16 i=0; i<MIDI_MAX_TRACKS; i++) {
		mt_reset(&mf->_track[i]);
	}

	// parse header
	fd_seekSet(&mf->_fd, 0);
	char h[MTHD_HDR_SIZE+1]; 	// Header characters + nul
	fd_fgets(&mf->_fd, h, MTHD_HDR_SIZE+1);
	h[MTHD_HDR_SIZE] = '\0';

	if (strcmp(h, MTHD_HDR) != 0) {
		dbg("Invalid header %02x%02x%02x%02x", h[0], h[1], h[2], h[3]);
		return false;
	}

	// read header size
	uint32 hsize = fd_readMultiByte(&mf->_fd, MB_LONG);
	if (hsize != 6) {
		err("Invalid header size (%d)", hsize);
		return false;
	}

	// read file type
	mf->_format = fd_readMultiByte(&mf->_fd, MB_WORD);
	dbg("Midi format = %d", mf->_format);
	if ((mf->_format != 0) && (mf->_format != 1)) {
		err("Invalid file format (%d)", mf->_format);
		return false;
	}

	// read number of tracks
	mf->_trackCount = fd_readMultiByte(&mf->_fd, MB_WORD);
	dbg("Midi tracks = %d", mf->_trackCount);
	if (((mf->_format == 0) && (mf->_trackCount != 1)) || (mf->_trackCount > MIDI_MAX_TRACKS)) {
		err("Invalid track count (%d)", mf->_trackCount);
		return false;
	}

	// read ticks per quarter note
	mf->_ticksPerQuarterNote = fd_readMultiByte(&mf->_fd, MB_WORD);
	if (mf->_ticksPerQuarterNote & 0x8000) // top bit set is SMTE format
	{
		int framespersecond = (mf->_ticksPerQuarterNote >> 8) & 0x00ff;
		int resolution      = mf->_ticksPerQuarterNote & 0x00ff;

		switch (framespersecond) 
		{
			case 232:  framespersecond = 24; break;
			case 231:  framespersecond = 25; break;
			case 227:  framespersecond = 29; break;
			case 226:  framespersecond = 30; break;
			default: {
				err("Invalid fps (%d)", framespersecond);
				return false;
			}
		}
		mf->_ticksPerQuarterNote = framespersecond * resolution;
	} 
	mf_calcTickTime(mf);  // we may have changed from default, so recalculate

	// load tracks
	bool failed = false;
	for (uint16 i = 0; i<mf->_trackCount; i++)
	{
		MD_MFTrack* mt = &mf->_track[i];

		// save the trackid for use later
		mt->_trackId = mt->_mev.track = i;

		// Read the Track header
		// track_chunk = "MTrk" + <length:4> + <track_event> [+ <track_event> ...]
		{
			char h[MTRK_HDR_SIZE+1]; // Header characters + nul
			fd_fgets(&mf->_fd, h, MTRK_HDR_SIZE+1);
			h[MTRK_HDR_SIZE] = '\0';
			if (strcmp(h, MTRK_HDR) != 0) {
				failed = true;
				break;
			}
		}

		// Row read track chunk size and in bytes. This is not really necessary 
		// since the track MUST end with an end of track meta event.
		mt->_length = fd_readMultiByte(&mf->_fd, MB_LONG);

		// save where we are in the file as this is the start of offset for this track
		mt->_startOffset = fd_pos(&mf->_fd);
		mt->_currOffset = 0;

		DUMP("track %d: %d (%d)", i, mt->_startOffset, mt->_length);

		// Advance the file pointer to the start of the next track;
		fd_seekSet(&mf->_fd, mt->_startOffset+mt->_length);
	}

	return !failed;
}

// ---------------------------------------------------------------------------------------------
// public
// ---------------------------------------------------------------------------------------------

void MD_Update(MD_MIDIFile* mf, uint32 microSeconds)
{
	currentMicros = microSeconds;
	mf_getNextEvent(mf);
}


void MD_Silence(MD_MIDIFile* mf) {
	if (mf->_midiHandler) {
		dbg("Midi silence");
		MD_midi_event ev;
		ev.size = 3;
		ev.data[0] = 0xb0;		// control change
		ev.data[1] = 0x7b;		// all notes off
		ev.data[2] = 0;
		for (uint16 i=0; i<16; i++) {
			ev.channel = i;
			ev.data[0] = 0xb0 | i;		// (control change | channel)
			(mf->_midiHandler)(&ev);
		}

#if 0
		for (uint16 i=0; i<16; i++) {
			ev.channel = i;
			for (uint16 j=0; j<128; j++) {
				ev.data[0] = 0x80 | i;		// note-off
				ev.data[1] = j;				// key
				ev.data[2] = 0;				// vel
				(mf->_midiHandler)(&ev);
			}
		}
#endif
	}
}

void MD_Pause(MD_MIDIFile* mf, bool bMode) {
	dbg("Midi pause %d", bMode);
	mf->_paused = bMode;
	if (mf->_paused) {
		MD_Silence(mf);
	} else {
		mf->_lastTickCheckTime = micros();
	}
}

void MD_Restart(MD_MIDIFile* mf) {
	// track 0 contains information that does not need to be reloaded every time, 
	// so if we are looping, ignore restarting that track. The file may have one 
	// track only and in this case always sync from track 0.
	dbg("Midi restart");
	MD_Pause(mf, true);
	for (uint16 i=0; i<mf->_trackCount; i++) {
		mt_restart(&mf->_track[i]);
	}
	mf->_synchDone = false;
	MD_Pause(mf, false);
}

bool MD_isEOF(MD_MIDIFile* mf) {
	bool bEof = true;
	for (uint16 i=0; i<mf->_trackCount && bEof; i++) {
		bEof = (mf->_track[i]._endOfTrack && bEof);
	}
	if (bEof && mf->_looping) {
		MD_Restart(mf);
		bEof = false;
	}
	return(bEof);
}

MD_MIDIFile* MD_OpenFile(const char* filename) {
	// load and create midi object
	int16 fhandle = open(filename, 0);
	if (fhandle <= 0) {
		err("Failed to open %s", filename);
		return null;
	}
	int32 fsize = lseek(fhandle, 0, SEEK_END);
	lseek(fhandle, 0, SEEK_SET);
	MD_MIDIFile* mf = malloc(sizeof(MD_MIDIFile) + fsize);
	if (!mf) {
		err("Failed to allocate midi file");
		close(fhandle);
		return null;
	}
	memset(mf, 0, sizeof(MD_MIDIFile));
	mf->_fd._data = (uint8*) (sizeof(MD_MIDIFile) + ((uint32)mf));
	mf->_fd._size = fsize;
	mf->_fd._pos = 0;
	read(fhandle, mf->_fd._data, fsize);
	close(fhandle);
	if (!mf_init(mf)) {
		err("Failed to init midi file");
		free(mf);
		mf = null;
	}
	return mf;
}

MD_MIDIFile* MD_OpenBuffer(uint8* buffer) {
	MD_MIDIFile* mf = malloc(sizeof(MD_MIDIFile));
	if (!mf) {
		err("Failed to allocate midi file");
		return null;
	}
	memset(mf, 0, sizeof(MD_MIDIFile));
	mf->_fd._data = buffer;
	mf->_fd._size = 0xffffffff;
	mf->_fd._pos = 0;
	if (!mf_init(mf)) {
		err("Failed to init midi file");
		free(mf);
		mf = null;
	}
	return mf;
}

void MD_Close(MD_MIDIFile* mf) {
	dbg("Closing midi file");
	MD_Pause(mf, true);
	free(mf);
}

