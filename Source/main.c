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
//******************************************************************************

void vButtonUpdate(void *);
void vLedBlinkBlue(void *);
void vLedBlinkRed(void *);
void vLedBlinkGreen(void *);
void vLedBlinkOrange(void *);
void vChangeSelection(void *);
void vBrewCoffee(void *);
void vBrewCoffeeType(void *);
void vTimerHandle(TimerHandle_t);

#define STACK_SIZE_MIN	128	/* usStackDepth	- the stack size DEFINED IN WORDS.*/

//times are all in ms
#define BLINK_TOGGLE 500
#define SHORT_PRESS 10
#define LONG_PRESS 100
#define DOUBLE_CLICK_TIME 500
#define ALERT_TIME 100


const static Coffee INITIAL_COFFEE = ESPRESSO;

TaskHandle_t taskHandleButton;
TimerHandle_t timerHandleButton;

static xSemaphoreHandle xButtonSemaphore;
static xSemaphoreHandle xBrewSemaphore;

static xSemaphoreHandle xBrewSemaphores[LEDn];
static TaskHandle_t xBrewTask[LEDn];
static TimerHandle_t xBrewTimer[LEDn];

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
	
	//need to do this better
	int leds[4] = {0, 1, 2, 3};
	
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
	
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);
	
	initializeCoffee(INITIAL_COFFEE);
	initializeLEDs(getLEDForSelected());
	
	timerHandleButton = 0;
	
	vSemaphoreCreateBinary( xButtonSemaphore );
	xSemaphoreTake(xButtonSemaphore, 0);
	vSemaphoreCreateBinary( xBrewSemaphore );
	xSemaphoreTake(xBrewSemaphore, 0);
	
	for(i = 0; i < LEDn; i++) {
		vSemaphoreCreateBinary(xBrewSemaphores[i]);
		xTaskCreate( vBrewCoffeeType, (const char*)"Brew Coffee Type", 
			STACK_SIZE_MIN, (void *)&leds[i], tskIDLE_PRIORITY + 2, &xBrewTask[i]);
		vTaskSuspend(xBrewTask[i]);
		xBrewTimer[i] = xTimerCreate("Brew Timer", getBrewDurations((Coffee) i) / portTICK_RATE_MS, pdFALSE, (void *)&leds[i], vTimerHandle);
	}
	
	xTaskCreate( vButtonUpdate, (const char*)"Button Update", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY + 1, taskHandleButton );
	xTaskCreate( vChangeSelection, (const char*)"Change Selection", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY + 1, NULL );
	xTaskCreate( vBrewCoffee, (const char*)"Brew Coffee", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY + 1, NULL );
	
	vTaskStartScheduler();
	
	// Should not reach here
	for(;;);
}

void vButtonUpdate(void *pvParameters) {
	uint32_t debounce_count = 0;
	
	// should be doing long/short clicks using timers but for now this works
	for(;;) {		
		if(STM_EVAL_PBGetState(BUTTON_USER)) {
			debounce_count++;
		} else if(debounce_count > LONG_PRESS) {
			xSemaphoreGive(xBrewSemaphore);
			debounce_count = 0;
			vTaskDelay(200 / portTICK_RATE_MS);
		} else if(debounce_count > SHORT_PRESS) {
			xSemaphoreGive(xButtonSemaphore);
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
	turnOffLED(led);
	xSemaphoreGive(xBrewSemaphores[led]);
	vTaskSuspend(xBrewTask[led]);
}

void vBrewCoffeeType(void *pvParameters) {
	Led_TypeDef selectedLED = *((Led_TypeDef *)pvParameters);	
	
	for(;;) {
		blinkLED(selectedLED);
	}
}

void vBrewCoffee(void *pvParameters) {
	Led_TypeDef selectedLED;
	
	for(;;) {
		if(xSemaphoreTake(xBrewSemaphore, 0) == pdTRUE) {
			selectedLED = getLEDForSelected();
			
			// Make sure this coffee is not already brewing
			if(xSemaphoreTake(xBrewSemaphores[selectedLED], 0) == pdTRUE) {
				xTimerStart(xBrewTimer[selectedLED], 0);
				vTaskResume(xBrewTask[selectedLED]);
			}
		}
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}

void vChangeSelection(void *pvParameters) {
	Led_TypeDef selectedLED;
	
	for(;;) {
		if(xSemaphoreTake(xButtonSemaphore, 0) == pdTRUE) {
			changeSelected();
			selectedLED = getLEDForSelected();
			resetAllLEDs();
			turnOnLED(selectedLED);
		}
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}
//******************************************************************************
