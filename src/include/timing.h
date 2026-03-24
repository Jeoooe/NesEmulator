#pragma once
#include <stdint.h>

inline constexpr int CPU_FRQ = 1789773;
inline constexpr int TICK_FRQ = CPU_FRQ * 3;
inline constexpr int APU_FRQ = CPU_FRQ / 2;


inline constexpr int PPU_TICK = 1;
inline constexpr int CPU_TICK = 3 * PPU_TICK;
inline constexpr int APU_TICK = 2 * CPU_TICK;


class Timing {
    static uint64_t ticks_;
public:
    /// @brief 获取当前时钟周期, 以PPU周期为基准. 1 CPU = 3 PPU
    /// @return 当前时钟周期
    inline static uint64_t get_current_tick() {
        return ticks_;
    }

    /// @brief 步进周期数
    inline static void step_tick(uint64_t ticks) {
        ticks_ += PPU_TICK * ticks;
    }

    /// @brief 步进一CPU周期
    inline static void step_cpu_tick(uint64_t ticks) {
        ticks_ += CPU_TICK * ticks;
    }

    /// @brief 重置时钟周期
    inline static void reset_tick() {
        ticks_ = 0;
    }

    /// @brief 不应该使用, 但是需要暴露给STA等指令...
    inline static void set_tick(uint64_t ticks) {
        ticks_ = ticks;
    }
};

