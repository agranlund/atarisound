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

#ifndef _JAM_SDK_H_
#define _JAM_SDK_H_

#define JAM_INTERFACE_VERSION    0x0100

//---------------------------------------------------------------------
// types
//---------------------------------------------------------------------
#ifndef uint8
typedef unsigned char        uint8;
#endif
#ifndef uint16
typedef unsigned short        uint16;
#endif
#ifndef uint32
typedef unsigned int        uint32;
#endif
#ifndef int8
typedef char                int8;
#endif
#ifndef int16
typedef short               int16;
#endif
#ifndef int32
typedef int                    int32;
#endif
#ifndef bool
typedef int16                bool;
#endif
#ifndef true
#define true                1
#endif
#ifndef false
#define false                0
#endif
#ifndef null
#define null                0
#endif


//---------------------------------------------------------------------
// debug
//---------------------------------------------------------------------

typedef enum {
    JAM_LOG_DEBUG    = 0,
    JAM_LOG_ALERT     = 1,
} jamLogType;

#ifdef DEBUG
    #include "stdio.h"
    #define dbg(...)    { jamLog(JAM_LOG_DEBUG, __VA_ARGS__); }
    #define err(...)    { jamLog(JAM_LOG_ALERT, __VA_ARGS__); }
    #ifdef assert
    #undef assert
    #endif
    #define assert(x, ...) { if(!x) { err(__VA_ARGS__); } }
#else
    #define dbg(...)    { }
    #define err(...)    { }
    #ifdef assert
    #undef assert
    #endif
    #define assert(x, ...)    { }
#endif


//---------------------------------------------------------------------
// jam structures
//---------------------------------------------------------------------

typedef enum {
  JAM_ST        = 0,
  JAM_STE        = 1,
  JAM_TT        = 2,     
  JAM_MEGASTE    = 3,
  JAM_F030        = 4
} jamMachine;


typedef enum {
  JAM_INFO                = 1,
  JAM_INIT                = 2,
  JAM_ACTIVATE            = 3,
  JAM_SONGSELECT        = 4,
  JAM_SONGINFO            = 5,
  JAM_PLAY                = 6,
  JAM_STOP                = 7,
  JAM_DEACTIVATE        = 8,
  JAM_DEINIT            = 9,
  JAM_SONG_NEXT_HOOK    = 10,
  JAM_LOG_HOOK            = 11,
  JAM_ALERT_HOOK        = 12,
  JAM_UPDATE            = 13,        // ??? appears to tick contiously
} jamMsg;

typedef struct {
  uint16 interfaceVersion;
  uint16 pluginVersion;
  char date [12];
  char fileExt1 [6];
  char fileExt2 [6];
  char fileExt3 [6];
  char fileExt4 [6];
  char fileExt5 [6];
  char fileExt6 [6];
  char fileExt71 [6];
  char fileExt8 [6];
  char pluginName [128];
  char authorName [128];
  char authorEmail [128];
  char authorUrl [128];
  char authorComment [128];
  uint16 isDsp;
  uint16 support;
  uint32 datastart;
  int16 supportsNextSongHook;
  int16 supportsName;
  int16 supportsComposer;
  int16 supportsSongCount;
  int16 supportsPreselect; 
  int16 supportsComment; 
  int16 supportsFastram;
} jamPluginInfo;

typedef struct {
  char title[256];
  char composer[256];
  char ripper[256];
  char email[256];
  char www[256];
  char comments[256];
  uint16 songHz;
  uint16 songCount; 
  uint16 songPreselect; 
  uint16 isYMsong;
  uint32 dmaHz;
  char filename[256];
  uint16 playtime_min[99];
  uint16 playtime_sec[99];
} jamSongInfo;


//---------------------------------------------------------------------
// jam call-ins
//---------------------------------------------------------------------
extern jamPluginInfo* jamOnPluginInfo();                // plugin info
extern bool jamOnPluginStart();                            // plugin start
extern void jamOnPluginStop();                            // plugin stop

extern void jamOnLoad(uint8* songData);                    // load song data
extern void jamOnInfo(jamSongInfo* songInfo);            // load song info
extern void jamOnPlay();                                // play song
extern void jamOnStop();                                // stop song
extern void jamOnUpdate();                                // periodic update


//---------------------------------------------------------------------
// jam call-outs
//---------------------------------------------------------------------
extern void jamSongFinished();
extern void jamLog(jamLogType type, const char* msg, ...);

extern void jamHookVbl(void(*func)(void));
extern void jamUnhookVbl();

extern volatile uint32 jamTimerATicks;
extern uint32 jamHookTimerA(void(*func)(void), uint32 hz);
extern void jamUnhookTimerA();

#endif // _JAM_SDK_H_
