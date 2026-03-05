#pragma once

#include <stdint.h>
#include <array>   
#include <memory>

class Cartridge;
class CPU;
class PPU;
class Mapper;
class Controller;

//这里要使用单例模式
class Bus {
    std::array<uint8_t, 2048> cpu_ram;  //cpu的运存
    std::shared_ptr<Cartridge> cart;    //游戏卡带
    std::shared_ptr<Mapper> mapper;     //映射方式
    std::shared_ptr<PPU> ppu;           //PPU指针
    std::shared_ptr<Controller> controller_device;
    Bus();
public:
    ~Bus() = default;
    Bus(const Bus &bus) = delete;
    Bus(const Bus &&bus) = delete;
    static inline constexpr Bus &get() {
        static Bus instance;
        return instance;
    };

    CPU &get_CPU();
    inline auto &get_PPU() {
        return ppu;
    }
    inline auto &get_cart() {
        return cart;
    }
    inline auto &get_cpu_ram() {
        return cpu_ram;
    }

    inline std::shared_ptr<Controller> &get_controller() {
        return controller_device;
    }
    void cpu_write(uint16_t addr, uint8_t value);
    uint8_t cpu_read(uint16_t addr);
    void ppu_write(uint16_t addr, uint8_t value);
    uint8_t ppu_read(uint16_t addr);
    void load_cart(std::shared_ptr<Cartridge>&& cartridge);
};

