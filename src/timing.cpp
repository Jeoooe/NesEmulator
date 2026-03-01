#include <timing.h>

static uint64_t ticks_ = 0;

uint64_t Timing::get_current_tick() {
    return ticks_;
}

/// @brief 步进一周期
void Timing::step_tick(uint64_t ticks) {
    ticks_ += ticks;
}

/// @brief 步进一CPU周期
void Timing::step_cpu_tick(uint64_t ticks) {
    ticks_ += 3 * ticks;
}

void Timing::reset_tick() {
    ticks_ = 0;
}

void Timing::set_tick(uint64_t ticks) {
    ticks_ = ticks;
}
