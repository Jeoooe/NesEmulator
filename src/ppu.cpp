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
#define D8 0x100
#define D9 0x200
#define D10 0x400
#define D11 0x800
#define D12 0x1000
#define D13 0x2000
#define D14 0x4000
#define D15 0x8000


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
    if (current_scanline == -2) {
        current_scanline = -1;
        return true;
    }
    return false;
}

bool PPU::is_rendering() {
    return !is_vblank && (reg2001 & (D3 | D4));
}

//映射自己的显存
inline uint16_t ppu_ram_map(uint16_t addr) {
    /*暂时只有垂直镜像, 即 
        A | B
        -----
        A | B
    */
    return addr >= 0x800 ? addr - 0x800 : addr;
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
        uint8_t value = ppu_ram_read(regv);
        regv += (reg2000 & D2) ? 32 : 1;
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
    
    /*
        这里寄存器的写入行为非常多
        参见 https://www.nesdev.org/wiki/PPU_scrolling
    */

    switch (addr) {
    case 0x2000:
        reg2000 = value;
        regt &= ~0x0C00;
        regt |= ((uint16_t)value & 0x3) << 10;
        break;

    case 0x2001:
        reg2001 = value;
        break;

    case 0x2003:
        reg2003 = value;
        break;
    case 0x2004:
        OAM[reg2003++] = value;
        break;
    case 0x2005:
        if (!regw) {
            regt &= ~0b11111;
            regt |= value >> 3;
            regx = value & 0x7;
            regw = 1;
        } else {
            regt &= ~0b111001111100000;
            regt |= ((uint16_t)value & 0b11111000) << 2;
            regt |= ((uint16_t)value & 0b111) << 12;
            regw = 0;
        }
        break;
    case 0x2006:
        if (!regw) {
            regt &= ~0x7F00;
            regt |= (uint16_t)(value & 0x3F) << 8;
            regw = 1;
        } else {
            regt &= ~0x00FF;
            regt |= (uint16_t)value;
            regw = 0;
            regv = regt;
        }
        break;
    case 0x2007:
        ppu_ram_write(regv, value);
        regv += (reg2000 & D2) ? 32 : 1;
        break;
    case 0x4014:
        //这里触发DMA直接存储
        {
            //BUG
            //这里存在可能的Bug
            //2. 时钟周期不同
            Timing::step_cpu_tick(512); //直接占用512周期?无所谓了, 先这样
            size_t r_addr = (size_t)value << 8;
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
    addr &= 0x3FFF;
    if (addr < 0x2000) {
        return Bus::get().ppu_read(addr);
    } 
    if (addr < 0x3000) {
        const uint8_t snapshot = vram_buffer;
        vram_buffer = ppu_bus[addr - 0x2000];
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
    addr &= 0x3FFF; //忽略高2位
    if (addr < 0x2000) {
        // return; //ROM应该不可写
        // FIX: 考虑到有CHR_RAM这种行为
        Bus::get().ppu_write(addr, value);
        return;
    }
    if (addr < 0x3000) {
        ppu_bus[addr - 0x2000] = value;
        return;
    } 
    if (addr < 0x3F00) {
        return;
    } else {
        if ((addr & 0x0F) == 0) {
            palette_indexes[0x0] = palette_indexes[0x4] = 
            palette_indexes[0x8] = palette_indexes[0xC] = 
            palette_indexes[0x10] = value;
        } else if (addr & 3) {
            palette_indexes[addr & 0x1F] = value;
        } else {
            addr &= 0x0F;
            palette_indexes[addr] = palette_indexes[addr | 0x10] = value;
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
    //Pre-render
    if (current_scanline == -1) {
        //更新数据  
        reg2002 &= ~D7;
        is_vblank = false;
        /*
            FIX:这里是极其严重的bug
            当PPU处于关闭渲染状态时, -1扫描线不会对寄存器进行操作
        */
        if (is_rendering()) {
            regv &= ~0b111101111100000;
            regv |= regt & 0b111101111100000;    
        }
    }
    // 判断是否在可见扫描线区域（0-239为可见扫描线）
    if (0 <= current_scanline && current_scanline <= 239) {
        //这里仅仅是把window_buffer的值设置为调色板的索引
        render_background_one_line();
        render_sprites_one_line();
        for (int i = 0;i < width;i++) {
            window_buffer[current_scanline*width + i] = nes_palette[palette_indexes[render_buffer[i]]];
        }
    }

    if (current_scanline == 241) {
        //开启vblank
        is_vblank = true;
        reg2002 |= D7;
        if (reg2000 & D7) {
            Bus::get().get_CPU().trigger_NMI();
        }
    }

    if (current_scanline == 260) {
        current_scanline = -2;  //-2则为帧结束
        return;
    }
    
    // 更新扫描线计数
    current_scanline++;
}

auto PPU::render_background_one_line() -> void {
    if (!(reg2001 & D3)) {
        //没有开启背景显示
        //直接设置为通用背景色
        memset(render_buffer.data(), 0, 256);
        return;
    }
    //有滚动就比较复杂
    //不考虑镜像
    unsigned int col = 0;
    //第一个瓦片存在偏移
    for (int i = 0;i < 33 && col < 256;i++) {
        //或许可以直接读取33个图块
        auto pixels = get_pixels();
        if (col < 8u - regx) {
            for (;col < 8u - regx;col++) {
                render_buffer[col] = pixels[regx + col];
            }
        } else if (256 - col < 8) {
            //最后一个图块的偏移
            for (unsigned int j = col;j < 256;j++) {
                render_buffer[j] = pixels[j - col];
            }
        } else {
            //中间的图块
            for (unsigned int j = 0;j < 8;j++) {
                render_buffer[col++] = pixels[j];
            }
        }
        //这里进行X增加
        if ((regv & 0x001F) == 31) {
            regv &= ~0x001F;
            regv ^= 0x0400;
        } else {
            regv++;
        }
    }

    //一行结束, 增加Y, 重置X
    if ((regv & 0x7000) != 0x7000) { //fine Y < 7
        regv += 0x1000;
    } else {
        regv &= ~0x7000;
        int y = (regv & 0x03E0) >> 5;   //coarse_y
        if (y == 29) {
            y = 0;
            regv ^= 0x0800;
        } else if (y == 31) {
            y = 0;
        } else {
            y += 1;
        }
        regv = (regv & ~0x03E0) | (y << 5);
    }
    //重置X
    regv &= ~0b000010000011111;
    regv |= regt & 0b000010000011111;
}



auto PPU::render_sprites_one_line() -> void {
    const uint8_t line = current_scanline;
    if (!(reg2001 & D4)) {
        //不显示精灵
        return;
    }
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
                render_buffer[sprites[i].x + j] = pixel;
            } else if (!(render_buffer[sprites[i].x] & 3)) {
                //背景是透明色
                render_buffer[sprites[i].x + j] = pixel;
            }
        }
    }
}

//获取背景瓦片
auto PPU::get_pixels() -> std::array<uint8_t, 8> {

    //这里开始获取颜色
    //先获取调色板entry
    const uint16_t tile_addr = regv & 0x0FFF;
    const uint16_t attribute_addr = 0x03C0 | (regv & 0x0C00) | ((regv >> 4) & 0x38) | ((regv >> 2) & 0x07);
    const uint16_t attribute = ppu_bus[attribute_addr];
    const uint8_t attribute_offset = (regv & 0x2) | ((regv & 0x60) >> 3);
    const uint8_t palette_entry = ((attribute >> attribute_offset) & 0x3) << 2;

    //然后看图案表
    //确定图案表
    const uint16_t pattern_table = (reg2000 & D4) ? 0x1000 : 0;
    //获取图案表tile地址
    const uint16_t pattern_index = (uint16_t)ppu_bus[tile_addr];
    const TileRow tile_row = get_tile_pixels(pattern_index, pattern_table, (regv >> 12) & 0x7);
    return decode_tile(tile_row, palette_entry);
}

auto PPU::get_tile_pixels(uint16_t index, uint16_t pattern_table, uint8_t row_in_tile) -> TileRow {
    // LOG("Tile %d, pattern_table=0x%04X, addr=0x%04X", index, pattern_table, pattern_table + (index<<4));
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
