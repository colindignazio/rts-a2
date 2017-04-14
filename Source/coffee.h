#ifndef _COFFEE_H
#define _COFFEE_H

#include "discoveryf4utils.h"

typedef enum {
	LATTE = LED_GREEN,
	ESPRESSO = LED_BLUE,
	MOCHA = LED_RED,
	CAPPUCCINO = LED_ORANGE,
	DEFAULT_COFFEE = 4
} Coffee;

void initializeCoffee(Coffee);
Coffee changeSelected(void);
Led_TypeDef getCoffeeLED(Coffee);
Led_TypeDef getLEDForSelected(void);
Led_TypeDef getLEDForCoffeeType(Coffee);
uint32_t getBrewDurations(Coffee);
uint32_t getCoffeePriority(Coffee);
uint32_t getCoffeePeriod(Coffee);
uint32_t getCoffeeDeadline(Coffee);

#endif
