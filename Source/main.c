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

#define SAME_START_TIME
//#define DIFFERENT_START_TIME

//#define FIXED_PRIORITY
#define EARLIEST_DEADLINE_FIRST
//#define LEAST_LAXITY_FIRST

#define STACK_SIZE_MIN	128	/* usStackDepth	- the stack size DEFINED IN WORDS.*/

#define SHORT_PRESS_THRESHOLD 10
#define LONG_PRESS_THRESHOLD 100
#define BLINK_TOGGLE 500

// task delays in ms
#define SOUND_DELAY 100
#define BUTTON_DELAY 10
#define SCHEDULER_DELAY 1000

void vButtonUpdate(void *);
void vLedBlinkBlue(void *);
void vLedBlinkRed(void *);
void vLedBlinkGreen(void *);
void vLedBlinkOrange(void *);
void vBrewCoffeeType(void *);
void vPlaySound(void *);
void vScheduler(void *);

void selectNextCoffee(void);
void brewCoffeeType(Coffee);

const static Coffee INITIAL_COFFEE = ESPRESSO;
//const static uint32_t PRIORITY_BUTTON = tskIDLE_PRIORITY + 3;
const static uint32_t PRIORITY_BREW = tskIDLE_PRIORITY + 1;
const static uint32_t PRIORITY_SCHEDULER = tskIDLE_PRIORITY + 4;

static xSemaphoreHandle xBrewSemaphores[LEDn];
static xSemaphoreHandle xTaskTableSemaphore;

static TaskHandle_t xBrewTasks[LEDn];
static uint32_t brewCounters[LEDn];

static Coffee coffees[LEDn] = {LATTE, ESPRESSO, MOCHA, CAPPUCCINO};

typedef struct {
	int priority;
	Coffee type;
	uint32_t scheduled;
	uint32_t deadline;
} CoffeeTask;

static CoffeeTask taskTable[LEDn] = {{0, LATTE, 0, 0},{0, ESPRESSO, 0, 0},{0, MOCHA, 0, 0},{0, CAPPUCCINO, 0, 0}};
static int ticks = 0;

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
	initializeLEDs((Led_TypeDef) -1);
	initializeSound();
	TM_Delay_Init();
	
	xTaskTableSemaphore = xSemaphoreCreateBinary();
	xSemaphoreGive(xTaskTableSemaphore);
	
	// Create a brew task and semaphore for each Coffee type
	for(i = 0; i < LEDn; i++) {
		xBrewSemaphores[i] = xSemaphoreCreateBinary();
		xTaskCreate( vBrewCoffeeType, (const char*)"Brew Type", 
			STACK_SIZE_MIN, (void *)&coffees[i], PRIORITY_BREW, &xBrewTasks[i]);
		vTaskSuspend(xBrewTasks[i]);
		xSemaphoreGive(xBrewSemaphores[i]);
	}
	
	xTaskCreate( vScheduler, (const char*)"Scheduler", 
		STACK_SIZE_MIN, NULL, PRIORITY_SCHEDULER, NULL );
	//xTaskCreate( vButtonUpdate, (const char*)"Button Update", 
	//	STACK_SIZE_MIN, NULL, PRIORITY_BUTTON, NULL );
	vTaskStartScheduler();
	
	// Should not reach here
	for(;;);
}

/* 
 * Set priorities in the taskTable using a fixed priority algorithm.
 */
void schedule_FixedPriority(Coffee *scheduled, uint16_t scheduledCount) {
	int i;
	
	for(i = 0; i < scheduledCount; i++) {
		taskTable[scheduled[i]].priority = getCoffeePriority(scheduled[i]);
	}
}

/* 
 * Set priorities in the taskTable using an earliest deadline first algorithm.
 */ 
void schedule_EarliestDeadlineFirst(Coffee *scheduled, uint16_t scheduledCount) {
	int i;
	int k;
	int priority;
	for(i = 0; i < scheduledCount; i++) {
		priority = 4;
		for(k = 0; k < scheduledCount; k++) { //loop through tasks to set the right priority based on deadline
			if(i != k) {
				if (taskTable[scheduled[i]].deadline > taskTable[scheduled[k]].deadline) {
					priority--;
				}
			}
		}
		taskTable[scheduled[i]].priority = priority;
	}
}


/* 
 * Retrieve the highest priority task from the taskTable.
 * Note that this is a different priority than the ones we're given in the assignment.
 * This priority is calculated based on the scheduling algorithm.
 */
Coffee getHighestPriorityTask() {
	int i;
	Coffee selectedTypeToBrew = DEFAULT_COFFEE;
	int maxPriority = -1;
	
	for(i = 0; i < LEDn; i++) {
		if(taskTable[i].scheduled > 0 && taskTable[i].priority > maxPriority) {
			maxPriority = taskTable[i].priority;
			selectedTypeToBrew = taskTable[i].type;
		}
	}
	
	return selectedTypeToBrew;
}

/*
 * Pause all brews. Note that this does not "complete" any brews
 * since we don't reset any counters and we don't play sound.
 */
void pauseAllBrews() {
	int i;
	resetAllLEDs();
	for(i = 0; i < LEDn; i++) {
		vTaskSuspend(xBrewTasks[i]);
	}
}

/*
 * Run every second to reevaluate which coffee should be brewing depending
 * on each coffee tpye's deadline, period and priority.
 */
void vScheduler(void *pvParameters) {
	Coffee scheduled[LEDn];
	uint16_t scheduledCount;
	Coffee selectedCoffeeToBrew;
	int i;
	
	for(;;) {
		scheduledCount = 0;
		if(xSemaphoreTake(xTaskTableSemaphore, 0)) {	

			// Schedule coffees to brew if their period is reached
			for(i = 0; i < LEDn; i++) {			
				if(ticks % getCoffeePeriod(taskTable[i].type) == 0) {
					taskTable[i].scheduled++;
					taskTable[i].deadline = ticks + getCoffeeDeadline(taskTable[i].type);
				}
			}
		
			for(i = 0; i < LEDn; i++) {			
				if(taskTable[i].scheduled > 0) {
					scheduled[i] = taskTable[i].type;
					scheduledCount++;
				}
			}
		
			#ifdef FIXED_PRIORITY
			schedule_FixedPriority(scheduled, scheduledCount);
			#elif defined(EARLIEST_DEADLINE_FIRST)
			schedule_EarliestDeadlineFirst(scheduled, scheduledCount);
			#elif LEAST_LAXITY_FIRST
			//schedule_LeastLaxityFirst(scheduled, scheduledCount);
			#endif
		
			selectedCoffeeToBrew = getHighestPriorityTask();
		
			if(selectedCoffeeToBrew != DEFAULT_COFFEE) {	
				pauseAllBrews();
				brewCoffeeType(selectedCoffeeToBrew);		
			}
		
			ticks++;
			xSemaphoreGive(xTaskTableSemaphore);
			vTaskDelay( SCHEDULER_DELAY / portTICK_RATE_MS );
		}
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
			//brewCoffee();
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
	// clean up
	brewCounters[coffee] = 0;
	taskTable[coffee].scheduled--;
	turnOffLED(getLEDForCoffeeType(coffee));
	xSemaphoreGive(xBrewSemaphores[coffee]);
	
	// play sound
	prepareSound();
	playSound();
	
	// prevent task from running
	vTaskSuspend(xBrewTasks[coffee]);
}

/*
 * Brews a Coffee type, blinking the corresponding LED to indicate progress.
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
		vTaskDelay(10 / portTICK_RATE_MS);
	}
}

/*
 * Begin/resume brewing a coffee type
 */
void brewCoffeeType(Coffee coffee) {
	resetAllLEDs();
	vTaskResume(xBrewTasks[coffee]);
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
