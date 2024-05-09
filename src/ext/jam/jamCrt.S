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

    .text
    .extern _jamMessageHandler
    .global _jamLibInited
    .global _jamHookLog
    .global _jamHookAlert
    .global _jamHookNextSong
    .global _jamTimerAVec
    .global _jamTimerAOld
    .global _jamTimerALock
    .global _jamTimerAFunc
    .global _jamTimerATicks

//-------------------------------------------------------------------------
// On entry (To the Plug-In):
//-------------------------------------------------------------------------
// a0 = void* ( data1 );
// a1 = void* ( data2 );
// d0 = long  ( parameter )
// d1 = long  ( msg )
// 
// -> Plug-In call
// 
//-------------------------------------------------------------------------
// On exit (From the Plug-In):
//-------------------------------------------------------------------------
// a0 = void* ( data );
// 
// The plug-in has got one main entry point. Based on the value in
// the command-register d1, it has to offer the different routines
// which are needed to handle the idividual music-formats.
//-------------------------------------------------------------------------

_jamEntry:
    move.l  a0,-(sp)
    movem.l d0-d1/a1,-(sp)
    jsr     _jamMessageHandler
    move.l  d0,a0
    movem.l (sp)+,d0-d1/a1
    addq.l  #4,sp
    rts


//-------------------------------------------------------------------------
//
// Log hook
//
//-------------------------------------------------------------------------
    .global _jamCallHookLog
_jamCallHookLog:
    movem.l d0-d7/a0-a6,-(sp)
    move.l  60+4(sp),a0             // a0 = message
    move.l  #0x786C6F67,d0          // d0 = 'xlog'
    move.l  _jamHookLog,a1          // a1 = func
    move.w  sr,d2
    and.w   #0x2000,d2
    bne.s   .log_hook_from_super
    jsr     (a1)
    movem.l (sp)+,d0-d7/a0-a6
    rts
.log_hook_from_super:
    move.l  usp,a2
    move.l  a2,-(sp)
    move.l  sp,usp
    sub.l   #64,sp                  // 64 byte user stack for jam
    jsr     (a1)                    // call-in
    add.l   #64,sp
    move.l  (sp)+,a2
    move.l  a2,usp
    movem.l (sp)+,d0-d7/a0-a6
    rts

//-------------------------------------------------------------------------
//
// Alert hook
//
//-------------------------------------------------------------------------
    .global _jamCallHookAlert
_jamCallHookAlert:
    rts


//-------------------------------------------------------------------------
//
// NextSong hook
//
//-------------------------------------------------------------------------
    .global _jamCallHookNextSong
_jamCallHookNextSong:
    rts


//-------------------------------------------------------------------------
//
// TimerA
//
//-------------------------------------------------------------------------
    dc.l    0x58425241              // XBRA
    dc.l    0x4A414D50              // JAMA
_jamTimerAOld:    dc.l    0
_jamTimerAVec:
    addq.l  #1,_jamTimerATicks      // increase tick counter
    tst.b   _jamTimerALock          // skip processing if locked
    bne.s   .skipTimerA
    move.b  #1,_jamTimerALock       // lock entry
    movem.l d0-d1/a0-a1,-(sp)       // save gcc regs
    move.w  16(sp),d0               // get old sr
    or.w    #0x2000,d0              // keep supervisor status
    move.l  _jamTimerAFunc,a0       // get timer functions
    move.b  #0xdf,0xfa0f.w          // clear in-service
    move.w  d0,sr                   // enable interrupts as they where
    jsr     (a0)                    // call timer function
    movem.l (sp)+,d0-d1/a0-a1       // restore gcc regs
    clr.b   _jamTimerALock          // unlock entry
    rte
.skipTimerA:
    move.b  #0xdf,0xfa0f.w          // clear timer-a in service
    rte




//-------------------------------------------------------------------------
// Variables in text section
//-------------------------------------------------------------------------
    .even
_jamLibInited:      dc.l    0
_jamHookLog:        dc.l    0
_jamHookAlert:      dc.l    0
_jamHookNextSong:   dc.l    0
_jamTimerAFunc:     dc.l    0
_jamTimerATicks:    dc.l    0
_jamTimerALock:     dc.w    0
                    dc.w    0
