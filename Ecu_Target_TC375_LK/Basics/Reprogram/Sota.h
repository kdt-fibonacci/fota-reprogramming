#ifndef SOTA_H_
#define SOTA_H_

#include "Transport/Sota_CanFdTransport.h"

void Sota_Init(void);
void Sota_MainFunction_10ms(void);
void Sota_OnCanFdRxFrame(const SotaCanFd_RxFrame_t *frame);
void Sota_BootCheck(void);

#endif /* SOTA_H_ */
