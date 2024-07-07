//---------------------------------------------------------------------
// VGM plugin
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
#include "jam_sdk.h"
#include "mint/osbind.h"
#include "mint/cookie.h"

extern size_t em_inflate(const void *pCompressedData, size_t nCompressedDataSize, unsigned char *pOutData, size_t nMaxOutDataSize);
extern uint8 vgmslap_init();
extern uint8 vgmslap_load(uint8* buf);
extern void  vgmslap_pause(uint8 paused);
extern void  vgmslap_stop();
extern void  vgmslap_play();

static inline uint32 swap32(uint32* v) {
    *v = ((*v & 0xff000000) >> 24) | ((*v & 0x00ff0000) >> 8) | ((*v & 0x0000ff00) << 8) | ((*v & 0x000000ff) << 24);
    return *v;
}



// -----------------------------------------------------------------------
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

// -----------------------------------------------------------------------

uint8* currentSongPtr;
uint8* currentSongData;

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

static void songRestart() {
    if (currentSongPtr) {
        vgmslap_stop();
        vgmslap_play();
    }
}

static void songPause(bool paused) {
    vgmslap_pause(paused);
}

static bool songFinished() {
    return false;
}

// -----------------------------------------------------------------------
jamPluginInfo* jamOnPluginInfo() {
    return &info;
}

bool jamOnPluginStart() {
    currentSongPtr = null;
    currentSongData = null;
    return (vgmslap_init() == 0);
}

void jamOnPluginStop() {
    songUnload();
}

void jamOnLoad(uint8* songData)
{
    songLoad(songData);
}

void jamOnInfo(jamSongInfo* songInfo)
{
    songInfo->isYMsong = 1;
    songInfo->songCount = 0;
    if (currentSongPtr) {
        songInfo->songCount = 1;
    }
}

void jamOnPlay()
{
    songRestart();
}

void jamOnStop() {
    if (currentSongPtr) {
        songPause(true);
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
