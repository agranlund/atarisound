

//-------------------------------------------------------------------------
//
// TimerA
//
//-------------------------------------------------------------------------
    .global _mxTimerATicks
    .global _mxTimerALock
    .global _mxTimerAOld
    .global _mxTimerAVec
    .global _mxTimerAFunc

_mxTimerATicks:     dc.l 0
_mxTimerALock:      dc.l 0
_mxTimerAFunc:      dc.l 0
_mxTimerAXbra:      dc.l 0x58425241              // XBRA
                    dc.l 0x4D585041              // MXPA
_mxTimerAOld:       dc.l 0
_mxTimerAVec:
    addq.l  #1,_mxTimerATicks       // increase tick counter
    tst.b   _mxTimerALock           // skip processing if locked
    bne.s   .skipTimerA
    move.b  #1,_mxTimerALock        // lock entry
    movem.l d0-d1/a0-a1,-(sp)       // save gcc regs
    move.w  16(sp),d0               // get old sr
    or.w    #0x2000,d0              // keep supervisor status
    move.l  _mxTimerAFunc,a0        // get timer functions
    move.b  #0xdf,0xfa0f.w          // clear in-service
    move.w  d0,sr                   // enable interrupts as they where
    jsr     (a0)                    // call timer function
    movem.l (sp)+,d0-d1/a0-a1       // restore gcc regs
    clr.b   _mxTimerALock           // unlock entry
    rte
.skipTimerA:
    move.b  #0xdf,0xfa0f.w          // clear timer-a in service
    rte

