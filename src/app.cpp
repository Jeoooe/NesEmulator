#include <app.h>
#include <chrono>
#include <thread>
#include <memory>
#include <types.h>
#include <cpu.h>
#include <cartridge.h>
#include <bus.h>

static constexpr double NTSC_CPU_CLOCK = 1789772.5;
static constexpr double FRAMES_PER_SECOND = 60.1;
static constexpr int CYCLES_PER_SECOND = static_cast<int>(NTSC_CPU_CLOCK / FRAMES_PER_SECOND);

/* 60.1 FPS 运行 */
/* 该片段由deepseek生成 */
class FrameTimer {
public:
    static constexpr double FRAME_INTERVAL_MS = 1000.0 / 60.1;  
    FrameTimer(): next_frame(std::chrono::steady_clock::now()) {}
    ~FrameTimer() = default;
    void wait_for_next_frame() {
        next_frame += std::chrono::microseconds(
            static_cast<int>(FRAME_INTERVAL_MS * 1000)
        );
        std::this_thread::sleep_until(next_frame);
    }
private:
    std::chrono::steady_clock::time_point next_frame;
};

void Application::start(int argc, char *argv[]) {
    if (argc < 2) {
        throw std::invalid_argument("Need at least 1 argument");
    }
    auto cart = load_nes_file(argv[1]);
    auto bus = std::make_shared<Bus>();
    CPU &cpu = get_cpu();

    bus->load_cart(std::move(cart));
    cpu.set_bus(bus);

    //CPU初始化
    cpu.reset();

    //主逻辑
    FrameTimer timer;
    while (true) {
        for (int i = 0;i < CYCLES_PER_SECOND;i++) {
            cpu.run1cycle();
        }
        timer.wait_for_next_frame();
    }
}