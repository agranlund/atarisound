// OpenCP Module Player
// copyright (c) '94-'98 Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//
// DOS4GFIX initialisation handlers
//
// revision history: (please note changes here)
//  -nb980510   Niklas Beisert <nbeisert@physik.tu-muenchen.de>
//    -first release
//  -kb98????   Tammo Hinrichs <opencp@gmx.net>                 (?)
//    -some bugfix(?)
//  -fd981205   Felix Domke <tmbinc@gmx.net>
//    -KB's "bugfix" doesn't work with Watcom106

#ifndef __IMSRTNS_H
#define __IMSRTNS_H

#include "string.h"
#include "sys/types.h"
#include "plugin.h"

#ifdef NOPRINTF
#define printf(...)
#endif

#define dbgprintf(...) dbg(__VA_ARGS__)

#if 1

static inline unsigned short ims_swap16(unsigned short* vle) {
    *vle = swap16(*vle);
    return *vle;
}

static inline unsigned long ims_swap32(unsigned long* vle) {
    *vle = swap32(*vle);
    return *vle;
}

static inline unsigned long umuldiv(unsigned long eax, unsigned long edx, unsigned long ebx) {
    __uint64_t edx_eax = eax * (__uint64_t)edx;
    unsigned long ret = (unsigned long) (edx_eax / ebx);
    return ret;
}

static inline unsigned long umulshr16(unsigned long eax,unsigned long edx) {
    __uint64_t edx_eax = eax * (__uint64_t)edx;
    unsigned long ret = (unsigned long) (edx_eax >> 16);
    return ret;
}

static inline unsigned long umuldivrnd(unsigned long eax, unsigned long edx, unsigned long ecx) {
    __uint64_t edx_eax = eax * (__uint64_t)edx;
    __uint64_t ebx = (__uint64_t) (ecx >> 1);
    edx_eax += ebx;
    unsigned long ret = (unsigned long) (edx_eax / ecx);
    return ret;
}

static inline unsigned short _disableint() {
    return mxDisableInterrupts();
}

static inline void _restoreint(unsigned short sr) {
    mxRestoreInterrupts(sr);
}

#else
long imuldiv(long,long,long);
#pragma aux imuldiv parm [eax] [edx] [ebx] value [eax] = "imul edx" "idiv ebx"

unsigned long umuldiv(unsigned long,unsigned long,unsigned long);
#pragma aux umuldiv parm [eax] [edx] [ebx] value [eax] = "mul edx" "div ebx"

long imulshr16(long,long);
#pragma aux imulshr16 parm [eax] [edx] [ebx] value [eax] = "imul edx" "shrd eax,edx,16"

unsigned long umulshr16(unsigned long,unsigned long);
#pragma aux umulshr16 parm [eax] [edx] [ebx] value [eax] = "mul edx" "shrd eax,edx,16"

unsigned long umuldivrnd(unsigned long, unsigned long, unsigned long);
#pragma aux umuldivrnd parm [eax] [edx] [ecx] modify [ebx] = "mul edx" "mov ebx,ecx" "shr ebx,1" "add eax,ebx" "adc edx,0" "div ecx"

void memsetd(void *, long, int);
#pragma aux memsetd parm [edi] [eax] [ecx] = "rep stosd"
void memsetw(void *, int, int);
#pragma aux memsetw parm [edi] [eax] [ecx] = "rep stosw"
void memsetb(void *, int, int);
#pragma aux memsetb parm [edi] [eax] [ecx] = "rep stosb"
void memcpyb(void *, void *, int);
#pragma aux memcpyb parm [edi] [esi] [ecx] = "rep movsb" modify [eax esi edi ecx]

short _disableint();
void _restoreint(short);

#ifdef WATCOM11
#pragma aux _disableint value [ax] modify exact [] = \
                         "pushf" \
                         "pop ax" \
                         "cli"
#pragma aux _restoreint parm [ax] modify exact [] = \
                         "push ax" \
                         "popf"
#else

#pragma aux _disableint value [ax] = "pushf" "pop ax" "cli"
#pragma aux _restoreint parm [ax] = "push ax" "popf"

#endif
#endif


#endif //__IMSRTNS_H
