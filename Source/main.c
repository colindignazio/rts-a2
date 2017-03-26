//******************************************************************************
#include "stm32f4xx.h"
#include "stm32f4xx_conf.h"
#include "discoveryf4utils.h"

//******************************************************************************

//******************************************************************************
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "croutine.h"
#include "timers.h"
#include "semphr.h"

#include "coffee.h"
#include "led.h"
#include "sound.h"
//******************************************************************************

void vButtonUpdate(void *);
void vLedBlinkBlue(void *);
void vLedBlinkRed(void *);
void vLedBlinkGreen(void *);
void vLedBlinkOrange(void *);
void vBrewCoffeeType(void *);
void vTimerHandle(TimerHandle_t);
void vPlaySound(void *);

void selectNextCoffee(void);
void brewCoffee(void);

#define STACK_SIZE_MIN	128	/* usStackDepth	- the stack size DEFINED IN WORDS.*/

//times are all in ms
#define SHORT_PRESS 10
#define LONG_PRESS 100

const static Coffee INITIAL_COFFEE = ESPRESSO;

static xSemaphoreHandle xSoundSemaphore;

static xSemaphoreHandle xBrewSemaphores[LEDn];
static TaskHandle_t xBrewTask[LEDn];
static TimerHandle_t xBrewTimer[LEDn];

static Led_TypeDef leds[LEDn] = {LED_GREEN, LED_BLUE, LED_RED, LED_ORANGE};

//******************************************************************************
int main(void) {
	/*!< At this stage the microcontroller clock setting is already configured,
	   this is done through SystemInit() function which is called from startup
	   file (startup_stm32f4xx.s) before to branch to application main.
	   To reconfigure the default setting of SystemInit() function, refer to
	   system_stm32f4xx.c file
	 */
	
	/*!< Most systems default to the wanted configuration, with the noticeable 
		exception of the STM32 driver library. If you are using an STM32 with 
		the STM32 driver library then ensure all the priority bits are assigned 
		to be preempt priority bits by calling 
		NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 ); before the RTOS is started.
	*/
	int i;
	
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
	
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);
	
	initializeCoffee(INITIAL_COFFEE);
	initializeLEDs(getLEDForSelected());
	initializeSound();
	
	vSemaphoreCreateBinary( xSoundSemaphore );
	xSemaphoreTake(xSoundSemaphore, 0);
	
	for(i = 0; i < LEDn; i++) {
		vSemaphoreCreateBinary(xBrewSemaphores[i]);
		xTaskCreate( vBrewCoffeeType, (const char*)"Brew Coffee Type", 
			STACK_SIZE_MIN, (void *)&leds[i], tskIDLE_PRIORITY + 2, &xBrewTask[i]);
		vTaskSuspend(xBrewTask[i]);
		// Putting all brew tasks at the same priority makes RTOS handle them round robin
		xBrewTimer[i] = xTimerCreate("Brew Timer", getBrewDurations((Coffee) i) / portTICK_RATE_MS, pdFALSE, (void *)&leds[i], vTimerHandle);
	}
	xTaskCreate( vButtonUpdate, (const char*)"Button Update", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY + 1, NULL );
	xTaskCreate( vPlaySound, (const char*)"Play Sound", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY + 1, NULL );
	vTaskStartScheduler();
	
	// Should not reach here
	for(;;);
}

void vPlaySound(void *pvParameters) {
	for(;;) {
		if(xSemaphoreTake(xSoundSemaphore, 0) == pdTRUE) {
			playSound();
		}
		vTaskDelay( 100 / portTICK_RATE_MS );
	}
}

void vButtonUpdate(void *pvParameters) {
	uint32_t debounce_count = 0;
	
	// should be doing long/short clicks using timers but for now this works
	for(;;) {		
		if(STM_EVAL_PBGetState(BUTTON_USER)) {
			debounce_count++;
		} else if(debounce_count > LONG_PRESS) {
			brewCoffee();
			debounce_count = 0;
			vTaskDelay(200 / portTICK_RATE_MS);
		} else if(debounce_count > SHORT_PRESS) {
			selectNextCoffee();
			debounce_count = 0;
			vTaskDelay(200 / portTICK_RATE_MS);
		}
		else {
			debounce_count = 0;
		}
		
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}

// Stop brewing task, should play sound here as well
void vTimerHandle(TimerHandle_t xTimer) {
	Led_TypeDef led = *(Led_TypeDef *)(pvTimerGetTimerID(xTimer));
	prepareSound();
	turnOffLED(led);
	xSemaphoreGive(xBrewSemaphores[led]);
	xSemaphoreGive(xSoundSemaphore);
	vTaskSuspend(xBrewTask[led]);
}

void vBrewCoffeeType(void *pvParameters) {
	Led_TypeDef selectedLED = *((Led_TypeDef *)pvParameters);	
	
	for(;;) {
		blinkLED(selectedLED);
	}
}

void brewCoffee() {
	Led_TypeDef selectedLED;
	
			selectedLED = getLEDForSelected();
			
			// Make sure this coffee is not already brewing
			if(xSemaphoreTake(xBrewSemaphores[selectedLED], 0) == pdTRUE) {
				xTimerStart(xBrewTimer[selectedLED], 0);
				vTaskResume(xBrewTask[selectedLED]);
			}
}

void selectNextCoffee() {
	Led_TypeDef selectedLED;
	
			// Skip any coffees that are brewing
			while(uxSemaphoreGetCount(xBrewSemaphores[changeSelected()]) < 1);
			selectedLED = getLEDForSelected();
			resetAllLEDs();
			turnOnLED(selectedLED);
}
//******************************************************************************
