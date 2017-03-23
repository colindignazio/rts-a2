#ifndef _LED_H
#define _LED_H

#include "discoveryf4utils.h"

void initializeLEDs(Led_TypeDef);
void blinkLED(Led_TypeDef led);
void resetAllLEDs(void);

#endif
