#pragma once

#include <stdint.h>
#include <memory>
#include <array>
#include <const.h>

class PPU {
public:
    PPU() = default;
    ~PPU() = default;

    inline auto &get_window_buffer() {
        return window_buffer;
    }
    //逻辑部分
    uint8_t cpu_read(uint16_t addr);
    void cpu_write(uint16_t addr, uint8_t value);
    void scan();

    /// @brief 
    /// @return 该帧是否结束
    bool is_frame_end();

private:
    bool is_rendering();
    uint8_t ppu_ram_read(uint16_t addr);
    void ppu_ram_write(uint16_t addr, uint8_t value);

    /// @brief 提供精度为扫描行的渲染
    auto scan_one_line() -> void;

    //瓦片其中一行的高低字节
    struct TileRow{
        uint8_t low;
        uint8_t high;
    };

    /// @brief 获取名称表对应的8个像素
    /// @param x 行像素索引, 对应像素点坐标为`[x * 8, x * 8 + 7]`. 范围为 `0-32`
    /// @param y 列像素索引, 范围为`0-240`
    /// @return 8个像素的调色板索引
    auto get_pixels(unsigned int x, unsigned int y, uint16_t name_table) -> std::array<uint8_t, 8>;

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
    auto render_sprites_one_line(unsigned int line) -> void;

    /// @brief 渲染一行背景
    /// @param line 行
    auto render_background_one_line(unsigned int line) -> void;

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
    std::array<uint32_t, width * height> window_buffer; //屏幕像素的缓冲区
    std::array<uint8_t, 0x1000> ppu_bus;        //专用总线 2kb 调色板单独拿出来
    std::array<uint8_t, 0x1F> palette_indexes;  //调色板
    std::array<uint8_t, 0x100> OAM;               //OAM精灵内存, 64个精灵
    //内部寄存器
    uint8_t regw = 0;   
    uint8_t regx = 0;
    uint16_t regv = 0;
    uint16_t regt = 0;
    //暴露的寄存器
    uint8_t reg2000 = 0;
    uint8_t reg2001 = 0;
    uint8_t reg2002 = 0;
    uint8_t reg2003 = 0;
    // uint8_t reg2004 = 0;
    // uint8_t reg2005 = 0;
    uint8_t scroll_X = 0;
    uint8_t scroll_Y = 0;
    // std::array<uint8_t, 0x8> reg{0, 0, 0, 0, 0, 0, 0, 0};           //八个寄存器 没有多线程安全
    // uint8_t reg4014 = 0;
           //部分寄存器需要连续写入

    //部分比较特殊的寄存器
    uint16_t reg2006 = 0;
    uint8_t vram_buffer = 0;        //VRAM的缓冲区

    bool is_vblank = false;                 //是否处于vblank状态
    int current_scanline = 0;  // 当前扫描行
};