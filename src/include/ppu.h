#pragma once

#include <stdint.h>
#include <memory>
#include <array>

class PPU {
public:
    PPU() = default;
    ~PPU() = default;

    //逻辑部分
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t value);

    //需要一个供渲染器获取屏幕像素的方法
    std::array<uint32_t, 8> get_pixels(unsigned int x, unsigned int y);
private:
    /*
        0x0000 - 0x1FFF: pattern
        0x2000 - 0x2FFF: Name Table
        0x3000 - 0x3EFF: Unused
        0x3F00 - 0x3F1F: Palette RAM index 调色板
        0x3F20 - 0x3FFF: Mirrors of 0x3F00 - 0x3F1F
    */
    std::array<uint8_t, 0x4000> ppu_bus;    //专用总线 8kb
    std::array<uint8_t, 0x8> reg;           //八个寄存器 没有多线程安全
};