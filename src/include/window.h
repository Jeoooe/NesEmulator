/*
    用于渲染的代码
    26.3.19 现在要再加上音频了
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
    void audio_update();
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
    SDL_AudioStream *audio_stream = nullptr;    //音频流
    bool running = true;

    static constexpr int samples_per_frame = 2048;
    float audio_buffer[samples_per_frame];
};

class DebugEmulatorWindow {
public:
    DebugEmulatorWindow();
    ~DebugEmulatorWindow();
    void render();
    void render_tile(unsigned int index);
    bool poll_event(SDL_Event *event);
    bool wait_event(SDL_Event *event);
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
