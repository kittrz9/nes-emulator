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
#define BUFFER_SIZE 1024

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
		} sweep;
		struct envStruct env;
		uint8_t enabled;
	} pulse[2];
	struct {
		uint16_t lfsr;
		uint16_t timer;
		uint8_t timerPeriod;
		uint8_t counter;
		uint8_t mode;
		uint8_t lengthCounterLoop;
		struct envStruct env;
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
	} tri;
	uint8_t frameCounter;
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
	// unsure what the starting value of this is supposed to be, but this makes it not start out at full volume on super mario bros
	apu.noise.env.constantVolFlag = 1;
}

// https://www.nesdev.org/wiki/APU_Pulse
uint8_t pulseDutyCycleLUT[] = {
	0x01,
	0x03,
	0x0F,
	0xFC,
};
float pulseGetSample(uint8_t index) {
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

void updateSweeps(void) {
	if(apu.pulse[0].sweep.enabled) {
		if(apu.pulse[0].sweep.timer == 0) {
			uint16_t change = apu.pulse[0].timerPeriod >> apu.pulse[0].sweep.shiftCount;
			if(apu.pulse[0].sweep.negate) {
				change *= -1;
				--change;
			}
			apu.pulse[0].timerPeriod += change;
			if(apu.pulse[0].timerPeriod < 0) {
				apu.pulse[0].timerPeriod = 0;
			}
		} else {
			--apu.pulse[0].sweep.timer;
		}
	}
	if(apu.pulse[1].sweep.enabled) {
		if(apu.pulse[1].sweep.timer == 0) {
			uint16_t change = apu.pulse[1].timerPeriod >> apu.pulse[1].sweep.shiftCount;
			if(apu.pulse[1].sweep.negate) {
				change *= -1;
			}
			apu.pulse[1].timerPeriod += change;
			if(apu.pulse[1].timerPeriod < 0) {
				apu.pulse[1].timerPeriod = 0;
			}
		} else {
			--apu.pulse[1].sweep.timer;
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
	if(apu.noise.counter > 0) {
		--apu.noise.counter;
	}
	if(!apu.tri.controlFlag && apu.tri.lengthCounter > 0) {
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
	if(apu.cycles%2 == 0) {
		if(apu.pulse[0].timer > 0) {
			--apu.pulse[0].timer;
		} else {
			--apu.pulse[0].dutyCycleProgress;
			apu.pulse[0].dutyCycleProgress &= 0x0F;
			apu.pulse[0].timer = apu.pulse[0].timerPeriod;
		}
		if(apu.pulse[1].timer > 0) {
			--apu.pulse[1].timer;
		} else {
			--apu.pulse[1].dutyCycleProgress;
			apu.pulse[1].dutyCycleProgress &= 0x0F;
			apu.pulse[1].timer = apu.pulse[1].timerPeriod;
		}
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

		++apu.noise.timerPeriod;
		apu.noise.timerPeriod &= 0xF;
		apu.noise.timer = noiseTimerLUT[apu.noise.timerPeriod];
	}

	++apu.cycles;

	if(apu.mode == 0) {
		switch(apu.cycles % (14915*2)) {
			case 3728*2:
				updateLinearCounter();
				updateEnvelopes();
				break;
			case 7456*2:
				updateLinearCounter();
				updateEnvelopes();
				updateSweeps();
				updateLengthCounters();
				break;
			case 11185*2:
				updateLinearCounter();
				updateEnvelopes();
				break;
			case 0:
				updateLinearCounter();
				updateEnvelopes();
				updateSweeps();
				updateLengthCounters();
				if(!apu.irqInhibit) {
					cpu.irq = 0;
				}
				break;
			default:
				break;
		}
		apu.cycles %= (14915*2);
	} else {
		switch(apu.cycles % (18641*2)) {
			case 3278*2:
				updateLinearCounter();
				updateEnvelopes();
				break;
			case 7456*2:
				updateLinearCounter();
				updateEnvelopes();
				updateLengthCounters();
				updateSweeps();
				break;
			case 11185*2:
				updateLinearCounter();
				updateEnvelopes();
				break;
			case 14914*2:
				break;
			case 0:
				updateLinearCounter();
				updateEnvelopes();
				updateLengthCounters();
				updateSweeps();
				break;
			default:
				break;
		}
		apu.cycles %= (18641*2);
	}

	// need to do actual resampling at some point instead of this lmao
	if(apu.cycles % (CPU_FREQ/SAMPLE_RATE) == 0) {
		if(currentSample > BUFFER_SIZE) {
			currentSample = 0;
			if(SDL_GetAudioStreamQueued(stream) < (int)(SAMPLE_RATE * sizeof(float))/8) {
				SDL_PutAudioStreamData(stream, samples, sizeof(samples));
			}
		}
		samples[currentSample] = 0.0f;
		samples[currentSample] += pulseGetSample(0);
		samples[currentSample] += pulseGetSample(1);
		samples[currentSample] += noiseGetSample();
		samples[currentSample] += triGetSample();
		++currentSample;
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
	apu.noise.timerPeriod = timer;
	apu.noise.timer = noiseTimerLUT[timer];
}

void noiseSetLengthcounter(uint8_t counter) {
	apu.noise.counter = lengthCounterLUT[counter];
	apu.noise.env.startFlag = 1;
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
}

uint8_t apuGetStatus(void) {
	uint8_t status = 0;
	status |= (apu.pulse[0].counter > 0) << 0;
	status |= (apu.pulse[1].counter > 0) << 1;
	// same as above for the tri channel
	// same as above for the noise channel
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
		apu.pulse[0].env.volume, apu.pulse[0].env.decayCounter, apu.pulse[0].timerPeriod, apu.pulse[0].loop,
		apu.pulse[1].env.volume, apu.pulse[1].env.decayCounter, apu.pulse[1].timerPeriod, apu.pulse[1].loop,
		apu.noise.counter, apu.noise.timerPeriod, apu.noise.env.decayCounter,
		apu.tri.linearCounter, apu.tri.lengthCounter, apu.tri.timerPeriod);
}
