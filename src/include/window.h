/*
    用于渲染的代码
*/
#pragma once
#include <SDL3/SDL.h>
#include <array>


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
    static constexpr int width  = 256;
    static constexpr int height = 240;
    std::array<Uint32, width * height> buffer;
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    SDL_Surface *surface = nullptr;
    SDL_Texture *gamepad_texture = nullptr;
    bool running = true;
};

