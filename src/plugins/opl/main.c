//---------------------------------------------------------------------
// VGM plugin for mxPlay and Jam
// 2024, anders.granlund
//---------------------------------------------------------------------
//
// This plugin is based on vgmslap (c) MrKsoft
// https://github.com/MrKsoft/vgmslap
//
// This plugin uses the em_inflate decompression library (c) Emmanuel Marty.
// https://github.com/emmanuel-marty
//
// See README.MD for licenses of the above mentioned softwares
//
//---------------------------------------------------------------------
#include "stdio.h"
#include "unistd.h"
#include "string.h"
#include "mint/osbind.h"
#include "mint/cookie.h"

#include "plugin.h"

// -----------------------------------------------------------------------
extern size_t em_inflate(const void *pCompressedData, size_t nCompressedDataSize, unsigned char *pOutData, size_t nMaxOutDataSize);
extern uint8 vgmslap_init();
extern uint8 vgmslap_load(uint8* buf);
extern void  vgmslap_pause(uint8 paused);
extern void  vgmslap_stop();
extern void  vgmslap_play();
extern void vgmslap_info(
    uint8*    chipType,
    uint32*   songLength,
    int16**   songName,
    int16**   songAuthor);

static inline uint32 swap32(uint32* v) {
    *v = ((*v & 0xff000000) >> 24) | ((*v & 0x00ff0000) >> 8) | ((*v & 0x0000ff00) << 8) | ((*v & 0x000000ff) << 24);
    return *v;
}

// -----------------------------------------------------------------------

static uint8* currentSongPtr = 0;
static uint8* currentSongData = 0;

static bool pluginInit() {
    currentSongPtr = null;
    currentSongData = null;

    mxCalibrateDelay();
    uint32 iobase = mxIsaInit();

    if (iobase == 0) {
        return false;
    }

    uint16 ioport = 0;
    ioport = ioport ? ioport : mxIsaPort("PNPB020", 0, 0);    // opl3 compatible
    ioport = ioport ? ioport : mxIsaPort("PNPB005", 0, 0);    // opl2 compatible
    ioport = ioport ? ioport : 0x388;                         // default oplx port
    return (vgmslap_init(iobase, ioport) == 0);
}

static void songUnload() {
    if (currentSongPtr) {
        vgmslap_stop();
        if (currentSongData) {
            free(currentSongData);
            currentSongData = null;
        }
        currentSongPtr = null;
    }
}

static bool songLoad(uint8* buf) {
    songUnload();
    if (buf == null)
        return false;

    // decompress
    currentSongPtr = buf;
    if ((buf[0] == 0x1F) && (buf[1] == 0x8B)) {
        // decompress and validate header
        uint8* buf_decompressed = malloc(1024);
        uint32 s1 = em_inflate(buf, 256, buf_decompressed, 256);
        if (*((uint32*)buf_decompressed) != 0x56676d20) {
            free(buf_decompressed);
            return false;
        }
        // reallocate decompression buffer for final size
        uint32 fsize_decompressed = *((uint32*)&buf_decompressed[4]);
        fsize_decompressed = 4 + swap32(&fsize_decompressed);
        buf_decompressed = realloc(buf_decompressed, fsize_decompressed);
        if (!buf_decompressed) {
            free(buf_decompressed);
            return false;
        }
        // decompress
        uint32 s = em_inflate(buf, fsize_decompressed, buf_decompressed, fsize_decompressed);
        if (*((uint32*)buf_decompressed) != 0x56676d20) {
            free(buf_decompressed);
            return false;
        }
        currentSongData = buf_decompressed;
    }
    if (vgmslap_load(currentSongData ? currentSongData : currentSongPtr) != 0) {
        songUnload();
        return false;
    }
    return true;
}

static void songStop() {
    if (currentSongPtr) {
        vgmslap_stop();
    }
}

static void songRestart() {
    if (currentSongPtr) {
        vgmslap_stop();
        vgmslap_play();
    }
}

#ifndef PLUGIN_JAM
static void songPause(bool paused) {
    vgmslap_pause(paused);
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
	"VGMSlap",
	"MrKsoft",
	"1.0",
    MXP_FLG_XBIOS
};

const struct SExtension mx_extensions[] = {
	{ "VGM", "VGM" },
	{ "VGZ", "VGM" },
	{ NULL, NULL }
};

static char tempBuffer[128];
const char* chipNames[] = { "Unknown", "OPL1", "OPL2", "OPL3", "OPL1x2", "OPL2x2", "OPL1+2", "OPL3x2" };

static int paramGetChipType() {
    uint8 chipType = 0;
    vgmslap_info(&chipType, 0, 0, 0);
    chipType = (chipType < 8) ? chipType : 0;
    mx_plugin.inBuffer.value = (long) chipNames[chipType];
    return MXP_OK;
}

static void wchar_to_char(char* out, short* in, int buflen, char* def) {
    out[0] = 0;
    if (in && (in[0] != 0)) {
        int pos = 0;
        while ((in[pos] != 0) && (pos < (buflen-1))) {
            out[pos] = (char) in[pos];
            pos++;
        }
        out[pos] = 0;
    }
    if ((out[0] == 0) && def) {
        strncpy(out, def, buflen-1);
    }
}

static int paramGetSongName() {
    int16* wstr = 0;
    vgmslap_info(0, 0, &wstr, 0);
    wchar_to_char(tempBuffer, wstr, 128, "Unknown");
    mx_plugin.inBuffer.value = (long) &tempBuffer;
    return MXP_OK;
}

static int paramGetAuthor() {
    int16* wstr = 0;
    vgmslap_info(0, 0, 0, &wstr);
    wchar_to_char(tempBuffer, wstr, 128, "Unknown");
    mx_plugin.inBuffer.value = (long) &tempBuffer;
    return MXP_OK;
}

const struct SParameter mx_settings[] = {
    { "Chip", MXP_PAR_TYPE_CHAR|MXP_FLG_INFOLINE|MXP_FLG_MOD_PARAM, NULL, paramGetChipType },
    { "Track", MXP_PAR_TYPE_CHAR|MXP_FLG_INFOLINE|MXP_FLG_MOD_PARAM, NULL, paramGetSongName },
    { "Author", MXP_PAR_TYPE_CHAR|MXP_FLG_MOD_PARAM, NULL, paramGetAuthor },
    { NULL, 0, NULL, NULL }
};


int mx_init() {
    return pluginInit() ? MXP_OK : MXP_ERROR;
}

int mx_register_module() {
    uint8* data = (uint8*) mx_plugin.inBuffer.pModule->p;
    size_t size = mx_plugin.inBuffer.pModule->size;
    return songLoad(data) ? MXP_OK : MXP_ERROR;
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
    0x0004,                     // pluginVersion
    "2024.05.09",               // date
    ".VGM",                     // ext0
    ".VGZ",                     // ext1
    " ",                        // ext2
    " ",                        // ext3
    " ",                        // ext4
    " ",                        // ext5
    " ",                        // ext6
    " ",                        // ext7
    "VGMSlap",                  // pluginName
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
    songUnload();
}

void jamOnLoad(uint8* songData) {
    songLoad(songData);
}

void jamOnInfo(jamSongInfo* songInfo) {
    songInfo->isYMsong = 1;
    songInfo->songCount = 0;
    if (currentSongPtr) {
        songInfo->songCount = 1;
    }
}

void jamOnPlay() {
    songRestart();
}

void jamOnStop() {
    if (currentSongPtr) {
        songStop();
    }
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

