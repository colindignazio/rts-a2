#include "led.h"

#include "FreeRTOS.h"
#include "task.h"

#define BLINK_TOGGLE 500

void initializeLEDs(Led_TypeDef initialLed) {
	STM_EVAL_LEDInit(LED_BLUE);
	STM_EVAL_LEDInit(LED_GREEN);
	STM_EVAL_LEDInit(LED_ORANGE);
	STM_EVAL_LEDInit(LED_RED);
	
	STM_EVAL_LEDOn(initialLed);
}

void blinkLED(Led_TypeDef led) {
	STM_EVAL_LEDToggle(led);
	vTaskDelay( BLINK_TOGGLE / portTICK_RATE_MS );
}

void resetAllLEDs() {
	STM_EVAL_LEDOff(LED_GREEN);
	STM_EVAL_LEDOff(LED_ORANGE);
	STM_EVAL_LEDOff(LED_RED);
	STM_EVAL_LEDOff(LED_BLUE);
}

void turnOnLED(Led_TypeDef led) {
	STM_EVAL_LEDOn(led);
}

void turnOffLED(Led_TypeDef led) {
	STM_EVAL_LEDOff(led);
}
