#include <ppu.h>
#include <cstring>
#include <bus.h>
#include <timing.h>
#include <cpu.h>
#include <log.h>

#define D0 1
#define D1 2
#define D2 4
#define D3 8
#define D4 16
#define D5 32
#define D6 64
#define D7 128

inline constexpr uint16_t vram_start = 0x2000; 
inline constexpr cycle_t ONE_LINE_TICK = 341;

static const uint32_t nes_palette[64] = {
    0x7f7f7fff, 0x2000b0ff, 0x2800b8ff, 0x6010a0ff,
    0x982078ff, 0xb01030ff, 0xa03000ff, 0x784000ff,
    0x485800ff, 0x386800ff, 0x386c00ff, 0x306040ff,
    0x305080ff, 0x000000ff, 0x000000ff, 0x000000ff,
    0xbcbcbcff, 0x4060f8ff, 0x4040ffff, 0x9040f0ff,
    0xd840c0ff, 0xd84060ff, 0xe05000ff, 0xc07000ff,
    0x888800ff, 0x50a000ff, 0x48a810ff, 0x48a068ff,
    0x4090c0ff, 0x000000ff, 0x000000ff, 0x000000ff,
    0xffffffff, 0x60a0ffff, 0x5080ffff, 0xa070ffff,
    0xf060ffff, 0xff60b0ff, 0xff7830ff, 0xffa000ff,
    0xe8d020ff, 0x98e800ff, 0x70f040ff, 0x70e090ff,
    0x60d0e0ff, 0x606060ff, 0x000000ff, 0x000000ff,
    0xffffffff, 0x90d0ffff, 0xa0b8ffff, 0xc0b0ffff,
    0xe0b0ffff, 0xffb8e8ff, 0xffc8b8ff, 0xffd8a0ff,
    0xfff090ff, 0xc8f080ff, 0xa0f0a0ff, 0xa0ffc8ff,
    0xa0fff0ff, 0xa0a0a0ff, 0x000000ff, 0x000000ff
};

bool PPU::is_frame_end() {
    if (current_scanline == -1) {
        current_scanline = 0;
        return true;
    }
    return false;
}

bool PPU::is_rendering() {
    return !is_vblank && (reg2001 & (D3 | D4));
}

uint8_t PPU::cpu_read(uint16_t addr) {
    //非vblank状态不进行处理
    // if (is_rendering()) return 0;

    if (addr >= 0x2000 && addr < 0x4000) {
        addr &= 0x2007;
    }


    switch (addr) {
    case 0x2000:
        return reg2000;
    case 0x2001:
        return reg2001;
    case 0x2002: 
    {
        uint8_t snapshot = reg2002;
        reg2002 &= ~D7;
        regw = 0;
        return snapshot;
    }

    case 0x2003:
        return reg2003;
    case 0x2004:
        return OAM[reg2003];
    case 0x2005:
        return 0;
    case 0x2006:
        return 0;   //这个应该不能读
    case 0x2007:
    {    
        uint8_t value = ppu_ram_read(reg2006);
        reg2006 += (reg2000 & D2) ? 32 : 1;
        return value;
    }
    case 0x4014:
        // return reg4014;
        return 0;

    default:
        return 0;
    }
    return 0;   //一般不会到这里
}

void PPU::cpu_write(uint16_t addr, uint8_t value) {
    //非vblank状态不进行处理
    // if (is_rendering()) return;

    if (addr >= 0x2000 && addr < 0x4000) {
        addr &= 0x2007;
    }

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
        OAM[reg2003++] = value;
        break;
    case 0x2005:
        if (regw) {
            scroll_Y = value;
            regw = 0x2005;
        } else {
            scroll_X = value;
            regw = 0;
        }
        break;
    case 0x2006:
        if (regw == 0) {
            reg2006 = (uint16_t)value << 8;
            regw = 0x2006;
        } else {
            reg2006 |= (uint16_t)value;
            regw = 0;
        }
        break;
    case 0x2007:
        ppu_ram_write(reg2006, value);
        reg2006 += (reg2000 & D2) ? 32 : 1;
        break;
    case 0x4014:
        //这里触发DMA直接存储
        {
            //BUG
            //这里存在可能的Bug
            //2. 时钟周期不同
            Timing::step_cpu_tick(512); //直接占用512周期?无所谓了, 先这样
            size_t r_addr = (size_t)value << 8;
            size_t start_offset = (size_t)reg2003;
            memcpy(
                (void *)(OAM.data() + reg2003), 
                (void *)(Bus::get().get_cpu_ram().data() + r_addr),
                0x100 - reg2003
            );
            if (reg2003) {
                //即起始点不是$00, 要复制256字节, 要求回绕
                memcpy(
                    (void *)(OAM.data()), 
                    (void *)(Bus::get().get_cpu_ram().data() + r_addr + 0x100 - reg2003),
                    reg2003
                );
            }
        }
        break;
    }
}



/// @brief ppu显存读取  `0000-2000`为ROM, `2000-3000`为RAM, `3F00-4000`为调色板
///
/// @param addr 地址
/// @param from_cpu 是否由cpu读取. cpu读取返回值为缓冲值, 即上次读取的值
/// @return
uint8_t PPU::ppu_ram_read(uint16_t addr) {
    if (addr < 0x2000) {
        return Bus::get().ppu_read(addr);
    } 
    if (addr < 0x3000) {
        const uint8_t snapshot = vram_buffer;
        vram_buffer = ppu_bus[addr -vram_start];
        return snapshot;
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
        // return; //ROM应该不可写
        // FIX: 考虑到有CHR_RAM这种行为
        Bus::get().ppu_write(addr, value);
        return;
    }
    if (addr < 0x3000) {
        ppu_bus[addr - vram_start] = value;
        return;
    } 
    if (addr < 0x3F00) {
        return;
    } else {
        if (addr & 3) {
            palette_indexes[addr & 0x1F] = value;
        } else {
            addr &= 0x0F;
            palette_indexes[0x0] = palette_indexes[0x4] = palette_indexes[0x8] =
            palette_indexes[0xC] = palette_indexes[addr | 0x10] = value;
        }
    }
}

void PPU::scan() {
    static uint64_t next_line_tick = 0;
    auto current_tick = Timing::get_current_tick();
    if (current_tick >= next_line_tick) {
        scan_one_line();
        next_line_tick += ONE_LINE_TICK;
    }
}



void PPU::scan_one_line() {  
    // 判断是否在可见扫描线区域（0-239为可见扫描线）
    if (current_scanline < 240) {
        //这里仅仅是把window_buffer的值设置为调色板的索引
        render_background_one_line(current_scanline);
        render_sprites_one_line(current_scanline);
    }
    
    // 更新扫描线计数
    current_scanline++;
    if (current_scanline == 240) {
        //开启vblank
        is_vblank = true;
        reg2002 |= D7;
        if (reg2000 & D7) {
            Bus::get().get_CPU().trigger_NMI();
        }
    }
    if (current_scanline >= 262) {
        current_scanline = -1;  //-1则为帧结束
        //更新数据
        reg2002 &= ~D7;
        is_vblank = false;
        //这里加载像素的颜色
        for (int i = 0;i < width * height;i++) {
            window_buffer[i] = nes_palette[palette_indexes[window_buffer[i]]];
        }
    }
}

auto PPU::render_background_one_line(unsigned int line) -> void
{
    //有滚动就比较复杂
    //不考虑镜像
    const unsigned int row = (line + scroll_Y) % 240;
    uint16_t name_table = ((reg2000 & 0x03) * 0x400);  // 当前使用的名字表
    const int start_col = scroll_X & 7;
    const uint8_t tile_offset = scroll_X>>3;
    int col = 0;
    //第一个瓦片存在偏移
    {
        int x = tile_offset;
        auto pixels = get_pixels(x, row, name_table);
        for (int offset = start_col; offset < 8; offset++) {   
            window_buffer[line*width + col] = pixels[offset];
            col++;
        }
    }
    for (int t = 1; t < 32; t++) {
        int x = t + tile_offset;
        auto pixels = get_pixels(x & 0x1F, row, x > 32 ? ((name_table + 0x400) & 0x400) : name_table);
        // auto pixels = get_pixels(x & 0x1F, row, name_table);
        for (int i = 0;i < 8;i++) {   
            window_buffer[line*width + col] = pixels[i];
            col++;
        }
    }
    //最后还可能有个瓦片
    if (start_col) {
        int x = 32 + tile_offset;
        auto pixels = get_pixels(x & 0x1F, row, x > 32 ? ((name_table + 0x400) & 0x400) : name_table);
        // auto pixels = get_pixels(x & 0x1F, row, name_table);
        for (int i = 0;i < start_col;i++) {   
            window_buffer[line*width + col] = pixels[i];
            col++;
        }
    }
}



auto PPU::render_sprites_one_line(unsigned int line) -> void {
    //遍历精灵, 选出前八个
    uint16_t pattern_table = (reg2002 & D3) ? 0 : 0x1000;
    bool is_8x16 = reg2000 & D5;
    unsigned int sprite_size = is_8x16 ? 16 : 8;
    int sprites_count = 0;
    struct SpiritAttribute {
        uint8_t y; 
        uint8_t tile_index;
        uint8_t attr;
        uint8_t x;
    };
    std::array<SpiritAttribute, 8> sprites;
    SpiritAttribute *sp_oam = (SpiritAttribute *)OAM.data();
    int i = 1;
    bool has_sp0 = false;
    {
        uint8_t sprite_y = sp_oam[0].y + 1;
        if (line >= sprite_y && line < sprite_y + sprite_size) {
            sprites[sprites_count] = sp_oam[0];
            sprites[sprites_count++].y++; 
            has_sp0 = true;
        }
    }
    for (;i < 64 && sprites_count < 8; i++) {
        uint8_t sprite_y = sp_oam[i].y + 1;
        if (line >= sprite_y && line < sprite_y + sprite_size) {
            sprites[sprites_count] = sp_oam[i];
            sprites[sprites_count++].y++; 
        }
    }
    if (sprites_count == 8) {
        for (;i < 64;i++) {
            uint8_t sprite_y = sp_oam[i].y + 1;
            if (line >= sprite_y && line < sprite_y + sprite_size) {
                reg2002 |= D5;  //精灵溢出标志
                break;
            }
        }
    }

    //下面渲染8个精灵
    for (i = sprites_count - 1;i >= 0;i--) {
        int row_in_tile = line - sprites[i].y;
        if (sprites[i].attr & D7) {
            //垂直翻转
            if (is_8x16) row_in_tile = 16 - row_in_tile;
            else row_in_tile = 8 - row_in_tile;
        }
        //对于8x8和8x16而言, 区别在选择瓦片
        uint8_t tile_index = sprites[i].tile_index;
        if (is_8x16) {
            pattern_table = (tile_index & 1) * 0x1000;
            tile_index &= ~1;
            //这里不能用row_in_tile,因为会有翻转
            tile_index |= (row_in_tile > 8) ? 1 : 0;
            row_in_tile &= 7;
        }
        const auto tile = get_tile_pixels(tile_index, pattern_table, row_in_tile);
        const auto pixels = decode_tile(tile, ((sprites[i].attr & 0x3)<<2) | 0x10);
        const bool is_hflip = sprites[i].attr & D6;
        for (int j = 0;j < 8;j++) {
            auto pixel = pixels[is_hflip ? 8 - j : j];
            //精灵像素透明
            if (!(pixel & 3)) continue;
            //精灵优先级
            if (!(sprites[i].attr & D5)) {
                //优先级为0, 精灵在背景前
                window_buffer[line*width + sprites[i].x + j] = pixel;
            } else if (!(window_buffer[line*width + sprites[i].x] & 3)) {
                //背景是透明色
                window_buffer[line*width + sprites[i].x + j] = pixel;
            }
        }
    }
}

auto PPU::get_pixels(unsigned int x, unsigned int y, uint16_t name_table) -> std::array<uint8_t, 8> {

    //这里开始获取颜色
    //先获取调色板entry
    const uint16_t attribute_table = name_table + 0x3C0;
    const uint16_t attribute_index = (y >> 5)*8 + (x>>2);
    const uint16_t attribute = ppu_bus[attribute_table + attribute_index];
    const uint8_t attribute_offset = (x & 0x2) | ((y & 0x10) >> 2);
    const uint8_t palette_entry = ((attribute >> attribute_offset) & 0x3) << 2;

    //然后看图案表
    //确定图案表
    const uint16_t pattern_table = (reg2000 & D4) ? 0x1000 : 0;
    //获取图案表tile地址
    const uint16_t pattern_index = (uint16_t)ppu_bus[name_table + (y >> 3)*32 + x];
    const TileRow tile_row = get_tile_pixels(pattern_index, pattern_table, y&7);
    return decode_tile(tile_row, palette_entry);
}

auto PPU::get_tile_pixels(uint16_t index, uint16_t pattern_table, uint8_t row_in_tile) -> TileRow {
    return TileRow {
        Bus::get().ppu_read(pattern_table + (index<<4) + row_in_tile),
        Bus::get().ppu_read(pattern_table + (index<<4) + row_in_tile + 8)
    };
}

auto PPU::decode_tile(TileRow tile_row, uint16_t palette_entry) -> std::array<uint8_t, 8> {
    //八个颜色
    const uint8_t pixel0246 = (tile_row.low&0b01010101) | ((tile_row.high&0b01010101)<<1);
    const uint8_t pixel1357 = ((tile_row.low&0b10101010)>>1) | (tile_row.high&0b10101010);
    return std::array<uint8_t, 8>{
        static_cast<uint8_t>(((pixel1357>>6u) & 0b11u) | palette_entry),
        static_cast<uint8_t>(((pixel0246>>6u) & 0b11u) | palette_entry),
        static_cast<uint8_t>(((pixel1357>>4u) & 0b11u) | palette_entry),
        static_cast<uint8_t>(((pixel0246>>4u) & 0b11u) | palette_entry),
        static_cast<uint8_t>(((pixel1357>>2u) & 0b11u) | palette_entry),
        static_cast<uint8_t>(((pixel0246>>2u) & 0b11u) | palette_entry),
        static_cast<uint8_t>(((pixel1357    ) & 0b11u) | palette_entry),
        static_cast<uint8_t>(((pixel0246    ) & 0b11u) | palette_entry),
    };
}
