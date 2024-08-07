#ifndef __TIMER_H
#define __TIMER_H

int  tmInit(void (*)(), int timerval, int stk);
void tmClose();
void tmSetNewRate(int val);
int  tmGetTicker();
void tmSetTicker(long t);
int  tmGetTimer();
int  tmGetCpuUsage();
void tmSetSecure();
void tmReleaseSecure();

#endif
