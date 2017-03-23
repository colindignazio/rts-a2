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

#include "../coffee.h"
#include "../led.h"
//******************************************************************************

void vButtonUpdate(void *);
void vLedBlinkBlue(void *);
void vLedBlinkRed(void *);
void vLedBlinkGreen(void *);
void vLedBlinkOrange(void *);
void vChangeSelection(void *);
void vBrewCoffee(void *);

#define STACK_SIZE_MIN	128	/* usStackDepth	- the stack size DEFINED IN WORDS.*/

//times are all in ms
#define BLINK_TOGGLE 500
#define SHORT_PRESS 100
#define LONG_PRESS 1000000
#define DOUBLE_CLICK_TIME 500
#define ALERT_TIME 100

#define BREW_TIME_ESPRESSO 3000
#define BREW_TIME_LATTE 5000
#define BREW_TIME_MOCHA 7000
#define BREW_TIME_BLACK 10000

const static Coffee INITIAL_COFFEE = ESPRESSO;

TaskHandle_t taskHandleButton;
TimerHandle_t timerHandleButton;

static xSemaphoreHandle xButtonSemaphore;
static xSemaphoreHandle xBrewSemaphore;

static xSemaphoreHandle xBrewSemaphores[LEDn];

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
	
	timerHandleButton = 0;
	
	vSemaphoreCreateBinary( xButtonSemaphore );
	xSemaphoreTake(xButtonSemaphore, 0);
	vSemaphoreCreateBinary( xBrewSemaphore );
	xSemaphoreTake(xBrewSemaphore, 0);
	
	for(i = 0; i < LEDn; i++) {
		vSemaphoreCreateBinary(xBrewSemaphores[i]);
	}
	
	xTaskCreate( vButtonUpdate, (const char*)"Button Update", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY + 1, taskHandleButton );
	xTaskCreate( vChangeSelection, (const char*)"Change Selection", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY, NULL );
	xTaskCreate( vBrewCoffee, (const char*)"Brew Coffee", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY, NULL );
	
	vTaskStartScheduler();
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
	}
}

void vBrewCoffeeType(void *pvParameters) {
	Led_TypeDef selectedLED = *((Led_TypeDef *)pvParameters);
	
	for(;;) {
		// Code to brew goes here
	}
}

void vBrewCoffee(void *pvParameters) {
	Led_TypeDef selectedLED;
	
	for(;;) {
		if(xSemaphoreTake(xBrewSemaphore, 0) == pdTRUE) {
			selectedLED = getLEDForSelected();
			
			// Make sure this coffee is not already brewing
			if(xSemaphoreTake(xBrewSemaphores[selectedLED], 0) == pdTRUE) {
				xTaskCreate( vBrewCoffeeType, (const char*)"Brew Coffee Type", 
					STACK_SIZE_MIN, (void *) (void *)&selectedLED, tskIDLE_PRIORITY, NULL );
			}
			
		}
	}
}

void vChangeSelection(void *pvParameters) {
	Led_TypeDef selectedLED;
	
	for(;;) {
		if(xSemaphoreTake(xButtonSemaphore, 0) == pdTRUE) {
			changeSelected();
			selectedLED = getLEDForSelected();
			resetAllLEDs();
			blinkLED(selectedLED);
		}
	}
}
//******************************************************************************
