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
#include "mint/osbind.h"
#include "mint/cookie.h"

#include "../mxplay_utils.h"

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
	{ "VGM", "Adlib" },
	{ "VGZ", "Adlib" },
	{ NULL, NULL }
};

const struct SParameter mx_settings[] = {
    { NULL, 0, NULL, NULL }
};

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

static void songPause(bool paused) {
    vgmslap_pause(paused);
}

static bool songFinished() {
    return false;
}


// -----------------------------------------------------------------------


int mx_init() {
    currentSongPtr = null;
    currentSongData = null;
    isa_t* isa = isaInit();
    if (!isa) {
        return MXP_ERROR;
    }
    return (vgmslap_init(isa->iobase, 0x388) == 0);
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

