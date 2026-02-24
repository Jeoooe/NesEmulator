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
    void scan();    

    //需要一个供渲染器获取屏幕像素的方法
    auto get_pixels(unsigned int x, unsigned int y) -> std::array<uint8_t, 8>;

private:
    bool is_rendering();
    uint8_t ppu_ram_read(uint16_t addr, bool from_cpu);
    void ppu_ram_write(uint16_t addr, uint8_t value);

    //下面是变量
public:
    static constexpr unsigned int rendering_ticks = 81840;
    static constexpr unsigned int vblank_ticks = 7502;
    static constexpr unsigned int total_frame_ticks = rendering_ticks + vblank_ticks;
private:
    /*
        0x0000 - 0x1FFF: pattern    这里还是ROM范围
        0x2000 - 0x2FFF: Name Table 从这里开始才是显存
        0x3000 - 0x3EFF: Unused
        0x3F00 - 0x3F1F: Palette RAM index 调色板
        0x3F20 - 0x3FFF: Mirrors of 0x3F00 - 0x3F1F
    */
    std::array<uint8_t, 0x1000> ppu_bus;        //专用总线 2kb 调色板单独拿出来
    std::array<uint8_t, 0x1F> palette_indexes;  //调色板
    uint8_t reg2000 = 0;
    uint8_t reg2001 = 0;
    uint8_t reg2002 = 0;
    uint8_t reg2003 = 0;
    uint8_t reg2004 = 0;
    uint8_t reg2005 = 0;
    // std::array<uint8_t, 0x8> reg{0, 0, 0, 0, 0, 0, 0, 0};           //八个寄存器 没有多线程安全
    uint8_t reg4014 = 0;

    //部分比较特殊的寄存器
    uint16_t reg2006 = 0;
    uint8_t vram_buffer = 0;        //VRAM的缓冲区

    unsigned int next_vblank_tick = rendering_ticks;
    unsigned int next_frame_tick = total_frame_ticks;
    bool is_vblank = false;                 //是否处于vblank状态
    uint16_t continuous_write = 0;          //部分寄存器需要连续写入
};