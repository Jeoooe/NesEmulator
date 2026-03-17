#pragma once

#include <stdint.h>
#include <memory>
#include <array>
#include <const.h>

#ifdef TEST_WINDOW
class DebugEmulatorWindow;
#endif

class PPU {
public:
    PPU();
    ~PPU() = default;

    inline auto &get_window_buffer() {
        return window_buffer;
    }
    inline auto &get_render_buffer() {
        return render_buffer;
    }
    inline auto &get_palette_indexes() {
        return palette_indexes;
    }
    inline uint8_t **get_mirror_palette() {
        return mirror_palette;
    }
    //逻辑部分
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t value);
    void scan();
    void reset();

    /// @brief 
    /// @return 该帧是否结束
    bool is_frame_end();

    #ifdef TEST_WINDOW
    void set_window(DebugEmulatorWindow* wind) {
        window = wind;
    }

    /// @brief 调试用, 直接渲染出整个名称表
    /// @param window 
    /// @return 
    auto render_full_screen(DebugEmulatorWindow *window) -> void;

    void update_tile(unsigned int index);
    #endif
private:
    bool is_rendering();
    uint8_t ppu_ram_read(uint16_t addr);
    void ppu_ram_write(uint16_t addr, uint8_t value);
    void trigger_vblank();

    /// @brief 提供精度为扫描行的渲染
    auto scan_one_line() -> void;

    //瓦片其中一行的高低字节
    struct TileRow{
        uint8_t low;
        uint8_t high;
    };

    /// @brief 获取目前regv指示名称表的瓦片的八个像素
    /// @return 8个像素的调色板索引
    auto get_pixels() -> std::array<uint8_t, 8>;

    /// @brief 获取图案表中一个瓦片的一行像素
    /// @param index 图案表瓦片索引
    /// @param pattern_table 图案表起始地址
    /// @param row_in_tile 瓦片中的行偏移
    /// @return 瓦片行的两个字节
    auto get_tile_pixels(uint16_t index, uint16_t pattern_table, uint8_t row_in_tile) -> TileRow;

    /// @brief 解码瓦片行
    /// @param tile 瓦片行
    /// @param palette_entry 调色板条目 (在`palette_indexes`中的索引)
    /// @return 8个像素对应的调色板地址 (在`palette_indexes`中的索引)
    auto decode_tile(TileRow tile, uint16_t palette_entry) -> std::array<uint8_t, 8>;

    /// @brief 渲染一行精灵
    /// @param line 行
    auto render_sprites_one_line() -> void;

    /// @brief 渲染一行背景
    /// @param line 行
    auto render_background_one_line() -> void;

    /*
        重构部分
    */

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
    
    // bit15表示 背景0/精灵1 像素
    // std::array<uint16_t, width> render_buffer; //渲染缓冲区, 存储调色板索引

    // abcx xxxx
    // D7 = sp0, D6 = is_sprite, D5 = prio
    std::array<uint8_t, width * height + 256> render_buffer; //渲染缓冲区, 存储调色板索引. 给256缓冲,防止越界
    std::array<uint32_t, width * height>  window_buffer; //屏幕像素的缓冲区, 存储最终的颜色
    std::array<uint8_t, 0x1000> ppu_bus;        //专用总线 2kb 调色板单独拿出来
    std::array<uint8_t, 0x20> palette_indexes;  //调色板
    std::array<uint8_t, 0x100> OAM;               //OAM精灵内存, 64个精灵
    //内部寄存器
    uint8_t regw = 0;       //1bit 控制二次写入
    uint8_t regx = 0;       //3bit 控制瓦片内X偏移

    /* 15bit
    `yyy NN YYYYY XXXXX`
    */
    uint16_t regv = 0;   
    /* 15bit
    `yyy NN YYYYY XXXXX`
    */   
    uint16_t regt = 0;
    //暴露的寄存器
    uint8_t reg2000 = 0;
    uint8_t reg2001 = 0;
    uint8_t reg2002 = 0;
    uint8_t OAMADDR = 0;
    // uint8_t reg2004 = 0;
    // uint8_t reg2005 = 0;
    // uint8_t scroll_X = 0;
    // uint8_t scroll_Y = 0;
    // std::array<uint8_t, 0x8> reg{0, 0, 0, 0, 0, 0, 0, 0};           //八个寄存器 没有多线程安全
    // uint8_t reg4014 = 0;
           //部分寄存器需要连续写入

    //部分比较特殊的寄存器
    // uint16_t reg2006 = 0;
    uint8_t vram_buffer = 0;        //VRAM的缓冲区

    bool is_vblank = false;                 //是否处于vblank状态
    bool vblank_triggered = false;
    //设置为-2时, 标志该帧结束, 并且检测之后会置为-1
    int current_scanline = -1;  //当前扫描行
    int cycle = 0;             //当前行的周期数

    uint8_t *mirror_palette[32];    //镜像的调色板

    #ifdef TEST_WINDOW
    DebugEmulatorWindow *window;
    #endif
};