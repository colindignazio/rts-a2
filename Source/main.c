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
#include "delay.h"
//******************************************************************************

void vButtonUpdate(void *);
void vLedBlinkBlue(void *);
void vLedBlinkRed(void *);
void vLedBlinkGreen(void *);
void vLedBlinkOrange(void *);
void vBrewCoffeeType(void *);
void vPlaySound(void *);

void selectNextCoffee(void);
void brewCoffee(void);

#define STACK_SIZE_MIN	128	/* usStackDepth	- the stack size DEFINED IN WORDS.*/

#define SHORT_PRESS_THRESHOLD 10
#define LONG_PRESS_THRESHOLD 100
#define BLINK_TOGGLE 500

// task delays in ms
#define SOUND_DELAY 100
#define BUTTON_DELAY 10

const static Coffee INITIAL_COFFEE = ESPRESSO;
const static uint32_t PRIORITY_BUTTON = tskIDLE_PRIORITY + 3;
const static uint32_t PRIORITY_BREW_COFFEE = tskIDLE_PRIORITY + 1;
const static uint32_t PRIORITY_SOUND = tskIDLE_PRIORITY + 2;

static xSemaphoreHandle xSoundSemaphore;

static xSemaphoreHandle xBrewSemaphores[LEDn];
static TaskHandle_t xBrewTasks[LEDn];
static uint32_t brewCounters[LEDn];

static Coffee leds[LEDn] = {ESPRESSO, LATTE, MOCHA, BLACK};

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
	
	// Initializations
	STM_EVAL_PBInit(BUTTON_USER, BUTTON_MODE_GPIO);
	initializeCoffee(INITIAL_COFFEE);
	initializeLEDs(getLEDForSelected());
	initializeSound();
	TM_Delay_Init();
	
	vSemaphoreCreateBinary( xSoundSemaphore );
	xSemaphoreTake(xSoundSemaphore, 0);
	
	// Create a brew task and semaphore for each Coffee type
	for(i = 0; i < LEDn; i++) {
		vSemaphoreCreateBinary(xBrewSemaphores[i]);
		xTaskCreate( vBrewCoffeeType, (const char*)"Brew Coffee Type", 
			STACK_SIZE_MIN, (void *)&leds[i], PRIORITY_BREW_COFFEE, &xBrewTasks[i]);
		vTaskSuspend(xBrewTasks[i]);
	}
	
	xTaskCreate( vButtonUpdate, (const char*)"Button Update", 
		STACK_SIZE_MIN, NULL, PRIORITY_BUTTON, NULL );
	xTaskCreate( vPlaySound, (const char*)"Play Sound", 
		STACK_SIZE_MIN, NULL, PRIORITY_SOUND, NULL );
	vTaskStartScheduler();
	
	// Should not reach here
	for(;;);
}

/*
 * Plays a sound to alert that a brew has completed.
 * Can sound weird sometimes due to FreeRTOS preempting while the sound is playing.
 */
void vPlaySound(void *pvParameters) {
	for(;;) {
		if(xSemaphoreTake(xSoundSemaphore, 0) == pdTRUE) {
			playSound();
		}
		vTaskDelay( SOUND_DELAY / portTICK_RATE_MS );
	}
}

/* 
 * Poll the button for input. If a short click is detecting change the selection.
 * If a long click is selected begin brewing.
 */
void vButtonUpdate(void *pvParameters) {
	uint32_t debounce_count = 0;
	
	for(;;) {		
		if(STM_EVAL_PBGetState(BUTTON_USER)) {
			debounce_count++;
		} else if(debounce_count > LONG_PRESS_THRESHOLD) {
			brewCoffee();
			debounce_count = 0;
			vTaskDelay(200 / portTICK_RATE_MS);
		} else if(debounce_count > SHORT_PRESS_THRESHOLD) {
			selectNextCoffee();
			debounce_count = 0;
			vTaskDelay(200 / portTICK_RATE_MS);
		}
		else {
			debounce_count = 0;
		}
		vTaskDelay(BUTTON_DELAY / portTICK_RATE_MS);
	}
}

/*
 * Stop brewing a Coffee type and play a sound to alert the user.
 */
void endBrew(Coffee coffee) {
	prepareSound();
	turnOffLED(getLEDForCoffeeType(coffee));
	xSemaphoreGive(xBrewSemaphores[coffee]);
	xSemaphoreGive(xSoundSemaphore);
	vTaskSuspend(xBrewTasks[coffee]);
}

/*
 * Brews a Coffee type, blinking the corresponding LED to indicatre progress.
 * We use TM_DelayMillis here as a "busy wait" function. We need to give this
 * task something to do so that the CPU doesn't just preempt to the next task
 * after toggling the LED.
 */
void vBrewCoffeeType(void *pvParameters) {
	Coffee coffeeType = *((Coffee *)pvParameters);	
	Led_TypeDef ledForCoffeeType = getLEDForCoffeeType(coffeeType);
	
	for(;;) {
		blinkLED(ledForCoffeeType);
		TM_DelayMillis(BLINK_TOGGLE);
		brewCounters[coffeeType] += BLINK_TOGGLE;
		
		if(brewCounters[coffeeType] >= getBrewDurations(coffeeType)) {
			endBrew(coffeeType);
		}
	}
}

/*
 * Initialize the brew counter for the Coffee type being brewed and start the brewing tak.
 */
void brewCoffee() {
	Led_TypeDef selectedLED = getLEDForSelected();
			
	// Make sure this coffee is not already brewing
	if(xSemaphoreTake(xBrewSemaphores[selectedLED], 0) == pdTRUE) {
		brewCounters[selectedLED] = 0;
		vTaskResume(xBrewTasks[selectedLED]);
	}
}

/*
 * Move the user through the Coffee selection menu skipping any Coffees that are already being brewed.
 */
void selectNextCoffee() {
	Led_TypeDef selectedLED;
	while(uxSemaphoreGetCount(xBrewSemaphores[changeSelected()]) < 1);
	selectedLED = getLEDForSelected();
	resetAllLEDs();
	turnOnLED(selectedLED);
}
//******************************************************************************
