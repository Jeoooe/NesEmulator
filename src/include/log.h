#pragma once

#include <SDL3/SDL_log.h>

#ifdef DEBUG
#define LOG SDL_Log
#else
#define LOG (void)
#endif