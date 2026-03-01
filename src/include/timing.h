#pragma once
#include <stdint.h>

class Timing {
public:
    /// @brief 获取当前时钟周期, 以PPU周期为基准. 1 CPU = 3 PPU
    /// @return 当前时钟周期
    static uint64_t get_current_tick();

    /// @brief 步进周期数
    static void step_tick(uint64_t ticks);  

    /// @brief 步进一CPU周期
    static void step_cpu_tick(uint64_t cpu_ticks);

    /// @brief 重置时钟周期
    static void reset_tick();

    /// @brief 不应该使用, 但是需要暴露给STA等指令...
    static void set_tick(uint64_t ticks);
};

