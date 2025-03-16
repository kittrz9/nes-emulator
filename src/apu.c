// https://github.com/libsdl-org/SDL/blob/main/examples/audio/01-simple-playback/simple-playback.c

#include "apu.h"

#include "SDL3/SDL.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

SDL_AudioStream* stream = NULL;
int currentSineSample = 0;

#define CPU_FREQ 1789773

struct {
	uint8_t volume;
	uint16_t timer;
} pulse[2];

void initAPU(void) {
	SDL_AudioSpec spec;

	if(SDL_Init(SDL_INIT_AUDIO) == 0) {
		printf("could not initialize SDL's audio\n");
		exit(1);
	}

	spec.channels = 1;
	spec.format = SDL_AUDIO_F32;
	spec.freq = 8000;

	stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, NULL, NULL);
	if(stream == NULL) {
		printf("could not create audio stream\n");
		exit(1);
	}
	SDL_ResumeAudioStreamDevice(stream);
}

void apuLoop(void) {
	const int minimumAudio = (8000 * sizeof(float))/2;

	if(SDL_GetAudioStreamQueued(stream) < minimumAudio) {
		static float samples[512];
		int i;

		for(i = 0; i < SDL_arraysize(samples); ++i) {
			const int freq = CPU_FREQ / (16 * (pulse[0].timer + 1));
			const float phase = currentSineSample * freq / 8000.0f;
			samples[i] = ((currentSineSample * freq) % 8000 < 4000 ? 0 : pulse[0].volume/32.0);
			++currentSineSample;
		}

		currentSineSample %= 8000;

		SDL_PutAudioStreamData(stream, samples, sizeof(samples));
	}
}

void pulseSetVolume(uint8_t index, uint8_t volume) {
	pulse[index].volume = volume;
}

void pulseSetTimerLow(uint8_t index, uint8_t timerLow) {
	pulse[index].timer &= 0xFF00;
	pulse[index].timer |= timerLow;
}

void pulseSetTimerHigh(uint8_t index, uint8_t timerHigh) {
	pulse[index].timer &= 0x00FF;
	pulse[index].timer |= timerHigh << 8;
}
