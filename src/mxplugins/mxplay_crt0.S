        .global _mx_plugin
		.global	_mx_parameter
		.global	_mx_register_module
		.global	_mx_get_playtime
		.global	_mx_init
		.global	_mx_set
		.global	_mx_feed
		.global	_mx_unset
		.global	_mx_pause
		.global	_mx_mute
		.global	_mx_info
		.global	_mx_extensions
		.global	_mx_settings

_mx_plugin:
        .dc.b   'M','X','P','2'
_mx_parameter:
        ds.l	1
_mx_callbacks:
		dc.l	_mx_register_module
		dc.l	_mx_get_playtime
		dc.l	_mx_get_songs
		dc.l	_mx_init
		dc.l	_mx_set
		dc.l	_mx_feed
		dc.l	_mx_unset
		dc.l	_mx_unregister_module
		dc.l	_mx_pause
		dc.l	_mx_mute
		dc.l	_mx_info
		dc.l	_mx_extensions
		dc.l	_mx_settings

_mx_vars:
        ds.l    1




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

