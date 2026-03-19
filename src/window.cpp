#include <window.h>
#include <vector>
#include <bus.h>
#include <ppu.h>
#include <const.h>
#include <log.h>

static const uint32_t nes_palette[64] = {
    0x7f7f7fff, 0x2000b0ff, 0x2800b8ff, 0x6010a0ff,
    0x982078ff, 0xb01030ff, 0xa03000ff, 0x784000ff,
    0x485800ff, 0x386800ff, 0x386c00ff, 0x306040ff,
    0x305080ff, 0x000000ff, 0x000000ff, 0x000000ff,
    0xbcbcbcff, 0x4060f8ff, 0x4040ffff, 0x9040f0ff,
    0xd840c0ff, 0xd84060ff, 0xe05000ff, 0xc07000ff,
    0x888800ff, 0x50a000ff, 0x48a810ff, 0x48a068ff,
    0x4090c0ff, 0x000000ff, 0x000000ff, 0x000000ff,
    0xffffffff, 0x60a0ffff, 0x5080ffff, 0xa070ffff,
    0xf060ffff, 0xff60b0ff, 0xff7830ff, 0xffa000ff,
    0xe8d020ff, 0x98e800ff, 0x70f040ff, 0x70e090ff,
    0x60d0e0ff, 0x606060ff, 0x000000ff, 0x000000ff,
    0xffffffff, 0x90d0ffff, 0xa0b8ffff, 0xc0b0ffff,
    0xe0b0ffff, 0xffb8e8ff, 0xffc8b8ff, 0xffd8a0ff,
    0xfff090ff, 0xc8f080ff, 0xa0f0a0ff, 0xa0ffc8ff,
    0xa0fff0ff, 0xa0a0a0ff, 0x000000ff, 0x000000ff
};

// static constexpr double FRAME_INTERVAL_MS = 500.0;  

EmulatorWindow::EmulatorWindow() {
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        throw std::runtime_error("SDL failed");
    }
    window = SDL_CreateWindow("Nes", width, height, 0);
    if (!window) {
        SDL_Log("Could not create a window: %s", SDL_GetError());
        throw std::runtime_error("SDL failed");
    }
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("Create renderer failed: %s", SDL_GetError());
        throw std::runtime_error("SDL failed");
    }
    // surface = SDL_CreateSurface(width, height, SDL_PIXELFORMAT_RGBA8888);
    gamepad_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, width, height);
}

EmulatorWindow::~EmulatorWindow() {
    SDL_DestroyTexture(gamepad_texture);
    // SDL_DestroySurface(surface);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void EmulatorWindow::render() {
    //更新缓冲区
    void *texture_buffer;
    int pitch;
    if (SDL_LockTexture(gamepad_texture, nullptr, &texture_buffer, &pitch)) {
        // auto ppu = Bus::get().get_PPU();
        // Uint32 *buffer = (Uint32 *)texture_buffer;
        // for (int y = 0;y < height;y++) {
        //     for (int x = 0;x < width/8;x++) {
        //         auto pixels = ppu->get_pixels(x, y);
        //         for (int i = 0;i < 8;i++) {
        //             buffer[y*width + x*8 + i] = palette[pixels[i]];
        //         }
        //     }
        // }
        // SDL_UnlockTexture(gamepad_texture);
        auto &ppu_buffer = Bus::get().get_PPU()->get_render_buffer();
        uint8_t **mirror_palette = Bus::get().get_PPU()->get_mirror_palette();
        Uint32 *buffer = (Uint32 *)texture_buffer;
        for (int i = 0;i < width * height;i++) {
            // int index = ppu_buffer[i] & 0x1F;
            // if ((index & 3) == 0) buffer[i] = nes_palette[palette[0]];
            // else buffer[i] = nes_palette[palette[index]];
            buffer[i] = nes_palette[*mirror_palette[ppu_buffer[i]]];
            // memcpy(buffer, ppu_buffer.data(), width * height * sizeof(Uint32));
        }
        //包边
        // const auto detail = SDL_PixelFormatDetails(SDL_PIXELFORMAT_RGBA8888);
        // SDL_MapRGBA(&detail, nullptr, 0xFF, 0, 0, 0xFF);
        // for (int i = 0;i < 30;i++) {
        //     memset(buffer + i*256*8, 0xFF, 256 * sizeof(Uint32));
        // }
        SDL_UnlockTexture(gamepad_texture);
    }

    // memcpy(surface->pixels, buffer.data(), buffer.size() * sizeof(Uint32));
    // 转换为纹理并渲染
    // SDL_Texture* texture = SDL_CreateTextureFromSurface(renderer, surface);

    SDL_RenderClear(renderer);
    SDL_RenderTexture(renderer, gamepad_texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    // SDL_Delay(100);
}

bool EmulatorWindow::poll_event(SDL_Event *event) {
    bool state = SDL_PollEvent(event);
    if (event->type == SDL_EVENT_QUIT) running = false;
    return state;
}


/*
    以下是一个调试用窗口
*/

DebugEmulatorWindow::DebugEmulatorWindow() {
    #ifndef NO_UI
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        throw std::runtime_error("SDL failed");
    }
    window = SDL_CreateWindow("Nes", test_width, height, 0);
    if (!window) {
        SDL_Log("Could not create a window: %s", SDL_GetError());
        throw std::runtime_error("SDL failed");
    }
    renderer = SDL_CreateRenderer(window, nullptr);
    if (!renderer) {
        SDL_Log("Create renderer failed: %s", SDL_GetError());
        throw std::runtime_error("SDL failed");
    }
    gamepad_texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_RGBA8888, SDL_TEXTUREACCESS_STREAMING, test_width, height);    
    #endif
}

DebugEmulatorWindow::~DebugEmulatorWindow() {
    #ifndef NO_UI
    SDL_DestroyTexture(gamepad_texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    #endif
}

void DebugEmulatorWindow::render() {
    #ifndef NO_UI
    // void *texture_buffer;
    // int pitch;
    // if (SDL_LockTexture(gamepad_texture, nullptr, &texture_buffer, &pitch)) {
    //     Uint32 *window_buffer = (Uint32 *)texture_buffer;
    //     memcpy(window_buffer, buffer.data(), test_width * height * sizeof(Uint32));
    //     SDL_UnlockTexture(gamepad_texture);
    // }

    // SDL_RenderTexture(renderer, gamepad_texture, nullptr, nullptr);
    // SDL_RenderPresent(renderer);

    // SDL_Delay(FRAME_INTERVAL_MS);
    #endif
}

void DebugEmulatorWindow::render_tile(unsigned int index) {
    void *texture_buffer;
    int pitch;
    unsigned offset = (index & 0x400) >> 2;
    if (SDL_LockTexture(gamepad_texture, nullptr, &texture_buffer, &pitch)) {
        Uint32 *window_buffer = (Uint32 *)texture_buffer;
        // memcpy(window_buffer, buffer.data(), test_width * height * sizeof(Uint32));
        unsigned X = index & 0x1F;
        unsigned Y = (index >> 5) & 0x1F;
        for (int i = 0;i < 8;i++) {
            for (int j = 0;j < 8;j++) {
                window_buffer[(Y*8 + i)*test_width + X*8 + j + offset] = 
                buffer[(Y*8 + i)*test_width + X*8 + j + offset];
            }
        }
        SDL_UnlockTexture(gamepad_texture);
    }

    SDL_RenderTexture(renderer, gamepad_texture, nullptr, nullptr);
    SDL_RenderPresent(renderer);
    SDL_Delay(5);
}

bool DebugEmulatorWindow::poll_event(SDL_Event *event) {
    #ifndef NO_UI
    bool state = SDL_PollEvent(event);
    if (event->type == SDL_EVENT_QUIT) running = false;
    return state;
    #endif
}

bool DebugEmulatorWindow::wait_event(SDL_Event *event) {
    bool state = SDL_WaitEvent(event);
    if (event->type == SDL_EVENT_QUIT) running = false;
    return state;
}
