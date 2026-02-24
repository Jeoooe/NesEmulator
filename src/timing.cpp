#include <timing.h>

static unsigned int ticks_ = 0;

unsigned int Timing::get_current_tick() {
    return ticks_;
}

/// @brief 步进一周期
void Timing::step_tick(unsigned int ticks) {
    ticks_ += ticks;
}

/// @brief 步进一CPU周期
void Timing::step_cpu_tick(unsigned int ticks) {
    ticks_ += 3 * ticks;
}

void Timing::reset_tick() {
    ticks_ = 0;
}

void Timing::set_tick(unsigned int ticks) {
    ticks_ = ticks;
}
