#ifndef __IRQ_H
#define __IRQ_H

int irqInit(int irq, void (*rout)(), int pre, int stk);
void irqClose();
void irqReInit();

void *getvect(unsigned char intno);
void setvect(unsigned char intno, void* vect);

#endif