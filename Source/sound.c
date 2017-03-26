#include "sound.h"

#include "stm32f4xx.h"
#include "stm32f4xx_gpio.h"
#include "stm32f4xx_rcc.h"
#include "codec.h"

//Defines
#define NOTEFREQUENCY 0.015		//frequency of saw wave: f0 = 0.5 * NOTEFREQUENCY * 48000 (=sample rate)
#define NOTEAMPLITUDE 500.0		//amplitude of the saw wave

typedef struct {
	float tabs[8];
	float params[8];
	uint8_t currIndex;
} fir_8;

// struct to initialize GPIO pins
GPIO_InitTypeDef GPIO_InitStructure;

volatile uint32_t sampleCounter = 0;
volatile int16_t sample = 0;

double sawWave = 0.0;

double filteredSaw = 0.0;

float updateFilter(fir_8*, float);
void initFilter(fir_8*);

static fir_8 filt;

void initializeSound() {
	RCC_AHB1PeriphClockCmd(RCC_AHB1Periph_GPIOC, ENABLE);
	codec_init();
}

void prepareSound() {
	codec_ctrl_init();
	initFilter(&filt);
	I2S_Cmd(CODEC_I2S, ENABLE);
}

void playSound() {
	int i;
			for(i = 0; i < 3600000; i++) {
				if (SPI_I2S_GetFlagStatus(CODEC_I2S, SPI_I2S_FLAG_TXE)) {
					SPI_I2S_SendData(CODEC_I2S, sample);

					//only update on every second sample to insure that L & R ch. have the same sample value
					if (sampleCounter & 0x00000001) {	
						sawWave += NOTEFREQUENCY;
    
						if (sawWave > 1.0) {
							sawWave -= 2.0;
						}
						filteredSaw = updateFilter(&filt, sawWave);
						sample = (int16_t)(NOTEAMPLITUDE*filteredSaw);
					}
					sampleCounter++;
				}
			}
			GPIO_ResetBits(GPIOD, CODEC_RESET_PIN);		
}

void initFilter(fir_8* theFilter) {
	uint8_t i;

	theFilter->currIndex = 0;
	sampleCounter = 0;

	for (i=0; i<8; i++)
		theFilter->tabs[i] = 0.0;

	theFilter->params[0] = 0.01;
	theFilter->params[1] = 0.05;
	theFilter->params[2] = 0.12;
	theFilter->params[3] = 0.32;
	theFilter->params[4] = 0.32;
	theFilter->params[5] = 0.12;
	theFilter->params[6] = 0.05;
	theFilter->params[7] = 0.01;
}

float updateFilter(fir_8* filt, float val) {
	uint16_t valIndex;
	uint16_t paramIndex;
	float outval = 0.0;

	valIndex = filt->currIndex;
	filt->tabs[valIndex] = val;

	for (paramIndex=0; paramIndex<8; paramIndex++) {
		outval += (filt->params[paramIndex]) * (filt->tabs[(valIndex+paramIndex)&0x07]);
	}

	valIndex++;
	valIndex &= 0x07;

	filt->currIndex = valIndex;

	return outval;
}
