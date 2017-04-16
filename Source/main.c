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

#define FIXED_PRIORITY
//#define EARLIEST_DEADLINE_FIRST
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
void vBrewCoffeeType(void *);
void vScheduler(void *);

const static Coffee INITIAL_COFFEE = ESPRESSO;
const static uint32_t PRIORITY_BUTTON = tskIDLE_PRIORITY + 4;
const static uint32_t PRIORITY_BREW = tskIDLE_PRIORITY + 1;
const static uint32_t PRIORITY_SCHEDULER = tskIDLE_PRIORITY + 3;

static xSemaphoreHandle xTaskTableSemaphore;

static TaskHandle_t xBrewTasks[LEDn];
static uint32_t brewCounters[LEDn];

static Coffee coffees[LEDn] = {LATTE, ESPRESSO, MOCHA, CAPPUCCINO};

uint32_t missedDeadlines = 0;

typedef struct {
	int32_t priority;
	Coffee type;
	uint32_t scheduled;
	int32_t deadline;
	int32_t remainingWork;
	int32_t started;
	int32_t startTime;
} CoffeeTask;

static CoffeeTask taskTable[LEDn] = {{0, LATTE, 0, 0, 0, 0, 0},{0, ESPRESSO, 0, 0, 0, 0, 0},{0, MOCHA, 0, 0, 0, 0, 0},{0, CAPPUCCINO, 0, 0, 0, 0, 0}};
static int32_t ticks = 0;

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
	int32_t i;
	
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
		xTaskCreate( vBrewCoffeeType, (const char*)"Brew Type", 
			STACK_SIZE_MIN, (void *)&coffees[i], PRIORITY_BREW, &xBrewTasks[i]);
		vTaskSuspend(xBrewTasks[i]);
	}
	
	xTaskCreate( vScheduler, (const char*)"Scheduler", 
		STACK_SIZE_MIN, NULL, PRIORITY_SCHEDULER, NULL );
	xTaskCreate( vButtonUpdate, (const char*)"Button Update", 
		STACK_SIZE_MIN, NULL, PRIORITY_BUTTON, NULL );
	vTaskStartScheduler();
	
	// Should not reach here
	for(;;);
}

/* 
 * Set priorities in the taskTable using a fixed priority algorithm.
 */
void schedule_FixedPriority(Coffee *scheduled, uint32_t scheduledCount) {
	int32_t i;
	
	for(i = 0; i < scheduledCount; i++) {
		taskTable[scheduled[i]].priority = getCoffeePriority(scheduled[i]);
	}
}

/* 
 * Set priorities in the taskTable using an earliest deadline first algorithm.
 */ 
void schedule_EarliestDeadlineFirst(Coffee *scheduled, uint32_t scheduledCount) {
	int32_t i;
	
	for(i = 0; i < scheduledCount; i++) {
		// Make this negative so that the numerically smallest (aka earliest) deadline gets
		// picked by getHighestPriorityTask
		taskTable[scheduled[i]].priority = -(taskTable[scheduled[i]].deadline);
	}
}

/* 
 * Set priorities in the taskTable using a least laxity first algorithm.
 */ 
void schedule_LeastLaxityFirst(Coffee *scheduled, uint32_t scheduledCount) {
	int32_t i;
	
	for(i = 0; i < scheduledCount; i++) {
		// Make this negative so that the numerically smallest laxity gets
		// picked by getHighestPriorityTask
		taskTable[scheduled[i]].priority = -(taskTable[scheduled[i]].deadline - taskTable[scheduled[i]].remainingWork);
	}
}


/* 
 * Retrieve the highest priority task from the taskTable.
 * Note that this is a different priority than the ones we're given in the assignment.
 * This priority is calculated based on the scheduling algorithm.
 */
Coffee getHighestPriorityTask() {
	int32_t i;
	Coffee selectedTypeToBrew = DEFAULT_COFFEE;
	int32_t maxPriority = 0x80000000; // Set to minimum 32 bit int
	
	for(i = 0; i < LEDn; i++) {
		if(taskTable[i].scheduled > 0 && 
				(taskTable[i].priority > maxPriority || 
				(taskTable[i].priority == maxPriority && getCoffeePriority(taskTable[i].type) > getCoffeePriority(selectedTypeToBrew)))) {
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
	int32_t i;
	for(i = 0; i < LEDn; i++) {
		vTaskSuspend(xBrewTasks[i]);
	}
}

/*
 * Stop brewing a Coffee type and play a sound to alert the user.
 */
void endBrew(Coffee coffee) {
	// clean up
	brewCounters[coffee] = 0;
	taskTable[coffee].scheduled--;
	
	if(ticks > taskTable[coffee].deadline) {
		missedDeadlines++;
	}
	
	turnOffLED(getLEDForCoffeeType(coffee));
	
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
		
		if(brewCounters[coffeeType] % (BLINK_TOGGLE * 2) == 0) {
			taskTable[coffeeType].remainingWork--;
		}
		
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
	vTaskResume(xBrewTasks[coffee]);
}

/*
 * Run every second to reevaluate which coffee should be brewing depending
 * on each coffee tpye's deadline, period and priority.
 */
void vScheduler(void *pvParameters) {
	Coffee scheduled[LEDn];
	int32_t scheduledCount;
	Coffee selectedCoffeeToBrew;
	int32_t i;
	uint32_t missedDeadlinesCopy; // leaving this here for debugging/demo purposes
	int32_t ticksSinceStart = -1;
	
	for(;;) {
		if(xSemaphoreTake(xTaskTableSemaphore, 0)) {	
			missedDeadlinesCopy = missedDeadlines;
			
			// Schedule coffees to brew if their period is reached
			for(i = 0; i < LEDn; i++) {			
				if(taskTable[i].started && 
					((ticks - taskTable[i].startTime) % getCoffeePeriod(taskTable[i].type) == 0)) {
					if(ticksSinceStart == -1) {
						ticksSinceStart = 0;
					}
					taskTable[i].scheduled++;
					taskTable[i].deadline = ticks + getCoffeeDeadline(taskTable[i].type);
					taskTable[i].remainingWork = getBrewDurations(taskTable[i].type) / 1000; // Convert ms to s
				}
			}
		
			scheduledCount = 0;
			for(i = 0; i < LEDn; i++) {			
				if(taskTable[i].scheduled > 0) {
					scheduled[scheduledCount] = taskTable[i].type;
					scheduledCount++;
				}
			}
		
#ifdef FIXED_PRIORITY
			schedule_FixedPriority(scheduled, scheduledCount);
#elif defined(EARLIEST_DEADLINE_FIRST)
			schedule_EarliestDeadlineFirst(scheduled, scheduledCount);
#elif defined(LEAST_LAXITY_FIRST)
			schedule_LeastLaxityFirst(scheduled, scheduledCount);
#endif
		
			selectedCoffeeToBrew = getHighestPriorityTask();
		
			if(selectedCoffeeToBrew != DEFAULT_COFFEE) {	
				pauseAllBrews();
				brewCoffeeType(selectedCoffeeToBrew);		
			}
		
			ticks++;
			if(ticksSinceStart != -1) {
				ticksSinceStart++;
			}
			
			// Set a break point here to see how many deadlines we missed after 100 cycles.
			if(ticksSinceStart > 100) {
				i = missedDeadlinesCopy; // I know what you're thinking, we need to use missedDeadlinesCopy or else we can't view it in the debugger.
				while(1);  
			}
			
			xSemaphoreGive(xTaskTableSemaphore);
			vTaskDelay( SCHEDULER_DELAY / portTICK_RATE_MS );
		}
	}
}

void startCoffeeType(Coffee type) {
	taskTable[type].started = 1;
	taskTable[type].startTime = ticks;
	turnOffLED(getLEDForCoffeeType(type));
}

void startAllCoffees() {
	int32_t i;
	
	for(i = 0; i < LEDn; i++) {
		startCoffeeType(coffees[i]);
	}
}

/*
 * Move the user through the Coffee selection menu skipping any Coffees that are already being brewed.
 */
void selectNextCoffee() {
	Led_TypeDef selectedLED;
	uint16_t count = 0;
	while(taskTable[changeSelected()].started == 1) {
		if(++count > 4) {
			return;
		}
	}
	selectedLED = getLEDForSelected();
	resetAllLEDs();
	turnOnLED(selectedLED);
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
			TM_DelayMillis(10);
		} else if(debounce_count > LONG_PRESS_THRESHOLD) {
#ifdef DIFFERENT_START_TIME
			startCoffeeType(getSelectedCoffee());
#endif
			debounce_count = 0;
			vTaskDelay(200 / portTICK_RATE_MS);
		} else if(debounce_count > SHORT_PRESS_THRESHOLD) {
#ifdef SAME_START_TIME
			startAllCoffees();
#elif defined(DIFFERENT_START_TIME)
			selectNextCoffee();
#endif
			debounce_count = 0;
			vTaskDelay(200 / portTICK_RATE_MS);
		}
		else {
			debounce_count = 0;
			vTaskDelay(BUTTON_DELAY / portTICK_RATE_MS);
		}
	}
}
