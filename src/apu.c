// https://github.com/libsdl-org/SDL/blob/main/examples/audio/01-simple-playback/simple-playback.c

#include "apu.h"

#include "SDL3/SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

#include "cpu.h"
#include "ppu.h"
#include "debug.h"

SDL_AudioStream* stream = NULL;

#define CPU_FREQ 1789773
#define SAMPLE_RATE 48000
#define BUFFER_SIZE 4096

struct envStruct {
	uint8_t constantVolFlag;
	uint8_t volume;
	uint8_t timer;
	uint8_t decayCounter;
	uint8_t startFlag;
	uint8_t loop;
};

struct {
	struct {
		uint16_t timer;
		int16_t timerPeriod;
		int16_t targetPeriod;
		uint8_t counter;
		uint8_t loop;
		uint8_t duty;
		uint8_t dutyCycleProgress;
		struct {
			uint8_t enabled;
			uint8_t shiftCount;
			uint8_t timer;
			uint8_t timerPeriod;
			uint8_t negate;
			uint8_t reloadFlag;
		} sweep;
		struct envStruct env;
		uint8_t enabled;
		uint8_t mute;
	} pulse[2];
	struct {
		uint16_t lfsr;
		uint16_t timer;
		uint8_t timerPeriod;
		uint8_t counter;
		uint8_t mode;
		uint8_t lengthCounterLoop;
		struct envStruct env;
		uint8_t enabled;
	} noise;
	struct {
		uint16_t timer;
		uint16_t timerPeriod;
		uint8_t lengthCounter;
		uint8_t linearCounter;
		uint8_t progress;
		uint8_t reloadFlag;
		uint8_t reloadValue;
		uint8_t controlFlag;
		struct envStruct env;
		uint8_t enabled;
	} tri;
	struct {
		uint16_t sampleAddress;
		uint16_t sampleLength;
		uint16_t currentAddress;
		uint8_t irqEnable;
		uint8_t loopFlag;
		uint8_t rateIndex;
		uint16_t timer;
		uint8_t sampleBuffer;
	} dmc;
	uint64_t frameCounter;
	uint64_t cycles;
	uint8_t mode;
	uint8_t irqInhibit;

} apu;

uint32_t currentSample;
float samples[BUFFER_SIZE];

void initAPU(void) {
	SDL_AudioSpec spec;

	if(SDL_Init(SDL_INIT_AUDIO) == 0) {
		printf("could not initialize SDL's audio\n");
		exit(1);
	}

	spec.channels = 1;
	spec.format = SDL_AUDIO_F32;
	spec.freq = SAMPLE_RATE;

	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
	if(stream == NULL) {
		printf("could not create audio stream\n");
		exit(1);
	}
	SDL_ResumeAudioStreamDevice(stream);

	apu.noise.lfsr = 1;
}

// https://www.nesdev.org/wiki/APU_Pulse
uint8_t pulseDutyCycleLUT[] = {
	0x01,
	0x03,
	0x0F,
	0xFC,
};
float pulseGetSample(uint8_t index) {
	if(apu.pulse[index].mute) { return 0.0f; }
	if(apu.pulse[index].counter != 0 && apu.pulse[index].timerPeriod > 8) {
		if((pulseDutyCycleLUT[apu.pulse[index].duty] >> apu.pulse[index].dutyCycleProgress) & 1) {
			if(apu.pulse[index].env.constantVolFlag) {
				return apu.pulse[index].env.volume/256.0;
			} else {
				return apu.pulse[index].env.decayCounter/256.0;
			}
		}
	}
	return 0.0f;
}

// https://www.nesdev.org/wiki/APU_Noise
uint16_t noiseTimerLUT[] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};
float noiseGetSample(void) {
	if(apu.noise.counter > 0 && !(apu.noise.lfsr & 1)) {
		if((apu.noise.lfsr & 1) == 0) {
			if(apu.noise.env.constantVolFlag) {
				return (apu.noise.env.volume/256.0);
			} else {
				return (apu.noise.env.decayCounter/256.0);
			}
		}
	}
	return 0.0f;
}

// https://www.nesdev.org/wiki/APU_Triangle
uint8_t triLUT[] = {
	15, 14, 13, 12, 11, 10,  9,  8,  7,  6,  5,  4,  3,  2,  1,  0,
	 0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14, 15
};

float triGetSample(void) {
	return (triLUT[apu.tri.progress]/16.0f)/8.0f;
}

float dmcGetSample(void) {
}

void updateSweeps(void) {
	for(uint8_t i = 0; i < 2; ++i) {
		if(apu.pulse[i].sweep.timer == 0 && apu.pulse[i].sweep.enabled && apu.pulse[i].sweep.shiftCount > 0) {
			if(!apu.pulse[i].mute) {
				apu.pulse[i].timerPeriod = apu.pulse[i].targetPeriod;
			}
		}
		if(apu.pulse[i].sweep.timer == 0 || apu.pulse[i].sweep.reloadFlag) {
			apu.pulse[i].sweep.timer = apu.pulse[i].sweep.timerPeriod;
			apu.pulse[i].sweep.reloadFlag = 0;
		} else {
			--apu.pulse[i].sweep.timer;
		}
	}
}

void updateLengthCounters(void) {
	if(apu.pulse[0].enabled && !apu.pulse[0].loop && apu.pulse[0].counter != 0) {
		--apu.pulse[0].counter;
	}
	if(apu.pulse[1].enabled && !apu.pulse[1].loop && apu.pulse[1].counter != 0) {
		--apu.pulse[1].counter;
	}
	if(apu.noise.enabled && apu.noise.counter > 0) {
		--apu.noise.counter;
	}
	if(apu.tri.enabled && !apu.tri.controlFlag && apu.tri.lengthCounter > 0) {
		--apu.tri.lengthCounter;
	}
}

void updateLinearCounter(void) {
	if(apu.tri.reloadFlag) {
		apu.tri.linearCounter = apu.tri.reloadValue;
	} else {
		if(apu.tri.linearCounter > 0) {
			--apu.tri.linearCounter;
		}
	}
	if(!apu.tri.controlFlag) {
		apu.tri.reloadFlag = 0;
	}
}

void updateEnv(struct envStruct* env) {
	if(env->startFlag == 0) {
		if(env->timer > 0) {
			--env->timer;
		} else {
			env->timer = env->volume;
			if(env->decayCounter > 0) {
				--env->decayCounter;
			} else {
				if(env->loop) {
					env->decayCounter = 15;
				}
			}
		}
	} else {
		env->decayCounter = 15;
		env->timer = env->volume;
		env->startFlag = 0;
	}
}

void updateEnvelopes(void) {
	updateEnv(&apu.pulse[0].env);
	updateEnv(&apu.pulse[1].env);
	updateEnv(&apu.noise.env);
}

void apuStep(void) {
	for(uint8_t i = 0; i < 2; ++i) {
		int16_t change = apu.pulse[i].timerPeriod >> apu.pulse[i].sweep.shiftCount;
		if(apu.pulse[i].sweep.negate) {
			change *= -1;
			if(i == 0) {
				--change;
			}
		}
		apu.pulse[i].targetPeriod = apu.pulse[i].timerPeriod + change;
		if(apu.pulse[i].targetPeriod < 0) {
			apu.pulse[i].targetPeriod = 0;
		}
		if(apu.pulse[i].timerPeriod < 8 || apu.pulse[i].targetPeriod > 0x7FF) {
			apu.pulse[i].mute = 1;
		} else {
			apu.pulse[i].mute = 0;
		}
	}
	if(apu.cycles%2 == 0) {
		if(apu.pulse[0].timer > 0) {
			--apu.pulse[0].timer;
		} else {
			--apu.pulse[0].dutyCycleProgress;
			apu.pulse[0].dutyCycleProgress %= 8;
			apu.pulse[0].timer = apu.pulse[0].timerPeriod;
		}
		if(apu.pulse[1].timer > 0) {
			--apu.pulse[1].timer;
		} else {
			--apu.pulse[1].dutyCycleProgress;
			apu.pulse[1].dutyCycleProgress %= 8;
			apu.pulse[1].timer = apu.pulse[1].timerPeriod;
		}
	}
	if(apu.noise.timer > 0) {
		--apu.noise.timer;
	} else {
		uint8_t feedback = apu.noise.lfsr & 1;
		if(apu.noise.mode == 0) {
			feedback ^= (apu.noise.lfsr >> 1) & 1;
		} else {
			feedback ^= (apu.noise.lfsr >> 6) & 1;
		}
		apu.noise.lfsr >>= 1;
		apu.noise.lfsr |= feedback << 14;

		apu.noise.timer = apu.noise.timerPeriod;
	}

	if(apu.tri.linearCounter > 0 && apu.tri.lengthCounter > 0) {
		if(apu.tri.timer > 0) {
			--apu.tri.timer;
		} else {
			apu.tri.timer = apu.tri.timerPeriod;
			++apu.tri.progress;
			apu.tri.progress %= 32;
		}
	}


	++apu.cycles;
	++apu.frameCounter;

	if(apu.frameCounter % (3728*2)== 0) {
		switch(apu.frameCounter / (3728*2)) {
			case 1:
			case 3:
				updateLinearCounter();
				updateEnvelopes();
				break;
			case 2:
				updateLinearCounter();
				updateEnvelopes();
				updateSweeps();
				updateLengthCounters();
				break;
			case 4:
				if(apu.mode == 0) {
					updateLinearCounter();
					updateEnvelopes();
					updateSweeps();
					updateLengthCounters();
					apu.frameCounter = 0;
				}
				break;
			case 5:
				updateLinearCounter();
				updateEnvelopes();
				updateSweeps();
				updateLengthCounters();
				apu.frameCounter = 0;
				break;
		}
	}


	// need to do actual resampling at some point instead of this lmao
	if(apu.cycles % (CPU_FREQ/SAMPLE_RATE) == 0) {
		if(currentSample > BUFFER_SIZE-1) {
			if(SDL_GetAudioStreamQueued(stream) < (int)(SAMPLE_RATE * sizeof(float))/32) {
				SDL_PutAudioStreamData(stream, samples, sizeof(samples));
				currentSample = 0;
			}
		} else {
			samples[currentSample] = 0.0f;
			samples[currentSample] += pulseGetSample(0);
			samples[currentSample] += pulseGetSample(1);
			samples[currentSample] += noiseGetSample();
			samples[currentSample] += triGetSample();
			++currentSample;
		}
	}
}

uint8_t lengthCounterLUT[] = {
	10,254, 20,  2, 40,  4, 80,  6, 160,  8, 60, 10, 14, 12, 26, 14,
	12, 16, 24, 18, 48, 20, 96, 22, 192, 24, 72, 26, 16, 28, 32, 30
};

void pulseSetVolume(uint8_t index, uint8_t volume) {
	apu.pulse[index].env.volume = volume;
}

void pulseSetLoop(uint8_t index, uint8_t loop) {
	apu.pulse[index].loop = loop;
}


void pulseSetTimerLow(uint8_t index, uint8_t timerLow) {
	apu.pulse[index].timerPeriod &= 0xFF00;
	apu.pulse[index].timerPeriod |= timerLow;
}

void pulseSetTimerHigh(uint8_t index, uint8_t timerHigh) {
	apu.pulse[index].timerPeriod &= 0x00FF;
	apu.pulse[index].timerPeriod |= timerHigh << 8;
	apu.pulse[index].dutyCycleProgress = 0;
}

void pulseSetLengthCounter(uint8_t index, uint8_t counter) {
	apu.pulse[index].counter = lengthCounterLUT[counter];
	apu.pulse[index].env.startFlag = 1;
}

void pulseSetDutyCycle(uint8_t index, uint8_t duty) {
	apu.pulse[index].duty = duty;
}

void pulseSetEnableFlag(uint8_t index, uint8_t flag) {
	if(flag) {
		apu.pulse[index].enabled = 1;
	} else {
		apu.pulse[index].enabled = 0;
		apu.pulse[index].counter = 0;
	}
}

void pulseSetSweepEnable(uint8_t index, uint8_t flag) {
	apu.pulse[index].sweep.enabled = flag;
	apu.pulse[index].sweep.reloadFlag = 1;
}

void pulseSetSweepTimer(uint8_t index, uint8_t timer) {
	apu.pulse[index].sweep.timerPeriod = timer;
}

void pulseSetSweepNegate(uint8_t index, uint8_t flag) {
	apu.pulse[index].sweep.negate = flag;
}

void pulseSetSweepShift(uint8_t index, uint8_t shift) {
	apu.pulse[index].sweep.shiftCount = shift;
}

void pulseSetConstVolFlag(uint8_t index, uint8_t flag) {
	apu.pulse[index].env.constantVolFlag = flag;
}

void noiseSetTimer(uint8_t timer) {
	timer &= 0x0F;
	apu.noise.timerPeriod = noiseTimerLUT[timer];
	//apu.noise.timer = noiseTimerLUT[timer];
}

void noiseSetLengthcounter(uint8_t counter) {
	apu.noise.counter = lengthCounterLUT[counter];
	apu.noise.env.startFlag = 1;
}

void noiseSetEnableFlag(uint8_t flag) {
	if(flag) {
		apu.noise.enabled = 1;
	} else {
		apu.noise.enabled = 0;
		apu.noise.counter = 0;
	}
}

void noiseSetVolume(uint8_t volume) {
	apu.noise.env.volume = volume;
}

void noiseSetConstVolFlag(uint8_t flag) {
	apu.noise.env.constantVolFlag = flag;
}

void noiseSetLoop(uint8_t flag) {
	apu.noise.env.loop = flag;
	apu.noise.lengthCounterLoop = flag;
}

void noiseSetMode(uint8_t mode) {
	apu.noise.mode = mode;
}

void triSetTimerLow(uint8_t timerLow) {
	apu.tri.timerPeriod &= 0xFF00;
	apu.tri.timerPeriod |= timerLow;
}

void triSetTimerHigh(uint8_t timerHigh) {
	apu.tri.timerPeriod &= 0x00FF;
	apu.tri.timerPeriod |= timerHigh << 8;
}

void triSetLengthCounter(uint8_t counter) {
	apu.tri.lengthCounter = lengthCounterLUT[counter];
}

void triSetCounterReload(uint8_t reload) {
	apu.tri.reloadValue = reload;
}

void triSetEnableFlag(uint8_t flag) {
	if(flag) {
		apu.tri.enabled = 1;
	} else {
		apu.tri.enabled = 0;
		apu.tri.lengthCounter = 0;
	}
}

void triSetReloadFlag(uint8_t flag) {
	if(flag) {
		apu.tri.reloadFlag = 1;
	} else {
		apu.tri.reloadFlag = 0;
	}
}

void triSetControlFlag(uint8_t flag) {
	if(flag) {
		apu.tri.controlFlag = 1;
	} else {
		apu.tri.controlFlag = 0;
	}
}

void apuSetFrameCounterMode(uint8_t byte) {
	apu.frameCounter = 0;
	apu.mode = byte >> 7;
	apu.irqInhibit = (byte >> 6) & 1;
	if(apu.mode == 1) {
		updateLinearCounter();
		updateEnvelopes();
		updateSweeps();
		updateLengthCounters();
	}
}

uint8_t apuGetStatus(void) {
	uint8_t status = 0;
	status |= (apu.pulse[0].counter > 0) << 0;
	status |= (apu.pulse[1].counter > 0) << 1;
	status |= (apu.tri.lengthCounter > 0) << 2;
	status |= (apu.noise.counter > 0) << 3;
	cpu.irq = 1;
	return status;
}


void apuPrintDebug(void) {
	drawDebugText(0, 0, "\
%i\n\
P1: %i %i %i\n\
P2: %i %i %i\n\
N: %i %i %i\n\
T: %i %i %i", 
		apu.cycles,
		apu.pulse[0].env.volume, apu.pulse[0].env.decayCounter, apu.pulse[0].timerPeriod,
		apu.pulse[1].env.volume, apu.pulse[1].env.decayCounter, apu.pulse[1].timerPeriod,
		apu.noise.counter, apu.noise.timerPeriod, apu.noise.env.decayCounter,
		apu.tri.linearCounter, apu.tri.lengthCounter, apu.tri.timerPeriod);
}
