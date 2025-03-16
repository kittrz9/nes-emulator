#ifndef APU_H
#define APU_H

#include <stdint.h>

void initAPU(void);
void apuLoop(void);

void pulseSetVolume(uint8_t index, uint8_t volume);
void pulseSetTimerLow(uint8_t index, uint8_t timerLow);
void pulseSetTimerHigh(uint8_t index, uint8_t timerHigh);

#endif // APU_H
