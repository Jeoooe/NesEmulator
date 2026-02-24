#include <ppu.h>
#include <bus.h>
#include <timing.h>
#include <cpu.h>

#define D0 1
#define D1 2
#define D2 4
#define D3 8
#define D4 16
#define D5 32
#define D6 64
#define D7 128

static constexpr uint16_t vram_start = 0x2000; 

uint8_t PPU::cpu_read(uint16_t addr) {
    //非vblank状态不进行处理
    if (is_rendering()) return 0;

    if (addr >= 0x2000 && addr < 0x4000) {
        addr &= 0x2007;
    }

    //是否是连续写入
    if (continuous_write != addr) continuous_write = 0;

    switch (addr) {
    case 0x2000:
        return reg2000;
    case 0x2001:
        return reg2001;
    case 0x2002: 
    {
        uint8_t snapshot = reg2002;
        reg2002 &= ~D7;
        return snapshot;
    }

    case 0x2003:
        return reg2003;
    case 0x2004:
        return reg2004;
    case 0x2005:
        return reg2005;
    case 0x2006:
        return 0;   //这个应该不能读
    case 0x2007:
    {    
        uint8_t value = ppu_ram_read(reg2006, true);
        reg2006 += (reg2000 & D2) ? 32 : 1;
        return value;
    }
    case 0x4014:
        return reg4014;

    default:
        return 0;
    }
}

void PPU::cpu_write(uint16_t addr, uint8_t value) {
    //非vblank状态不进行处理
    if (is_rendering()) return;

    if (addr >= 0x2000 && addr < 0x4000) {
        addr &= 0x2007;
    }

    if (continuous_write != addr) continuous_write = 0;

    switch (addr) {
    case 0x2000:
        reg2000 = value;
        break;

    case 0x2001:
        reg2001 = value;
        break;

    case 0x2002:
        break;

    case 0x2003:
        reg2003 = value;
        break;
    case 0x2004:
        reg2004 = value;
        break;
    case 0x2005:
        reg2005 = value;
        break;
    case 0x2006:
        if (continuous_write == 0) {
            reg2006 = (uint16_t)value << 8;
            continuous_write = 0x2006;
        } else {
            reg2006 |= (uint16_t)value;
            continuous_write = 0;
        }
        break;
    case 0x2007:
        ppu_ram_write(reg2006, value);
        reg2006 += (reg2000 & D2) ? 32 : 1;
        break;
    case 0x4014:
        reg4014 = value;
        break;
    }
}

void PPU::scan() {
    auto current_tick = Timing::get_current_tick();
    if (current_tick >= next_vblank_tick) {
        //开启vblank
        next_vblank_tick = next_frame_tick + rendering_ticks;
        is_vblank = true;
        reg2002 |= D7;
        if (reg2000 & D7) {
            Bus::get().get_CPU().trigger_NMI();
        }
    }
    if (current_tick >= next_frame_tick) {
        //更新数据
        reg2002 &= ~D7;
        next_frame_tick += PPU::total_frame_ticks;
        is_vblank = false;
    }
}

bool PPU::is_rendering() {
    return !is_vblank && (reg2001 & (D3 | D4));
}

/// @brief ppu显存读取  `0000-2000`为ROM, `2000-3000`为RAM, `3F00-4000`为调色板
///
/// @param addr 地址
/// @param from_cpu 是否由cpu读取. cpu读取返回值为缓冲值, 即上次读取的值
/// @return
uint8_t PPU::ppu_ram_read(uint16_t addr, bool from_cpu)
{
    if (addr < vram_start) {
        return Bus::get().ppu_read(addr);
    } 
    if (addr < 0x3000) {
        if (from_cpu) {
            const uint8_t snapshot = vram_buffer;
            vram_buffer = ppu_bus[addr -vram_start];
            return snapshot;
        } 
        else {
            return ppu_bus[addr -vram_start];
        }
    } else if (addr < 0x3F00) {
        //这里是不使用的区域
        return 0;
    } else {    //这里是调色板
        addr &= 0x1F;
        vram_buffer = palette_indexes[addr];
        return vram_buffer;
    }
}

/// @brief ppu显存写入  `0000-2000`为ROM, `2000-3000`为RAM, `3F00-4000`为调色板
///
/// @param addr 地址
/// @param value 写入值
/// @return 
void PPU::ppu_ram_write(uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        return; //ROM应该不可写
    }
    if (addr < 0x3000) {
        ppu_bus[addr - vram_start] = value;
    } else if (addr < 0x3F00) {
        return;
    } else {
        addr &= 0x1F;
        palette_indexes[addr] = value;
    }
}


/// @brief 获取屏幕上的像素点, 注意一次返回8个像素
/// @param x 行像素索引, 对应像素点坐标为`[x * 8, x * 8 + 7]`. 范围为 `0-32`
/// @param y 列像素索引, 范围为`0-240`
/// @return 8个像素的颜色
auto PPU::get_pixels(unsigned int x, unsigned int y) -> std::array<uint8_t, 8> {
    //先渲染背景
    //先确定是哪个名称表
    const uint16_t name_table = 0x2000; //默认第一个

    //这里开始获取颜色
    //先获取调色板entry
    const uint16_t attribute_table = name_table + 0x3C0;
    const uint16_t attribute_index = (y >> 5)*8 + (x>>2);
    const uint16_t attribute = ppu_bus[attribute_table + attribute_index - vram_start];
    const uint8_t attribute_offset = (x & 0x2) | ((y & 0x10) >> 2);
    const uint8_t palette_entry = ((attribute >> attribute_offset) & 0x3) << 2;
    //直接获取调色板四个颜色
    std::array<uint8_t, 4> palette{
        ppu_ram_read( palette_entry      | 0x3F00, false),
        ppu_ram_read((palette_entry + 1) | 0x3F00, false),
        ppu_ram_read((palette_entry + 2) | 0x3F00, false),
        ppu_ram_read((palette_entry + 3) | 0x3F00, false),
    };

    //然后看图案表
    //确定图案表
    const uint16_t pattern_table = (reg2000 & D4) ? 0x1000 : 0;
    //获取图案表tile地址
    const uint16_t pattern_addr = \
        pattern_table + (y & 0x7) + 
        16*(uint16_t)ppu_bus[name_table + (y >> 3)*32 + x - vram_start];
    const uint8_t pixels_low = Bus::get().ppu_read(pattern_addr);
    const uint8_t pixels_high = Bus::get().ppu_read(pattern_addr + 8);

    //八个颜色
    const uint8_t pixel0246 = (pixels_low&0b01010101) | ((pixels_high&0b01010101)<<1);
    const uint8_t pixel1357 = ((pixels_low&0b10101010)>>1) | (pixels_high&0b10101010);


    return std::array<uint8_t, 8>{
        palette[((pixel1357>>6u) & 0b11u)],
        palette[((pixel0246>>6u) & 0b11u)],
        palette[((pixel1357>>4u) & 0b11u)],
        palette[((pixel0246>>4u) & 0b11u)],
        palette[((pixel1357>>2u) & 0b11u)],
        palette[((pixel0246>>2u) & 0b11u)],
        palette[((pixel1357    ) & 0b11u)],
        palette[((pixel0246    ) & 0b11u)],
    };
}

