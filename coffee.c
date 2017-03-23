#include "coffee.h"

static Coffee selected;

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

Led_TypeDef getLEDForSelected() {
	return getCoffeeLED(selected);
}
