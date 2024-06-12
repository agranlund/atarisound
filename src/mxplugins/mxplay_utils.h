#ifndef _MXPLAY_UTILS_
#define _MXPLAY_UTILS_

#include "plugin.h"

#ifndef uint8
typedef unsigned char       uint8;
#endif
#ifndef uint16
typedef unsigned short      uint16;
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
typedef int                 int32;
#endif
#ifndef bool
typedef int16               bool;
#endif
#ifndef true
#define true                1
#endif
#ifndef false
#define false               0
#endif
#ifndef null
#define null                0
#endif

// -----------------------------------------------------------------------

extern void(*mxTimerAFunc)(void);
extern uint32 mxTimerATicks;
extern uint32 mxTimerALock;
extern uint32 mxTimerAOld;
extern void mxTimerAVec();

extern uint32 mxHookTimerA(void(*func)(void), uint32 hz);
extern void   mxUnhookTimerA();

// -----------------------------------------------------------------------

#define C__ISA  0x5F495341 /* '_ISA' */

typedef struct
{
    unsigned short  version;
    unsigned int    iobase;
    unsigned int    membase;
    unsigned short  irqmask;
    unsigned char   drqmask;
    unsigned char   endian;

    void            (*outp)(unsigned short port, unsigned char data);
    void            (*outpw)(unsigned short port, unsigned short data);
    unsigned char   (*inp)(unsigned short port);
    unsigned short  (*inpw)(unsigned short addr);
} isa_t;

extern isa_t* isa;

extern isa_t* isaInit();

static inline void outp(uint16 port, uint8 data) {
    isa->outp(isa->iobase + port, data);
}

static inline uint8 inp(uint16 port) {
    return isa->inp(isa->iobase + port);
}

// -----------------------------------------------------------------------

#ifdef DEBUG
    #include "stdio.h"
    #define dbg(...)    { printf(__VA_ARGS__); printf("\r\n"); }
    #define err(...)    { printf(__VA_ARGS__); printf("\r\n"); }
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


#endif
