#ifndef APU_H
#define APU_H

#include <stdint.h>

void initAPU(void);


void apuStep(void);
// needs a better name
void apuFrameCheck(uint8_t cycles);

void apuSetFrameCounterMode(uint8_t byte);

uint8_t apuGetStatus(void);

void pulseSetVolume(uint8_t index, uint8_t volume);
void pulseSetTimerLow(uint8_t index, uint8_t timerLow);
void pulseSetTimerHigh(uint8_t index, uint8_t timerHigh);
void pulseSetLengthCounter(uint8_t index, uint8_t counter);
void pulseSetLoop(uint8_t index, uint8_t loop);
void pulseSetDutyCycle(uint8_t index, uint8_t duty);
void pulseSetEnableFlag(uint8_t index, uint8_t flag);

void pulseSetSweepEnable(uint8_t index, uint8_t flag);
void pulseSetSweepTimer(uint8_t index, uint8_t timer);
void pulseSetSweepNegate(uint8_t index, uint8_t flag);
void pulseSetSweepShift(uint8_t index, uint8_t shift);
void pulseSetConstVolFlag(uint8_t index, uint8_t flag);

void noiseSetEnableFlag(uint8_t flag);
void noiseSetLengthcounter(uint8_t counter);
void noiseSetTimer(uint8_t timer);
void noiseSetMode(uint8_t mode);
void noiseSetConstVolFlag(uint8_t flag);
void noiseSetVolume(uint8_t volume);
void noiseSetLoop(uint8_t flag);


void triSetEnableFlag(uint8_t flag);
void triSetTimerLow(uint8_t timerLow);
void triSetTimerHigh(uint8_t timerHigh);
void triSetLengthCounter(uint8_t counter);
void triSetCounterReload(uint8_t reload);
void triSetReloadFlag(uint8_t flag);
void triSetControlFlag(uint8_t flag);

void apuPrintDebug(void);

#endif // APU_H
