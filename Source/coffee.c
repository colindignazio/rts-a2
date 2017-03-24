#include "coffee.h"

#define BREW_TIME_ESPRESSO 3000
#define BREW_TIME_LATTE 5000
#define BREW_TIME_MOCHA 7000
#define BREW_TIME_BLACK 10000

static Coffee selected;

static Led_TypeDef getCoffeeLED(Coffee type) {
	switch(type) {
		case ESPRESSO:
			return LED_GREEN;
		case LATTE:
			return LED_ORANGE;
		case MOCHA:
			return LED_RED;
		case BLACK:
			return LED_BLUE;
		default:
			return LED_GREEN;
	}
}

void initializeCoffee(Coffee defaultType) {
	selected = defaultType;
}

Coffee getSelected() {
	return selected;
}

Coffee changeSelected() {
	selected = (Coffee)(((uint32_t)selected + 1) % 4);
	return selected;
}

Led_TypeDef getLEDForSelected() {
	return getCoffeeLED(selected);
}

uint32_t getCoffeeBrewTime(Coffee type) {
	return 1;
}

uint32_t getBrewDurations(Coffee type) {
	switch(type) {
		case ESPRESSO:
			return BREW_TIME_ESPRESSO;
		case LATTE:
			return BREW_TIME_LATTE;
		case MOCHA:
			return BREW_TIME_MOCHA;
		case BLACK:
			return BREW_TIME_BLACK;
		default:
			return 0;
	}
}
