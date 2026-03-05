#pragma once

#include <SDL3/SDL_log.h>
#include <cstdio>

#ifdef DEBUG
// #define LOG SDL_Log
#define LOG printf
#else
#define LOG (void)
#endif