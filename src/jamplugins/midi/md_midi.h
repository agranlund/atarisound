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

#ifndef _MD_MIDI_H_
#define _MD_MIDI_H_

#include "jam_sdk.h"

#define MIDI_MAX_TRACKS 32

typedef struct
{
    uint8 track;          ///< the track this was on
    uint8 channel;        ///< the midi channel
    uint8 size;           ///< the number of data bytes
    uint8 data[4];        ///< the data. Only 'size' bytes are valid
} MD_midi_event;

typedef struct
{
    uint8  track;         ///< the track this was on
    uint16 size;          ///< the number of data bytes
    uint8  data[256];     ///< the data. Only 'size' bytes are valid
} MD_sysex_event;

typedef struct
{
    uint8  track;         ///< the track this was on
    uint16 size;          ///< the number of data bytes
    uint8  type;          ///< meta event type
    uint8 data[256];
} MD_meta_event;

typedef struct
{
    uint8         _trackId;           ///< the id for this track
    uint32        _length;            ///< length of track in bytes
    uint32        _startOffset;       ///< start of the track in bytes from start of file
    uint32        _currOffset;        ///< offset from start of the track for the next read of SD data
    bool          _endOfTrack;        ///< true when we have reached end of track or we have encountered an undefined event
    uint32        _elapsedTicks;      ///< the total number of elapsed ticks since last event
    MD_midi_event _mev;               ///< data for MIDI callback function - persists between calls for run-on messages
} MD_MFTrack;

typedef struct
{
    uint32        _pos;
    uint32        _size;
    uint8*        _data;
} MD_MFBuf;


typedef struct {
    MD_MFBuf    _fd;

    void        (*_midiHandler)(MD_midi_event *pev);         ///< callback into user code to process MIDI stream
    void        (*_sysexHandler)(MD_sysex_event *pev);       ///< callback into user code to process SYSEX stream
    void        (*_metaHandler)(const MD_meta_event *pev);   ///< callback into user code to process META stream

    uint16      _format;                                    ///< file format - 0: single track, 1: multiple track, 2: multiple song
    uint16      _trackCount;                                ///< number of tracks in file

    uint16      _ticksPerQuarterNote;                       ///< time base of file
    uint32      _tickTime;                                  ///< calculated per tick based on other data for MIDI file
    uint16      _lastTickError;                             ///< error brought forward from last tick check
    uint32      _lastTickCheckTime;                         ///< the last time (microsec) an tick check was performed

    bool        _synchDone;                                 ///< sync up at the start of all tracks
    bool        _paused;                                    ///< if true we are currently paused
    bool        _looping;                                   ///< if true we are currently looping

    uint16      _tempo;                                     ///< tempo for this file in beats per minute
    int16       _tempoDelta;                                ///< tempo offset adjustment in beats per minute

    uint8       _timeSignature[2];                          ///< time signature [0] = numerator, [1] = denominator

    MD_sysex_event  _sev;                                   ///< temporary sysex data
    MD_meta_event   _mev;                                   ///< temporary meta data

    MD_MFTrack      _track[MIDI_MAX_TRACKS];                ///< the track data for this file
} MD_MIDIFile;


extern MD_MIDIFile* MD_OpenFile(const char* filename);
extern MD_MIDIFile* MD_OpenBuffer(uint8* buffer);
extern void MD_Close(MD_MIDIFile* md);

extern void MD_Update(MD_MIDIFile* mf, uint32 microSeconds);
extern void MD_Pause(MD_MIDIFile* mf, bool bMode);
extern void MD_Restart(MD_MIDIFile* mf);
extern bool MD_isEOF(MD_MIDIFile* mf);
extern void MD_Silence(MD_MIDIFile* mf);


#endif // _MD_MIDI_H_
