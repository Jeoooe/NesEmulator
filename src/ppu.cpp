#include <ppu.h>

#include <cstring>

#include <bus.h>
#include <cartridge.h>
#include <const.h>
#include <cpu.h>
#include <log.h>
#include <timing.h>
#include <window.h>

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

#define SpriteOverflow D5
#define Sprite0Hit D6
#define VblankFlag D7

#define Sprite0Flag D7
#define SpritePixelFlag D6
#define SpritePriorityFlag D5


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

//映射自己的显存
inline uint16_t ppu_ram_map(uint16_t addr) {
    const auto &cart = Bus::get().get_cart();
    const uint16_t table = (addr >> 10) & 0x3;
    const uint16_t offset = addr & 0x03FF;
    const int mirroring = cart ? cart->mirroring : 0;

    switch (mirroring) {
    case 1: // vertical: [A, B, A, B]
        return ((table & 0x1) << 10) | offset;
    case 2: // four-screen: [A, B, C, D]
        return addr & 0x0FFF;
    default: // horizontal: [A, A, B, B]
        return (((table >> 1) & 0x1) << 10) | offset;
    }
}

PPU::PPU() {
    for (int i = 0;i < 32;i++) {
        if (i & 3) mirror_palette[i] = &palette_indexes[i];
        else mirror_palette[i] = &palette_indexes[0];
    }
}

uint8_t PPU::cpu_read(uint16_t addr)
{
    static uint8_t latch = 0;
    //非vblank状态不进行处理
    // if (is_rendering()) return 0;

    if (addr >= 0x2000 && addr < 0x4000) {
        addr &= 0x2007;
    }


    switch (addr) {
    case 0x2002: 
    {
        uint8_t snapshot = reg2002;
        reg2002 &= ~D7;
        regw = 0;
        // LOG("Read 2002: 0x%02x\n", snapshot);
        return latch = snapshot;
    }

    case 0x2004:
        return latch = OAM[OAMADDR];
    case 0x2007:
    {    
        uint8_t value = ppu_ram_read(regv);
        regv += (uint16_t)((reg2000 & D2) ? 32 : 1);
        regv &= 0x7FFF;
        return latch = value;
    }
    default:
        return latch;
    }
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
        {
            bool old_nmi_enable = reg2000 & D7;
            reg2000 = value;
            regt &= ~0x0C00;
            regt |= ((uint16_t)value & 0x3) << 10;
            bool new_nmi_enable = reg2000 & D7;
            // LOG("Write 2000: 0x%02x\n", reg2000);
            /*
                重大Bug修复
                reg2000 NMI enable位的置位边沿才会触发vblank
            */
            //这里如果处于vblank状态, 且enable从0->1的边沿
            if (is_vblank && !old_nmi_enable && new_nmi_enable) {
                trigger_vblank();
            }
        }
        break;

    case 0x2001:
        // LOG("Write in 2001: 0x%02x\n", value);
        reg2001 = value;
        break;
    case 0x2003:
        OAMADDR = value;
        break;
    case 0x2004:
        OAM[OAMADDR++] = value;
        break;
    case 0x2005:
        if (!regw) {
            regt &= ~0b11111;
            regt |= value >> 3;
            regx = value & 0x7;
            regw = 1;
            // LOG("Write $2005 = 0x%02x",   value);
        } else {
            regt &= ~0b111001111100000;
            regt |= ((uint16_t)value & 0b11111000) << 2;
            regt |= ((uint16_t)value & 0b111) << 12;
            regw = 0;
            // LOG("%02x\n", value);
        }
        
        break;
    case 0x2006:
        if (!regw) {
            regt &= ~0x7F00;
            regt |= (uint16_t)(value & 0x3F) << 8;
            regw = 1;
            // LOG("Write $2006: 0x%02x", value);
        } else {
            regt &= ~0x00FF;
            regt |= (uint16_t)value;
            regw = 0;
            regv = regt;
            // LOG(" %02x\n", value);
        }
        break;
    case 0x2007:
        ppu_ram_write(regv, value);
        regv += (uint16_t)((reg2000 & D2) ? 32 : 1);
        regv &= 0x7FFF;
        break;
    case 0x4014:
        //这里触发DMA直接存储
        // if (false) {
        {
            //BUG
            //这里存在可能的Bug
            //2. 时钟周期不同
            uint16_t offset = (uint16_t)value << 8;
            auto &ram = Bus::get().get_cpu_ram();
            for (int i = 0;i < 256;i++) {
                OAM[OAMADDR++] = ram[offset++];
            }
            //这里需要同步扫描
            //直接调用扫描五次应该就行了, 最多应该也就五行
            uint64_t cpu_cycle = Timing::get_current_tick() / 3;
            Timing::step_cpu_tick(513 + (cpu_cycle & 1)); //直接占用513周期?无所谓了, 先这样
            scan();
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
        const uint8_t snapshot = vram_buffer;
        vram_buffer = Bus::get().ppu_read(addr);
        return snapshot;
    } 
    if (addr < 0x3F00) {
        if (addr >= 0x3000) {
            addr -= 0x1000;
        }
        const uint8_t snapshot = vram_buffer;
        vram_buffer = ppu_bus[ppu_ram_map(addr - 0x2000)];
        return snapshot;
    }

    {    //这里是调色板
        vram_buffer = ppu_bus[ppu_ram_map(addr - 0x3000)];
        addr &= 0x1F;
        return palette_indexes[addr];
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
        if (Bus::get().get_cart()->use_chr_ram) {
            Bus::get().ppu_write(addr, value);
        }
        return;
    }
    if (addr < 0x3F00) {
        if (addr >= 0x3000) {
            addr -= 0x1000;
        }
        addr = ppu_ram_map(addr - 0x2000);
        ppu_bus[addr] = value;
        #ifdef TEST_WINDOW
        //写入名称表就渲染一次
        if (value && (addr & ~0x0C00) < 0x3C0) {
            update_tile(addr);
            #ifndef NO_UI
            window->render_tile(addr);
            #endif
            // if (addr >= 0x400) {
            //     LOG("Write 0x%04x\n", addr);
            // }
        }
        #endif
        return;
    }
    
    {
        if (addr & 3) {
            palette_indexes[addr & 0x1F] = value;
        } else {
            addr &= 0x0F;
            palette_indexes[addr] = palette_indexes[addr | 0x10] = value;
        }
    }
}

bool PPU::is_frame_end() {
    if (frame_ended) {
        frame_ended = false;
        return true;
    }
    return false;
}

bool PPU::is_rendering() {
    return !is_vblank && (reg2001 & (D3 | D4));
}

void PPU::trigger_vblank() {
    //开启vblank, 处于vblank且未被触发过
    if ((reg2000 & D7) && is_vblank && !vblank_triggered) {
        Bus::get().get_CPU().trigger_NMI();
        vblank_triggered = true;
    }
}

void PPU::scan() {
    /*
        内容参见 https://www.nesdev.org/wiki/PPU_rendering
    */


    static struct Sprite {
        uint8_t y; 
        uint8_t tile_index;
        uint8_t attr;
        uint8_t x;
    } sec_oam[8];
    static uint8_t internal_reg[4] = {0, 0, 0, 0};  //内部读取瓦片的寄存器
    static uint8_t shift_buffer[16];                //移位缓冲区
    static int shift_switch = 0;                
    static int write_switch = 0;
    static int sprite_count = 0;    //精灵评估的数量
    static int sprite_entry = 0;    //正在评估第几个精灵
    static bool has_sprite0 = 0;
    // static uint8_t sprite_read_buffer = 0;  //精灵评估时的读取值
    static int render_pos = 0;
    static uint64_t next_cycle_tick = 0;
    // -2 不处理 直接给is_frame_end消费, 可能会导致缺几百个周期?
    if (current_scanline == -2) return;
    auto current_tick = Timing::get_current_tick();
    for (;current_tick > next_cycle_tick; next_cycle_tick++) {
        //考虑扫描线是什么
        if (current_scanline == 241 && cycle == 1) {
            //触发vblank
            is_vblank = true;
            vblank_triggered = false;
            reg2002 |= D7;
            trigger_vblank();
            frame_ended = true; //在vblank的时候设置帧结束, 然后渲染
            goto scan_behavior_end;
        }

        if (current_scanline == -1) { 
            if (cycle == 1) {
                //清空vblank sprite0标志 sprite overflow标志
                is_vblank = false;
                reg2002 &= ~(D7 | D6 | D5); 
                render_pos = 0;
            } 
            if (!is_rendering()) goto scan_behavior_end;

            if (280 <= cycle && cycle <= 304) {
                regv &= ~0b111101111100000;
                regv |= regt & 0b111101111100000;  
                OAMADDR = 0;
            }
            if (cycle >= 321) goto scan_begin;
            goto scan_behavior_end;
        }

        if (240 <= current_scanline && current_scanline <= 260 ) {
            goto scan_behavior_end;
        }


        //没开启渲染的话就只需要考虑vblank 和-1清标志位 ?
        scan_begin:
        if (!is_rendering()) {
            goto scan_behavior_end;
        }

        if (!(reg2001 & D3)) {
            //未开启背景渲染 
            goto scan_sprite;
        }
        
        if ((  1 <= cycle && cycle <= 256) || (321 <= cycle && cycle <= 336)) {
        // if ((1 <= cycle && cycle <= 256)) {
            switch (cycle & 0b111) {
            case 0b001:     //读取名称表
            {
                //获取图案表tile地址
                const uint16_t tile_addr = regv & 0x0FFF;
                internal_reg[0] = ppu_bus[ppu_ram_map(tile_addr)];
                break;
            }
            case 0b011:     //读取属性表
            {
                const uint16_t attribute_addr = 0x03C0 | (regv & 0x0C00) | ((regv >> 4) & 0x38) | ((regv >> 2) & 0x07);
                internal_reg[1] = ppu_bus[ppu_ram_map(attribute_addr)];
                // const uint8_t attribute_offset = (regv & 0x2) | ((regv & 0x40) >> 4);
                // const uint8_t palette_entry = ((attribute >> attribute_offset) & 0x3) << 2;
                break;
            }
            case 0b101:     //图案表低字节
            {
                const uint16_t pattern_table = (reg2000 & D4) << 8;
                const uint16_t index = internal_reg[0];
                const uint8_t row_in_tile = (regv >> 12) & 0x7;
                internal_reg[2] = Bus::get().ppu_read(pattern_table | ((index)<<4) | row_in_tile);
                break;
            }
            case 0b111:
            {
                const uint16_t pattern_table = (reg2000 & D4) << 8;
                const uint16_t index = internal_reg[0];
                const uint8_t row_in_tile = (regv >> 12) & 0x7;
                internal_reg[3] = Bus::get().ppu_read(pattern_table | (index<<4) | row_in_tile | 8);;
                break;
            }
            case 0:
            {
                //这里要先渲染一个再加载数据
                if (cycle <= 256) {
                    for (int i = 0;i < 8;i++) {
                        //这里要检测精灵
                        uint16_t pixel = render_buffer[render_pos];
                        int shift = (regx + shift_switch + i) & 15;
                        //sp0碰撞
                        //是sp0, 两者不透明. 
                        if ((pixel & Sprite0Flag) && (shift_buffer[shift] & 3)) {
                            reg2002 |= Sprite0Hit;
                        }
                        //这里再考虑渲染
                        if (!(pixel & SpritePixelFlag) || 
                            ((pixel & SpritePriorityFlag) && (shift_buffer[shift] & 3))
                        ) {
                            // 不是精灵
                            // 或者精灵是透明
                            // 或者精灵优先级=1, 即在背景后面, 并且背景不是透明
                            render_buffer[render_pos] = shift_buffer[shift];
                        }
                        render_buffer[render_pos] &= 0x1F;  //然后直接清掉所有标志, 给渲染用
                        render_pos++;
                    }
                    shift_switch = (shift_switch + 8) & 15;
                }
                //这里也是上一次读取的结束, 加入渲染.
                const uint8_t attribute_offset = (regv & 0x2) | ((regv & 0x40) >> 4);
                const uint8_t palette_entry = ((internal_reg[1] >> attribute_offset) & 0x3) << 2;
                const auto pixels = decode_tile(
                    TileRow{ internal_reg[2], internal_reg[3] }, palette_entry
                );
                for (int i = 0;i < 8;i++) {
                    shift_buffer[write_switch + i] = pixels[i];
                }
                write_switch = (write_switch + 8) & 15;
                
                //这里进行X增加
                if ((regv & 0x001F) == 31) {
                    regv &= ~0x001F;
                    regv ^= 0x0400;
                } else {
                    regv++;
                }
            }
            }   
        }
        
        //这里负责精灵渲染
        scan_sprite:
        if (!(reg2001 & D4) || current_scanline == -1) {
            //未开启精灵渲染
            goto scan_2_end;
        }

        //1-64覆写应该没有必要
        
        //直接模拟也太难了, 还是直接一次性评估吧
        if (cycle == 256) {
            Sprite *oam = (Sprite *)OAM.data();
            while (sprite_entry < 64) {
                int diff = current_scanline - oam[sprite_entry].y;
                int sprite_high = (reg2000 & D5) ? 16 : 8;
                if (diff >= 0 && diff < sprite_high) {
                    if (sprite_count < 8) {
                        sec_oam[sprite_count] = oam[sprite_entry];
                    }
                    if (sprite_entry == 0) has_sprite0 = true;
                    sprite_count++; //这个用作精灵溢出
                }
                sprite_entry++;
            }
            if (sprite_count >= 8) reg2002 |= SpriteOverflow;
            goto scan_2_end;
        }

        if (cycle == 320 && current_scanline < 239) {
            //这里直接在末尾获取精灵数据了
            //直接绘制到下一行像素里面
            bool is_8x16 = reg2000 & D5;
            for (int i = sprite_count - 1;i >= 0;i--) {
                int row_in_tile = current_scanline - sec_oam[i].y;
                if (sec_oam[i].attr & D7) {
                    //垂直翻转
                    if (is_8x16) row_in_tile = 15 - row_in_tile;
                    else row_in_tile = 7 - row_in_tile;
                }
                //对于8x8和8x16而言, 区别在选择瓦片
                uint8_t tile_index = sec_oam[i].tile_index;
                uint16_t pattern_table = (reg2000 & D3) << 9;
                if (is_8x16) {
                    pattern_table = (tile_index & 1) ? 0x1000 : 0;
                    tile_index &= ~1;
                    tile_index |= (row_in_tile > 7) ? 1 : 0;
                    row_in_tile &= 7;
                }
                const auto tile = get_tile_pixels(tile_index, pattern_table, row_in_tile);
                const auto pixels = decode_tile(tile, ((sec_oam[i].attr & 0x3)<<2) | 0x10);
                const bool is_hflip = sec_oam[i].attr & D6;
                const uint16_t sp0_flag = (i == 0 && has_sprite0) ? Sprite0Flag : 0;
                for (int j = 0;j < 8;j++) {
                    uint16_t pixel = pixels[is_hflip ? 7 - j : j];
                    int pos = sec_oam[i].x + j;
                    if (pos < 0 || pos >= 256 || (pixel & 3) == 0) continue;
                    //检测在背景渲染部分
                    pixel |= (sec_oam[i].attr & SpritePriorityFlag) | sp0_flag | SpritePixelFlag;
                    render_buffer[(current_scanline + 1)*width + pos] = pixel;
                }
            }
        }


        scan_2_end:

        //337 - 340 不做什么
        
        //cycle == 0 不需要额外代码
        //下面是每行的重置寄存器
        if (current_scanline >= 240) {
            goto scan_behavior_end;
        }
        if (cycle == 256) {
            //重置Y
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
        }
        if (cycle == 257) {
            //重置X
            regv &= ~0b000010000011111;
            regv |= regt & 0b000010000011111;
        }
        
        scan_behavior_end:
        cycle++;
        if (cycle == 341) {
            cycle = 0;
            current_scanline++;
            sprite_entry = sprite_count = 0;
            has_sprite0 = false;
            if (current_scanline == 261) {
                current_scanline = -1;
                break;
            }
        }

    }
}



// void PPU::scan_one_line() {  
//     static int odd_frame = 0;
//     //修复奇数偶数帧的问题?
//     //Pre-render
//     if (current_scanline == -1) {
//         //更新数据  
//         is_vblank = false;
//         /*
//             FIX:这里是极其严重的bug
//             当PPU处于关闭渲染状态时, -1扫描线不会对寄存器进行操作
//         */
//         if (is_rendering()) {
//         // {
//             // 预渲染线开始时, 把滚动目标地址恢复到当前VRAM地址。
//             // 否则CPU在vblank期间写名称表后留下的regv会直接影响下一帧取背景。
//             regv &= ~0b111101111100000;
//             regv |= regt & 0b111101111100000;  
//             OAMADDR = 0;
//         }
//     }
//     // 判断是否在可见扫描线区域（0-239为可见扫描线）
//     if (0 <= current_scanline && current_scanline <= 239) {
//         //这里仅仅是把window_buffer的值设置为调色板的索引
//         render_background_one_line();
//         render_sprites_one_line();
//         #ifndef TEST_WINDOW
//         for (int i = 0;i < width;i++) {
//             const auto index = (render_buffer[i] & 3) ? (render_buffer[i] & 0xFF) : 0;
//             window_buffer[current_scanline*width + i] = nes_palette[palette_indexes[index]];
//         }
//         #endif
//     }

//     if (current_scanline == 240) {
//         //开启vblank
//         is_vblank = true;
//         vblank_triggered = false;
//         reg2002 |= D7;
//         trigger_vblank();
//     }

//     //扫描线260的最后要执行的,
//     //也有-1扫描线刚开始会做的
//     if (current_scanline == 260) {
//         reg2002 &= ~(D7 | D6 | D5);  //清空vblank sprite0标志 sprite overflow标志
//         current_scanline = -2;  //-2则为帧结束
//         Timing::set_tick(Timing::get_current_tick() - odd_frame);
//         odd_frame = 1 - odd_frame;
//         return;
//     }
    
//     // 更新扫描线计数
//     current_scanline++;
// }

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
            col = 256;
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
    uint16_t pattern_table = (reg2000 & D3) ? 0x1000 : 0;
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
            // LOG("SP0: x = %d, y = %d, attr = 0x%02x\n", sprites[0].x, sprites[0].y, sprites[0].attr);
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
        // LOG("Sprite %d: y = %d, ind = 0x%x, attr = 0x%x, x  = %d\n, 8x16: %d", i,
        // sprites[i].y, sprites[i].tile_index, sprites[i].attr, sprites[i].x);
        int row_in_tile = line - sprites[i].y;
        if (sprites[i].attr & D7) {
            //垂直翻转
            if (is_8x16) row_in_tile = 15 - row_in_tile;
            else row_in_tile = 7 - row_in_tile;
        }
        //对于8x8和8x16而言, 区别在选择瓦片
        uint8_t tile_index = sprites[i].tile_index;
        if (is_8x16) {
            pattern_table = (tile_index & 1) ? 0x1000 : 0;
            tile_index &= ~1;
            tile_index |= (row_in_tile > 7) ? 1 : 0;
            row_in_tile &= 7;
        }
        const auto tile = get_tile_pixels(tile_index, pattern_table, row_in_tile);
        const auto pixels = decode_tile(tile, ((sprites[i].attr & 0x3)<<2) | 0x10);
        const bool is_hflip = sprites[i].attr & D6;
        for (int j = 0;j < 8;j++) {
            auto pixel = pixels[is_hflip ? 7 - j : j];
            int pos = sprites[i].x + j;
            if (pos < 0 || pos >= 256) continue;
            //精灵像素透明
            if (!(pixel & 3)) continue;
            //检测sprite 0命中
            //是sp0, 有sp0存在, 该位置是背景像素, 并且背景和精灵均不是透明
            if (i == 0 && has_sp0 && !(render_buffer[pos] & D15) && (render_buffer[pos] & 3)) {
                reg2002 |= D6;
                // LOG("SP0 hit\n");
            }
            //精灵优先级
            if (!(sprites[i].attr & D5)) {
                //优先级为0, 精灵在背景前
                render_buffer[pos] = pixel | D15;
            } else if (!(render_buffer[pos] & 3)) {
                //背景是透明色
                render_buffer[pos] = pixel | D15;
            }
        }
    }
}

//获取背景瓦片
auto PPU::get_pixels() -> std::array<uint8_t, 8> {
    //这里开始获取颜色
    //先获取调色板entry
    const uint16_t attribute_addr = 0x03C0 | (regv & 0x0C00) | ((regv >> 4) & 0x38) | ((regv >> 2) & 0x07);
    const uint16_t attribute = ppu_bus[ppu_ram_map(attribute_addr)];
    const uint8_t attribute_offset = (regv & 0x2) | ((regv & 0x40) >> 4);
    const uint8_t palette_entry = ((attribute >> attribute_offset) & 0x3) << 2;

    //然后看图案表
    //确定图案表
    const uint16_t pattern_table = (reg2000 & D4) ? 0x1000 : 0;
    //获取图案表tile地址
    const uint16_t tile_addr = regv & 0x0FFF;
    const uint16_t pattern_index = (uint16_t)ppu_bus[ppu_ram_map(tile_addr)];
    const TileRow tile_row = get_tile_pixels(pattern_index, pattern_table, (regv >> 12) & 0x7);
    return decode_tile(tile_row, palette_entry);
}

auto PPU::get_tile_pixels(uint16_t index, uint16_t pattern_table, uint8_t row_in_tile) -> TileRow {
    // LOG("Tile %d, pattern_table=0x%04X, addr=0x%04X", index, pattern_table, pattern_table + (index<<4));
    return TileRow {
        Bus::get().ppu_read(pattern_table | (index<<4) | row_in_tile),
        Bus::get().ppu_read(pattern_table | (index<<4) | row_in_tile | 8)
    };
}

/// @brief 解码瓦片行
/// @param tile 瓦片行
/// @param palette_entry 调色板条目 (在`palette_indexes`中的索引)
/// @return 8个像素对应的调色板地址 (在`palette_indexes`中的索引)
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

#ifdef TEST_WINDOW
#include <window.h>
//这个函数直接往窗口里面写
auto PPU::render_full_screen(DebugEmulatorWindow *window) -> void {
    // if (!is_rendering()) return;
    const auto pattern_table = (reg2000 & D4) ? 0x1000 : 0;
    //调色板修改
    auto a1 = palette_indexes[0x4];
    auto a2 = palette_indexes[0x8];
    auto a3 = palette_indexes[0xC];
    palette_indexes[0xC] = palette_indexes[0x8] = palette_indexes[0x4] = palette_indexes[0];
    for (unsigned i = 0;i < 30;i++) {
        for (unsigned j = 0;j < 8;j++) {
            for (unsigned k = 0;k < 32;k++) {
                const uint16_t attribute_addr = 0x03C0  | ((i & ~3)<<1) | ((k >> 2) & 0x07);
                const uint16_t attribute = ppu_bus[ppu_ram_map(attribute_addr)];
                const uint8_t attribute_offset = (k & 0x2) | ((i & 0x2)<<1);
                const uint8_t palette_entry = ((attribute >> attribute_offset) & 0x3) << 2;
                const uint8_t index = ppu_bus[i * 32 + k];
                auto tile = get_tile_pixels(index, pattern_table, j);
                auto piexels = decode_tile(tile, palette_entry);
                for (unsigned w = 0;w < 8;w++) {
                    window->buffer[(i*8 + j) * DebugEmulatorWindow::test_width + k*8 + w] =
                        nes_palette[palette_indexes[piexels[w]]];
                }
            }
        }
    }
    palette_indexes[0x4] = a1;
    palette_indexes[0x8] = a2;
    palette_indexes[0xC] = a3;
}

void PPU::update_tile(unsigned int index) {
    const auto pattern_table = (reg2000 & D4) ? 0x1000 : 0;
    //调色板修改
    auto a1 = palette_indexes[0x4];
    auto a2 = palette_indexes[0x8];
    auto a3 = palette_indexes[0xC];
    palette_indexes[0xC] = palette_indexes[0x8] = palette_indexes[0x4] = palette_indexes[0];


    unsigned name_table = index & 0x400;
    unsigned offset = name_table >> 2;
    unsigned X = index & 0x1F;
    unsigned Y = (index >> 5) & 0x1F;
    const uint16_t attribute_addr = 0x03C0 | (name_table) | ((Y & ~3)<<1) | ((X >> 2) & 0x07);
    const uint16_t attribute = ppu_bus[ppu_ram_map(attribute_addr)];
    const uint8_t attribute_offset = (X & 0x2) | ((Y & 0x2)<<1);
    const uint8_t palette_entry = ((attribute >> attribute_offset) & 0x3) << 2;
    for (int i = 0;i < 8;i++) {
        auto tile = get_tile_pixels(ppu_bus[index], pattern_table, i);
        auto piexels = decode_tile(tile, palette_entry);
        for (unsigned w = 0;w < 8;w++) {
            window->buffer[(Y*8 + i) * DebugEmulatorWindow::test_width + X*8 + w + offset] =
                nes_palette[palette_indexes[piexels[w]]];
        }
    }

    palette_indexes[0x4] = a1;
    palette_indexes[0x8] = a2;
    palette_indexes[0xC] = a3;
}

#endif