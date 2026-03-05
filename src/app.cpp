#include <app.h>
#include <memory>
#include <unordered_map>
#include <chrono>
#include <thread>
#include <types.h>
#include <const.h>
#include <cartridge.h>
#include <bus.h>
#include <cpu.h>
#include <ppu.h>
#include <timing.h>
#include <window.h>
#include <controller.h>

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

    if (!cart) {
        exit(-1);
    }

    auto &bus = Bus::get();
    auto &cpu = bus.get_CPU();
    auto &ppu = Bus::get().get_PPU();  

    bus.load_cart(std::move(cart));

    //CPU初始化
    cpu.reset();

#ifndef NO_UI
    //创建模拟器窗口
    #ifndef TEST_WINDOW
    EmulatorWindow window;
    #else 
    DebugEmulatorWindow window;
    ppu->set_window(&window);
    #endif

    //主逻辑
    
    while (window.is_running()) {
        SDL_Event event;
        while (window.poll_event(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                key_event(&event);
            }
        }
        
        while (!ppu->is_frame_end()) {
            cpu.run1operation();
            ppu->scan();
        }
        #ifdef TEST_WINDOW
        ppu->render_full_screen(&window);
        #endif
        window.render();
    }
#else 
    FrameTimer timer;
    #ifndef TEST_WINDOW
    EmulatorWindow window;
    #else 
    DebugEmulatorWindow window;
    ppu->set_window(&window);
    #endif
    while (true) {
        while (!ppu->is_frame_end()) {
            cpu.run1operation();
            ppu->scan();
        }
        timer.wait_for_next_frame();
        #ifdef TEST_WINDOW
        ppu->render_full_screen(&window);
        #endif
    }
#endif
}
