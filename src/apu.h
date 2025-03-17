#ifndef APU_H
#define APU_H

#include <stdint.h>

void initAPU(void);

// needs a better name
void apuFrameCheck(uint8_t cycles);

void apuSetFrameCounterMode(uint8_t byte);

void pulseSetVolume(uint8_t index, uint8_t volume);
void pulseSetTimerLow(uint8_t index, uint8_t timerLow);
void pulseSetTimerHigh(uint8_t index, uint8_t timerHigh);
void pulseSetLengthCounter(uint8_t index, uint8_t counter);
void pulseSetLoop(uint8_t index, uint8_t loop);

#endif // APU_H
