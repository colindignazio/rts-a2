#include "coffee.h"

#define BREW_TIME_LATTE 4000
#define BREW_TIME_ESPRESSO 3000
#define BREW_TIME_MOCHA 6000
#define BREW_TIME_CAPPUCCINO 4000

#define PRIORITY_LATTE 1
#define PRIORITY_ESPRESSO 3
#define PRIORITY_MOCHA 2
#define PRIORITY_CAPPUCCINO 4

#define PERIOD_LATTE 30
#define PERIOD_ESPRESSO 20
#define PERIOD_MOCHA 40
#define PERIOD_CAPPUCCINO 40

#define DEADLINE_LATTE 10
#define DEADLINE_ESPRESSO 5
#define DEADLINE_MOCHA 15
#define DEADLINE_CAPPUCCINO 20

static Coffee selected;

void initializeCoffee(Coffee defaultType) {
	selected = defaultType;
}

Coffee changeSelected() {
	selected = (Coffee)(((uint32_t)selected + 1) % 4);
	return selected;
}

Led_TypeDef getLEDForSelected() {
	return getLEDForCoffeeType(selected);
}

Led_TypeDef getLEDForCoffeeType(Coffee type) {
	switch(type) {
		case LATTE:
			return LED_GREEN;
		case ESPRESSO:
			return LED_BLUE;
		case MOCHA:
			return LED_RED;
		case CAPPUCCINO:
			return LED_ORANGE;
		default:
			return LED_GREEN;
	}
}

uint32_t getBrewDurations(Coffee type) {
	switch(type) {
		case LATTE:
			return BREW_TIME_LATTE;
		case ESPRESSO:
			return BREW_TIME_ESPRESSO;
		case MOCHA:
			return BREW_TIME_MOCHA;
		case CAPPUCCINO:
			return BREW_TIME_CAPPUCCINO;
		default:
			return 0;
	}
}

uint32_t getCoffeePriority(Coffee type) {
	switch(type) {
		case LATTE:
			return PRIORITY_LATTE;
		case ESPRESSO:
			return PRIORITY_ESPRESSO;
		case MOCHA:
			return PRIORITY_MOCHA;
		case CAPPUCCINO:
			return PRIORITY_CAPPUCCINO;
		default:
			return -1;
	}
}

uint32_t getCoffeePeriod(Coffee type) {
	switch(type) {
		case LATTE:
			return PERIOD_LATTE;
		case ESPRESSO:
			return PERIOD_ESPRESSO;
		case MOCHA:
			return PERIOD_MOCHA;
		case CAPPUCCINO:
			return PERIOD_CAPPUCCINO;
		default:
			return -1;
	}
}

uint32_t getCoffeeDeadline(Coffee type) {
	switch(type) {
		case LATTE:
			return DEADLINE_LATTE;
		case ESPRESSO:
			return DEADLINE_ESPRESSO;
		case MOCHA:
			return DEADLINE_MOCHA;
		case CAPPUCCINO:
			return DEADLINE_CAPPUCCINO;
		default:
			return -1;
	}
}
