#pragma once

#include <stdint.h>
#include <array>   
#include <memory>

class Cartridge;
class CPU;
class PPU;
class Mapper;

//这里要使用单例模式
class Bus {
    std::array<uint8_t, 2048> cpu_ram;  //cpu的运存
    std::shared_ptr<Cartridge> cart;    //游戏卡带
    std::shared_ptr<Mapper> mapper;     //映射方式
    std::shared_ptr<PPU> ppu;           //PPU指针
    Bus();
public:
    ~Bus() = default;
    Bus(const Bus &bus) = delete;
    Bus(const Bus &&bus) = delete;
    static inline Bus &get() {
        static Bus instance;
        return instance;
    };

    CPU &get_CPU();
    inline std::shared_ptr<PPU> &get_PPU() {
        return ppu;
    }
    inline std::shared_ptr<Cartridge> &get_cart() {
        return cart;
    }

    void cpu_write(uint16_t addr, uint8_t value);
    uint8_t cpu_read(uint16_t addr);
    void ppu_write(uint16_t addr, uint8_t value);
    uint8_t ppu_read(uint16_t addr);
    void load_cart(std::shared_ptr<Cartridge>&& cartridge);
};

