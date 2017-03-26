//======================================================================
// We did not write these functions, original author information below
//======================================================================

/*    Keil project for delay with Systick timer
 *
 *    Before you start, select your target, on the right of the "Load" button
 *
 *    @author     Tilen Majerle
 *    @email      tilen@majerle.eu
 *    @website    http://stm32f4-discovery.com
 *    @ide        Keil uVision 5
 */
/* Include core modules */
#include "stm32f4xx.h"
 
uint32_t multiplier;
 
void TM_Delay_Init(void) {
    RCC_ClocksTypeDef RCC_Clocks;
    
    /* Get system clocks */
    RCC_GetClocksFreq(&RCC_Clocks);
    
    /* While loop takes 4 cycles */
    /* For 1 us delay, we need to divide with 4M */
    multiplier = RCC_Clocks.HCLK_Frequency / 4000000;
}
 
void TM_DelayMillis(uint32_t millis) {
    /* Multiply millis with multipler */
    /* Substract 10 */
    millis = 1000 * millis * multiplier - 10;
    /* 4 cycles for one loop */
    while (millis--);
}
