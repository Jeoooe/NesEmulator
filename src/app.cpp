#include <app.h>

#include <memory>
#include <unordered_map>
#include <chrono>
#include <thread>

#include <bus.h>
#include <cartridge.h>
#include <cpu.h>
#include <const.h>
#include <controller.h>
#include <ppu.h>
#include <types.h>
#include <timing.h>
#include <window.h>


#ifdef DISASSEMBLE
#include "test/check_log.hpp"
#endif

class FrameTimer {
public:
    // 60.1 fps = 1000/60.1 ≈ 16.6389ms 每帧
    static constexpr double FRAME_INTERVAL_MS = 1000.0 / 60.0988;
    
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

    auto &bus = Bus::get();
    auto &cpu = bus.get_CPU();
    auto &ppu = Bus::get().get_PPU();  

    if (bus.load_cart(argv[1]) == -1) {
        throw std::runtime_error("Fail to load this file");
    }

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
    #ifdef DISASSEMBLE

    Disassembler disa;

    #endif
    FrameTimer timer;
    while (window.is_running()) {
        SDL_Event event;
        #ifdef DISASSEMBLE

        if (disa.enabled) {
            (void)window.wait_event(&event);
            bool step = false;
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                switch (event.key.key) {
                case SDLK_E:
                    disa.enabled = false;
                    break;
                case SDLK_L:
                    disa.list_lines();
                    break;
                case SDLK_S:
                    step = true;
                    break;
                }
            }
            if (step) {
                cpu.run1operation();
                ppu->scan();
            }
        } else {

        #endif

        while (window.poll_event(&event)) {
            if (event.type == SDL_EVENT_KEY_DOWN || event.type == SDL_EVENT_KEY_UP) {
                key_event(&event);

                #ifdef DISASSEMBLE
                if (event.key.key == SDLK_E) {
                    //开启关闭
                    disa.enabled = true;
                }
                #endif
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
        // window.audio_update();
        //这里控制帧率为60.1
        timer.wait_for_next_frame();

        #ifdef DISASSEMBLE
        }
        #endif
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
