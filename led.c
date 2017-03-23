#include "led.h"

void initializeLEDs(Led_TypeDef initialLed) {
	STM_EVAL_LEDInit(LED_BLUE);
	STM_EVAL_LEDInit(LED_GREEN);
	STM_EVAL_LEDInit(LED_ORANGE);
	STM_EVAL_LEDInit(LED_RED);
	
	STM_EVAL_LEDOn(initialLed);
}

void blinkLED(Led_TypeDef led) {
	STM_EVAL_LEDOn(led);
}

void resetAllLEDs() {
	STM_EVAL_LEDOff(LED_GREEN);
	STM_EVAL_LEDOff(LED_ORANGE);
	STM_EVAL_LEDOff(LED_RED);
	STM_EVAL_LEDOff(LED_BLUE);
}
