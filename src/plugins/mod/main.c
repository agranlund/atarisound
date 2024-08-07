#include "stdio.h"
#include "string.h"
#include "unistd.h"
#include "binfile.h"
#include "xmplay.h"
#include "mcp.h"
#include "ims.h"
#include "plugin.h"


// -----------------------------------------------------------------------
// plugin
// -----------------------------------------------------------------------

static uint8 modType = 0;
static const char* modTypeNames[] = { "ProTracker", "FastTrackerII", "ScreamTracker" };
static uint8* currentSongPtr = 0;
static xmodule mod;

static bool pluginInit() {
    currentSongPtr = null;
    memset(&mod, 0, sizeof(xmodule));

    mxCalibrateDelay();
    uint32 iobase = mxIsaInit();
    if (iobase == 0) {
        return false;
    }

    imsinitstruct is;
    imsFillDefaults(&is);
    is.bufsize = 65536;
    is.pollmin = 61440;

    dbg("IMS Init");
    if (!imsInit(&is)) {
        err("IMS Init");
        return false;
    }

    return true;
}

static void songUnload() {
    if (currentSongPtr) {
        xmpStopModule();
        xmpFreeModule(&mod);
        memset(&mod, 0, sizeof(xmodule));
    }
    currentSongPtr = null;
}

static bool songLoad(uint8* buf, uint32 siz) {
    songUnload();
    if (buf == null)
        return false;

    binfile fil;
    bf_initref(&fil, buf, siz);

    int(*xmpLoad)(xmodule*m, binfile*f) = null;
    if (memcmp(&buf[0], "Extended Module: ", 17) == 0) {    // FastTrackerII
        modType = 1;
        xmpLoad = xmpLoadModule;
    } else if (memcmp(&buf[44], "SCRM", 4) == 0) {          // ScreamTracker (todo)
        modType = 2;
        xmpLoad = null;
    } else {                                                // ProTracker / Generic
        modType = 0;
        xmpLoad = xmpLoadMOD;
    }

    if (xmpLoad == null) {
        err("xmpLoad");
    }

    dbg("xmpLoadMOD");
    if (xmpLoad(&mod, &fil) < 0) {
        err("xmpLoadMOD");
        songUnload();
        return false;
    }

    currentSongPtr = buf;

    dbg("xmpLoadSamples");
    if (!xmpLoadSamples(&mod)) {
        err("xmpLoadSamples");
        songUnload();
        return false;
    }

    dbg("songLoad: OK")
    return true;
}

static void songStop() {
    if (currentSongPtr) {
        dbg("songStop: stop");
        xmpStopModule();
        dbg("songStop: OK");
    }
}

static void songRestart() {
    if (currentSongPtr) {
        dbg("songRestart: stop");
        xmpStopModule();
        dbg("songRestart: play");
        xmpPlayModule(&mod);
        dbg("songRestart: OK");
    }
}

#ifndef PLUGIN_JAM
static void songPause(bool paused) {
}
#endif

static bool songFinished() {
    return false;
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
	"OpenCP",
	"OpenCP Team",
	"2.6.0pre6",
    MXP_FLG_XBIOS
};

const struct SExtension mx_extensions[] = {
	{ "MOD", "ProTracker" },
	{ "XM",  "FastTrackerII" },
//	{ "S3M", "ScreamTracker" },
	{ NULL, NULL }
};

static int paramGetSongName() {
    mx_plugin.inBuffer.value = (long) &mod.name;
    return MXP_OK;
}

const struct SParameter mx_settings[] = {
    { "Track", MXP_PAR_TYPE_CHAR|MXP_FLG_INFOLINE|MXP_FLG_MOD_PARAM, NULL, paramGetSongName },
    { NULL, 0, NULL, NULL }
};


int mx_init() {
    return pluginInit() ? MXP_OK : MXP_ERROR;
}

int mx_register_module() {
    uint8* data = (uint8*) mx_plugin.inBuffer.pModule->p;
    size_t size = mx_plugin.inBuffer.pModule->size;
    return songLoad(data, size) ? MXP_OK : MXP_ERROR;
}

int mx_unregister_module() {
    songUnload();
    return MXP_OK;
}

int mx_get_songs() {
    mx_plugin.inBuffer.value = 1;
    return MXP_OK;
}

int mx_set() {
    songRestart();
    return MXP_OK;
}

int mx_unset() {
    songStop();
    return MXP_OK;
}

int mx_feed() {
   if (currentSongPtr) {
        if (songFinished()) {
            songStop();
            return MXP_ERROR;
        }
    }
    return MXP_OK;
}

int mx_pause() {
    if (currentSongPtr) {
        songPause(mx_plugin.inBuffer.value ? true : false);
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

#endif // PLUGIN_MXP

// -----------------------------------------------------------------------
//
// Jam
//
// -----------------------------------------------------------------------
#ifdef PLUGIN_JAM

jamPluginInfo info = {
    JAM_INTERFACE_VERSION,      // interfaceVersion
    0x0001,                     // pluginVersion
    "2024.08.07",               // date
    ".MOD",                     // ext0
    ".XM",                      // ext1
#if 0
    ".S3M",                     // ext2
#else
    " ",                        // ext2
#endif    
    " ",                        // ext3
    " ",                        // ext4
    " ",                        // ext5
    " ",                        // ext6
    " ",                        // ext7
    "OpenCP",                   // pluginName
    "Anders Granlund",          // authorName
    "granlund23@yahoo.se",      // authorEmail
    "www.happydaze.se",         // authorUrl
    "",                         // authorComment
    0,                          // isDsp
    0x1F,                       // support
    0,                          // datastart
    1,                          // supportsNextSongHook
    1,                          // supportsName
    1,                          // supportsComposer
    1,                          // supportsSongCount
    1,                          // supportsPreselect
    1,                          // supportsComment
    1,                          // supportsFastram
};

jamPluginInfo* jamOnPluginInfo() {
    return &info;
}

bool jamOnPluginStart() {
    return pluginInit();
}

void jamOnPluginStop() {
    songUnload();
}

void jamOnLoad(uint8* songData) {
    // they say size don't matter...
    uint32 size = 128 * 1024 * 1024;
    songLoad(songData, size);
}

void jamOnInfo(jamSongInfo* songInfo) {
    songInfo->isYMsong = 1;
    songInfo->songCount = 0;
    if (currentSongPtr) {
        songInfo->songCount = 1;
        strcpy(songInfo->title, mod.name);
        strcpy(songInfo->comments, modTypeNames[modType]);
    }
}

void jamOnPlay() {
    songRestart();
}

void jamOnStop() {
    songStop();
}

void jamOnUpdate() {
    if (currentSongPtr) {
        if (songFinished()) {
#if 0
            songUnload();
            jamSongFinished();
#else
            songRestart();
#endif
        }
    }
}

#endif // PLUGIN_JAM
