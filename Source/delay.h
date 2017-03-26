/*
 * Implementations for these functions found on: https://stm32f4-discovery.net/2014/09/precise-delay-counter/
 */

#ifndef _DELAY_H
#define _DELAY_H

void TM_Delay_Init(void);
void TM_DelayMillis(uint32_t);

#endif
