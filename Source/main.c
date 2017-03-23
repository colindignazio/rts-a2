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

void vButtonUpdate(void *pvParameters);
void vLedBlinkBlue(void *pvParameters);
void vLedBlinkRed(void *pvParameters);
void vLedBlinkGreen(void *pvParameters);
void vLedBlinkOrange(void *pvParameters);
void vChangeSelection(void *pvParameters);

#define STACK_SIZE_MIN	128	/* usStackDepth	- the stack size DEFINED IN WORDS.*/

#define SHORT_CLICK 100
#define butDEBOUNCE_DELAY	( 200 / portTICK_RATE_MS )

const static Coffee INITIAL_COFFEE = ESPRESSO;

TaskHandle_t taskHandleButton;
TimerHandle_t timerHandleButton;
static xSemaphoreHandle xButtonSemaphore;

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
	NVIC_PriorityGroupConfig( NVIC_PriorityGroup_4 );
	
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);
	
	initializeCoffee(INITIAL_COFFEE);
	initializeLEDs(getLEDForSelected());
	
	timerHandleButton = 0;
	
	vSemaphoreCreateBinary( xButtonSemaphore );
	xSemaphoreTake(xButtonSemaphore, 0);
	
	xTaskCreate( vButtonUpdate, (const char*)"Button Update", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY + 1, taskHandleButton );
	xTaskCreate( vChangeSelection, (const char*)"Change Selection", 
		STACK_SIZE_MIN, NULL, tskIDLE_PRIORITY, NULL );
	
	vTaskStartScheduler();
}

void vButtonUpdate(void *pvParameters) {
	uint32_t debounce_count = 0;
	
	for(;;) {		
		if(STM_EVAL_PBGetState(BUTTON_USER)) {
			debounce_count++;
			
			if(debounce_count == SHORT_CLICK) {
				xSemaphoreGive(xButtonSemaphore);
				debounce_count = 0;
				vTaskDelay(200 / portTICK_RATE_MS);
			}
		} else {
			debounce_count = 0;
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
