#include <mint/osbind.h>
#include <mint/cookie.h>
#include "plugin.h"


// ------------------------------------------------------------------------------------------
static int mfpParamsFromHz(int32 hz, uint16* ctrl, uint16* data) {
    const uint32 dividers[8] = {1, 4, 10, 16, 50, 64, 100, 200};
    const uint32 baseclk = 2457600;
    int mindiff = 0xFFFFFF;
    int result = hz;
    *ctrl = 1; *data = 1;
    for (int i=7; i!=0; i--) {
        uint32 val0 = baseclk / dividers[i];
        for (int j=1; j<256; j++) {
            int val = val0 / j;
            int diff = (hz > val) ? (hz - val) : (val - hz);
            if (diff < mindiff) {
                result = val;
                mindiff = diff;
                *ctrl = i;
                *data = j;
            }
            if (val < hz) {
                break;
            }
        }
    }
    return result;
}

uint32 mxHookTimerA(void(*func)(void), uint32 hz)
{
    mxUnhookTimerA();
    mxTimerALock = 1;
    Jdisint(MFP_TIMERA);
    // todo: save all relevant mfp regs?
    mxTimerAOld = (uint32)Setexc(0x134>>2, -1);
    mxTimerAFunc = func;
    uint16 ctrl, data;
    uint32 real_hz = mfpParamsFromHz(hz, &ctrl, &data);
    Xbtimer(XB_TIMERA, ctrl, data, mxTimerAVec);
    Jenabint(MFP_TIMERA);
    mxTimerALock = 0;
    return real_hz;
}

void mxUnhookTimerA()
{
    if (mxTimerAFunc != null) {
        mxTimerALock = 1;
        Jdisint(MFP_TIMERA);
        (void)Setexc(0x134>>2, mxTimerAOld);
        mxTimerAFunc = 0;
        mxTimerAOld = 0;
    }
}

uint32 mxChangeTimerA(uint32 hz)
{
    // todo: can this be made platform independent and in a safe way?
    // this function can and will be called from inside a triggered
    // timer interrupt so special care would have to be taken.
    // Hades and Raven has MFP in the usual location but I am unsure about Milan
    // so we may need a special case for that machine here.
    mxTimerALock = 1;
    uint16 ctrl, data;
    uint32 real_hz = mfpParamsFromHz(hz, &ctrl, &data);
    *((volatile unsigned char*)0xfffa19) = ctrl;
    *((volatile unsigned char*)0xfffa1f) = data;
    mxTimerALock = 0;
    return real_hz;
}

// ------------------------------------------------------------------------------------------
static uint32 delayus_count = 0;
void mxCalibrateDelay() {
    uint32 tick_start = *((volatile uint32*)0x4ba);
    uint32 tick_end = tick_start;
    uint32 counter = 0;
    do {
        __asm__ volatile ( "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t" : : : );
        __asm__ volatile ( "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t" : : : );
        tick_end = *((volatile uint32*)0x4ba);
        counter++;
        if (counter > 1000000) {
            delayus_count = 0xffffffff;
            return;
        }
    } while ((tick_end - tick_start) <= 50);
    delayus_count = counter;
}

void mxDelay(uint32 us)
{
    if (delayus_count == 0) {
        mxCalibrateDelay();
        return;
    }
    if ((us < 1000) && (delayus_count != 0xffffffff)) {
        uint32 loops = 1 + ((4 * delayus_count * us) / (1000 * 1000));
        for (uint32 i=0; i<=loops; i++) {
            __asm__ volatile ( "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t" : : : );
            __asm__ volatile ( "nop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\tnop\n\t" : : : );
        }
        return;
    }

    // libcmini sleep resolution if 5ms
    unsigned int msec = us / 1000;
    msec = (msec > 5) ? msec : 5;
    extern void delay(unsigned long milliseconds);
    delay(msec);
}


// ------------------------------------------------------------------------------------------
#ifndef C__ISA
#define C__ISA  0x5F495341      /* '_ISA' */
#endif
#ifndef C_hade
#define C_hade  0x68616465      /* 'hade' */
#endif
#ifndef C__MIL
#define C__MIL  0x5F4D494C      /* '_MIL' */
#endif
#ifndef C__P2I
#define C__P2I  0x502F3249      /* 'P/2I' */
#endif
#ifndef C_RAVN
#define C_RAVN  0x5241564E      /* 'RAVN' */
#endif

#define ISA_MAX_DEV_IDS     5
#define ISA_MAX_DEV_PORT    8
#define ISA_MAX_DEV_MEM     4
#define ISA_MAX_DEV_IRQ     2
#define ISA_MAX_DEV_DMA     2

typedef struct
{
    unsigned int    id[ISA_MAX_DEV_IDS];
    unsigned int    mem[ISA_MAX_DEV_MEM];
    unsigned short  port[ISA_MAX_DEV_PORT];
    unsigned char   irq[ISA_MAX_DEV_IRQ];
    unsigned char   dma[ISA_MAX_DEV_DMA];
} isa_dev_t;

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
    void            (*outp_buf)(unsigned short port, unsigned char* buf, int count);
    void            (*outpw_buf)(unsigned short port, unsigned short* buf, int count);
    void            (*inp_buf)(unsigned short port, unsigned char* buf, int count);
    void            (*inpw_buf)(unsigned short port, unsigned short* buf, int count);
    unsigned int    (*irq_set)(unsigned char irq, unsigned int func);
    unsigned int    (*irq_en)(unsigned char irq, unsigned char enabled);
    isa_dev_t*      (*find_dev)(const char* id, unsigned short idx);
} isa_t;

static isa_t* isa = 0;
static uint32 isabase = 0;
void(*outp)(uint16 port, uint8 data);
void(*outpw)(uint16 port, uint16 data); 
uint8(*inp)(uint16 port);
uint16(*inpw)(uint16 port);

static void   outpb_null(uint16 port, uint8  data) {  }
static void   outpw_null(uint16 port, uint16 data) {  }
static uint8  inpb_null(uint16 port) { return 0xff;   }
static uint16 inpw_null(uint16 port) { return 0xffff; }

static void   outpb_isa(uint16 port, uint8  data)   { *((volatile uint8*)(isabase+port)) = data; }
static void   outpw_isa(uint16 port, uint16 data)   { *((volatile uint16*)(isabase+port)) = swap16(data); }
static uint8  inpb_isa(uint16 port)                 { return *((volatile uint8*)(isabase+port)); }
static uint16 inpw_isa(uint16 port)                 { return swap16(*((volatile uint16*)(isabase+port))); }

static void   outpb_isabios(uint16 port, uint8  data)   { isa->outp(isa->iobase + port, data);  }
static void   outpw_isabios(uint16 port, uint16 data)   { isa->outpw(isa->iobase + port, data); }
static uint8  inpb_isabios(uint16 port)                 { return isa->inp(isa->iobase + port);  }
static uint16 inpw_isabios(uint16 port)                 { return isa->inpw(isa->iobase + port); }


uint32 mxIsaInit() {
    isa = null;
    isabase = 0;

    // isa_bios
    if (Getcookie(C__ISA, (long*)&isa) == C_FOUND) {
        isabase = isa->iobase;
        outp = outpb_isabios;
        outpw = outpw_isabios;
        inp = inpb_isabios;
        inpw = inpw_isabios;
    }

    // guess when isa_bios is not available
    if (!isa || (isabase == 0)) {
        isa = null; isabase = 0;

        // todo: check environment variable

        // best guess based on computer cookies
        unsigned long cookie = 0;
        if (Getcookie(C_hade, (long*)&cookie) == C_FOUND) {            // hades
            isabase  = 0xFFF30000;
        } else if (Getcookie(C__MIL, (long*)&cookie) == C_FOUND) {     // milan
            isabase = 0xC0000000;
        } else if (Getcookie(C__P2I, (long*)&cookie) == C_FOUND) {     // panther
            uint32* cardpth2 = (uint32*) (cookie+6);
            isabase  = *cardpth2;
        } else if (Getcookie(C_RAVN, (long*)&cookie) == C_FOUND) {     // raven
            isabase = 0x81000000;
        }

        if (isabase == 0) {
            outp  = outpb_null;
            outpw = outpw_null;
            inp   = inpb_null;
            inpw  = inpw_null;
        } else {
            outp  = isabase ? outpb_isa : outpb_null;
            outpw = isabase ? outpw_isa : outpw_null;
            inp   = isabase ? inpb_isa : inpb_null;
            inpw  = isabase ? inpw_isa : inpw_null;
        }
    }

    return isabase;
}

uint16 mxIsaPort(const char* dev, uint8 idx, uint16 fallback) {

    if (isa) {
        isa_dev_t* isadev = isa->find_dev(dev, idx);
        if (isadev) {
            return isadev->port[0];
        }
    }
    return fallback;
}

