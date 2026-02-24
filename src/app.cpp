#include <app.h>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <types.h>
#include <cartridge.h>
#include <bus.h>
#include <cpu.h>
#include <ppu.h>
#include <timing.h>
#include <window.h>
#include <controller.h>

// #define NO_UI

static constexpr double NTSC_CPU_CLOCK = 1789772.5;
static constexpr double FRAMES_PER_SECOND = 60.1;
static constexpr int CYCLES_PER_FRAME = static_cast<int>(NTSC_CPU_CLOCK / FRAMES_PER_SECOND);
static constexpr int TICKS_PER_FRAME = CYCLES_PER_FRAME * 3;


class FrameTimer {
public:
    // 60.1 fps = 1000/60.1 ≈ 16.6389ms 每帧
    static constexpr double FRAME_INTERVAL_MS = 1000.0 / 60.1;
    
    FrameTimer() : next_frame(std::chrono::steady_clock::now()) {}
    
    void wait_for_next_frame() {
        next_frame += std::chrono::microseconds(
            static_cast<int>(FRAME_INTERVAL_MS * 1000)
        );
        std::this_thread::sleep_until(next_frame);
    }
    
private:
    std::chrono::steady_clock::time_point next_frame;
};


void ppu_test();

void key_event(SDL_Event *event) {
    auto &ctrl = Bus::get().get_controller();
    int type = event->type == SDL_EVENT_KEY_DOWN ? 1 : 0;
    auto fn = [type, &ctrl](KeyMask mask){
        if (type == 1) {
            ctrl->key_down(Controller::Controller1, mask);
        } else {
            ctrl->key_up(Controller::Controller1, mask);
        }
    };
    switch (event->key.key) {
    case SDLK_A:
        fn(KeyMask::Left);
        break;
    case SDLK_W:
        fn(KeyMask::Up);
        break;
    case SDLK_S:
        fn(KeyMask::Down);
        break;
    case SDLK_D:
        fn(KeyMask::Right);
        break;
    case SDLK_F:
        fn(KeyMask::Select);
        break;
    case SDLK_H:
        fn(KeyMask::Start);
        break;
    case SDLK_J:
        fn(KeyMask::B);
        break;
    case SDLK_K:
        fn(KeyMask::A);
        break;   
    default:
        break;
    }
}


void Application::start(int argc, char *argv[]) {
    if (argc < 2) {
        throw std::invalid_argument("Need at least 1 argument");
    }
    auto cart = load_nes_file(argv[1]);
    auto &bus = Bus::get();
    auto &cpu = get_cpu();

    bus.load_cart(std::move(cart));

    //CPU初始化
    cpu.reset();

#ifndef NO_UI
    //创建模拟器窗口
    EmulatorWindow window;

    //主逻辑
    while (window.is_running()) {
        SDL_Event event;
        while (window.poll_event(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                key_event(&event);
            }
        }

        auto last_tick = Timing::get_current_tick() + PPU::total_frame_ticks;
        while (Timing::get_current_tick() < last_tick) {
            Bus::get().get_CPU().run1operation();
            Bus::get().get_PPU()->scan();
        }
        
        window.render();
    }
#else 
    FrameTimer timer;
    while (true) {
        auto last_tick = Timing::get_current_tick() + PPU::total_frame_ticks;
        while (Timing::get_current_tick() < last_tick) {
            Bus::get().get_CPU().run1operation();
            Bus::get().get_PPU()->scan();
        }
        ppu_test();
        // timer.wait_for_next_frame();
    }
#endif
}
std::array<uint32_t, 256 * 240> buffer;
void ppu_test() {
    auto ppu = Bus::get().get_PPU();
    for (int y = 0;y < 240;y++) {
        for (int x = 0;x < 256/8;x++) {
            auto pixels = ppu->get_pixels(x, y);
            for (int i = 0;i < 8;i++) {
                buffer[y * 256 + x * 8 + i] = pixels[i];
            }
        }
    }
}