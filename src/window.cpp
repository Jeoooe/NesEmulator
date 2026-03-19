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

/* 音频相关常量 */
static constexpr int NES_CHANNELS = 1;
static constexpr int NES_AUDIO_FRQ = 44100;

EmulatorWindow::EmulatorWindow() {
    SDL_SetLogPriorities(SDL_LOG_PRIORITY_VERBOSE);
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        SDL_Log("SDL_Init failed: %s", SDL_GetError());
        throw std::runtime_error("SDL failed");
    }
    if (!SDL_Init(SDL_INIT_AUDIO)) {
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

    //音频部分
    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.channels = NES_CHANNELS;
    spec.format = SDL_AUDIO_F32;
    spec.freq = NES_AUDIO_FRQ;
    audio_stream = SDL_OpenAudioDeviceStream(
        SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK,
        &spec,
        nullptr, nullptr
    );
    if (!audio_stream) {
        SDL_Log("Create audio stream failed: %s", SDL_GetError());
        throw std::runtime_error("SDL failed");
    }
    SDL_ResumeAudioStreamDevice(audio_stream);
}

EmulatorWindow::~EmulatorWindow() {
    SDL_DestroyTexture(gamepad_texture);
    // SDL_DestroySurface(surface);
    SDL_DestroyAudioStream(audio_stream);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
}

void EmulatorWindow::render() {
    //更新缓冲区
    void *texture_buffer;
    int pitch;
    if (SDL_LockTexture(gamepad_texture, nullptr, &texture_buffer, &pitch)) {
        auto &ppu_buffer = Bus::get().get_PPU()->get_render_buffer();
        uint8_t **mirror_palette = Bus::get().get_PPU()->get_mirror_palette();
        Uint32 *buffer = (Uint32 *)texture_buffer;
        if (Bus::get().get_PPU()->render_enabled()) {   
            for (int i = 0;i < width * height;i++) {
                buffer[i] = nes_palette[*mirror_palette[ppu_buffer[i]]];
            }
        } else {
            Uint32 pixel = nes_palette[*mirror_palette[0]];
            std::fill(buffer, buffer + width * height, pixel);
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

void EmulatorWindow::audio_update() {
    static float phase = 0.0f;               // 当前相位 (0.0 到 1.0)
    static float phase_increment = 440.0f / 44100.0f; // 每样本相位增量 (440Hz A音)
    static float volume = 0.25f;              // 音量，小一点防止破音
    
    int queued_bytes = SDL_GetAudioStreamQueued(audio_stream);
    constexpr float bytes_per_second = NES_AUDIO_FRQ * NES_CHANNELS * sizeof(float);
    float queued_seconds = queued_bytes / bytes_per_second;
    if (queued_seconds < 0.1f) {
        int target_bytes = static_cast<int>(0.2f * bytes_per_second);
        int bytes_to_generate = target_bytes - queued_bytes;
        
        // 转换成样本数，并确保不超过我们每帧能处理的上限
        int samples_to_generate = bytes_to_generate / sizeof(float);
        samples_to_generate = std::min(samples_to_generate, samples_per_frame);

        if (samples_to_generate > 0) {
            // 4. 开始烧水！生成方波样本
            for (int i = 0; i < samples_to_generate; i++) {
                // 方波的核心算法：相位小于0.5时输出正电压，否则输出负电压
                // 这就是产生方波"嘟嘟"声的秘密
                audio_buffer[i] = (phase < 0.5f) ? volume : -volume;
                
                // 更新相位，让它为下一个样本做准备
                phase += phase_increment;
                
                // 确保相位始终在0到1之间循环
                if (phase >= 1.0f) {
                    phase -= 1.0f;
                }
            }

            // 5. 把烧好的水（音频数据）灌入热水器的水箱
            //    SDL_PutAudioStreamData 会把数据复制走，所以我们的audio_buffer可以重复使用
            SDL_PutAudioStreamData(audio_stream, audio_buffer, samples_to_generate * sizeof(float));
        }
    }
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
