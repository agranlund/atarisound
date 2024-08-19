#ifndef _PLUGIN_API_H_
#define _PLUGIN_API_H_

// -----------------------------------------------------------------------
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
extern volatile uint32 mxTimerATicks;
extern volatile uint32 mxTimerALock;
extern volatile uint32 mxTimerAOld;
extern void mxTimerAVec();

extern uint32   mxHookTimerA(void(*func)(void), uint32 hz);
extern uint32   mxChangeTimerA(uint32 hz);
extern void     mxUnhookTimerA();
extern bool     mxDisableTimerA();
extern void     mxRestoreTimerA(bool enable);

extern bool     mxHookIsaInterrupt(void(*func)(void), uint16 inum);
extern void     mxUnhookIsaInterrupt();

extern void mxCalibrateDelay();
extern void mxDelay(uint32 us);

// -----------------------------------------------------------------------
static inline uint16 mxDisableInterrupts() {
    uint16 oldsr;
    __asm__ __volatile__(
        " move.w    sr,%0\n\t"
        " or.w      #0x0700,sr\n\t"
    : "=d"(oldsr) : : "cc" );
    return oldsr & 0x0F00;
}

static inline void mxRestoreInterrupts(uint16 oldsr) {
    __asm__ __volatile__(
        " move.w    sr,d0\n\t"
        " and.w     #0xF0FF,d0\n\t"
        " or.w      %0,d0\n\t"
        " move.w    d0,sr\r\t"
    : : "d"(oldsr) : "d0", "cc" );
}

// -----------------------------------------------------------------------
static inline uint16 swap16(uint16 le) { uint16 be = ((le & 0xFF00) >> 8) | ((le << 8) & 0xFF00); return be; }
static inline uint32 swap32(uint32 le) { uint32 be = (((le & 0xff000000) >> 24) | ((le & 0x00ff0000) >>  8) | ((le & 0x0000ff00) <<  8) | ((le & 0x000000ff) << 24)); return be; }

// -----------------------------------------------------------------------
extern uint32 mxIsaInit();
extern uint16 mxIsaPort(const char* dev_id, uint8 dev_idx, uint8 port_idx, uint16 fallback);
extern uint8  mxIsaIrq(const char* dev_id, uint8 dev_idx, uint8 irq_idx, uint8 fallback);

extern void(*outp)(uint16 port, uint8 data);
extern void(*outpw)(uint16 port, uint16 data);
extern uint8(*inp)(uint16 port);
extern uint16(*inpw)(uint16 port);

// -----------------------------------------------------------------------
#ifdef PLUGIN_MXP
#include "plugin_mxp.h"
#endif
#ifdef PLUGIN_JAM
#include "plugin_jam.h"
#endif

// -----------------------------------------------------------------------
#ifdef DEBUG
    #ifdef PLUGIN_JAM
        #define dbg(...)    { jamLog(JAM_LOG_DEBUG, __VA_ARGS__); }
        #define err(...)    { jamLog(JAM_LOG_ALERT, __VA_ARGS__); }
    #else
        #include "stdio.h"
        #define dbg(...)    { printf(__VA_ARGS__); printf("\r\n"); }
        #define err(...)    { printf("ERR: "); printf(__VA_ARGS__); printf("\r\n"); }
    #endif    
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


#endif // _PLUGIN_API_H_
