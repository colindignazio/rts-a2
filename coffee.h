#ifndef _COFFEE_H
#define _COFFEE_H

#include "discoveryf4utils.h"

typedef enum {
	ESPRESSO = 0,
	LATTE = 1,
	MOCHA = 2,
	BLACK = 3,
	DEFAULT_COFFEE = 4
} Coffee;

void initializeCoffee(Coffee);
Coffee getSelected(void);
Coffee changeSelected(void);
Led_TypeDef getLEDForSelected(void);

#endif
