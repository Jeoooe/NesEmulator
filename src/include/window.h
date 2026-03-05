/*
    用于渲染的代码
*/
#pragma once
#include <SDL3/SDL.h>
#include <array>
#include <const.h>


class EmulatorWindow {
public:
    EmulatorWindow();
    ~EmulatorWindow();
    void render();
    bool poll_event(SDL_Event *event);
    inline bool is_running() {
        return running;
    }
private:
    // std::array<Uint32, width * height> buffer;
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Surface *surface = nullptr;
    SDL_Texture *gamepad_texture = nullptr;
    bool running = true;
};

class DebugEmulatorWindow {
public:
    DebugEmulatorWindow();
    ~DebugEmulatorWindow();
    void render();
    void render_tile(unsigned int index);
    bool poll_event(SDL_Event *event);
    inline bool is_running() {
        return running;
    }
    static constexpr int test_width = width * 2;
    std::array<Uint32, test_width * height> buffer;
private:
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Surface *surface = nullptr;
    SDL_Texture *gamepad_texture = nullptr;
    bool running = true;
};
