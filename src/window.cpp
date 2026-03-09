#include <window.h>
#include <vector>
#include <bus.h>
#include <ppu.h>
#include <const.h>

static constexpr double FRAME_INTERVAL_MS = 1000.0 / 60.1;  
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
        auto &ppu_buffer = Bus::get().get_PPU()->get_window_buffer();
        Uint32 *buffer = (Uint32 *)texture_buffer;
        memcpy(buffer, ppu_buffer.data(), width * height * sizeof(Uint32));
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

    //这里还要帧率问题
    SDL_Delay(FRAME_INTERVAL_MS);
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
