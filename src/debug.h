#ifndef DEBUG_H
#define DEBUG_H

#include <stdarg.h>

#include "SDL3/SDL.h"

void initDebugRenderer(void);
void drawDebugText(uint16_t x, uint16_t y, char* fmt, ...);
void renderDebugInfo(SDL_Surface* windowSurface);

void toggleDebugInfo(void);

#endif // DEBUG_H
