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

#ifdef PLUGIN_JAM

#include "plugin.h"
#include "string.h"
#include "stdio.h"
#include "stdarg.h"
#include "mint/osbind.h"

static char buf[128];
extern uint32 jamLibInited;
extern uint32 jamHookLog;
extern uint32 jamHookAlert;
extern uint32 jamHookNextSong;

static uint8* songData = 0;
static jamSongInfo* songInfo = 0;
static bool ignoreLoad = true;

extern void jamCallHookNextSong();
extern void jamCallHookLog(char* msg);
extern void jamCallHookAlert(char* msg);

//-------------------------------------------------------------
// Initialization
//-------------------------------------------------------------
void jamInitLib()
{
    songData = 0;
    songInfo = 0;
    jamLibInited = 1;
    ignoreLoad = true;
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
            dbg("JAM_INFO");
            return (uint32) jamOnPluginInfo();
        } break;

          case JAM_INIT:
        {
            dbg("JAM_INIT");
            jamInitLib();
        } break;

          case JAM_ACTIVATE:
        {
            dbg("JAM_ACTIVATE");
            jamOnPluginStart();
        } break;

          case JAM_SONGSELECT:
        {
            // data1 is a pointer to file contents
            dbg("JAM_SONGSELECT: %x", data1);
#if 0
            songData = (uint8*) data1;
            jamOnLoad(songData);
#endif

        } break;

          case JAM_SONGINFO:
        {
            // data1 is pointer to song info struct
            // data2 is same as JAM_SONGSELECT:data1 (file content)
            dbg("JAM_SONGINFO: %08x %08x", data1, data2);
#if 1            
            if (data2) {
                songData = (uint8*) data2;
                jamOnLoad(songData);
            }
            songInfo = (jamSongInfo*) data1;
            jamOnInfo(songInfo);
#else            
            if (data1) {
                songInfo = (jamSongInfo*) data1;
                jamOnInfo(songInfo);
            }
#endif            
        } break;

          case JAM_PLAY:
        {
            dbg("JAM_PLAY: %08x %08x", songData, songInfo);
            if (songData && songInfo) {
                jamOnPlay();
            }
        } break;

          case JAM_STOP:
        {
            dbg("JAM_STOP");
            jamOnStop();
        } break;

          case JAM_DEACTIVATE:
        {
            dbg("JAM_DEACTIVATE");
            jamOnPluginStop();
        } break;

          case JAM_DEINIT:
        {
            dbg("JAM_DEINIT");
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

#endif // PLUGIN_JAM
