#include <bus.h>
#include <memory>
#include <mapper.h>
#include <cartridge.h>
#include <ppu.h>
#include <cpu.h>

Bus::Bus() {
    ppu = std::make_shared<PPU>();
}

CPU &Bus::get_CPU() {
    return get_cpu();
}

// 未实现多线程支持
void Bus::cpu_write(uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        //Ram
        cpu_ram[addr & 0x7FF] = value;
        return;
    }
    if (0x2000 <= addr && addr < 0x4020) {
        //PPU registers
        ppu->cpu_write(addr, value);
    }
}

uint8_t Bus::cpu_read(uint16_t addr) {
    //Ram
    if (addr < 0x2000) {
        return cpu_ram[addr & 0x7FF];
    }

    if (0x2000 <= addr && addr < 0x4020) {
        //PPU registers
        return ppu->cpu_read(addr);
    }
    
    //ROM
    if (addr >= 0x8000) {
        addr = mapper->map_read(addr);
        return cart->prg_rom[addr];
    }
    return 0;
}

void Bus::ppu_write(uint16_t addr, uint8_t value) {
    mapper->ppu_map_write(addr, value);
}

uint8_t Bus::ppu_read(uint16_t addr) {
    return mapper->ppu_map_read(addr);
}

void Bus::load_cart(std::shared_ptr<Cartridge>&& cartridge) {
    if (cartridge) {
        cart = std::move(cartridge);
    }
    mapper = MapperFactory::create_mapper(cart);
}
