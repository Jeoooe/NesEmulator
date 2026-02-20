#include <bus.h>
#include <memory>
#include <mapper.h>
#include <cartridge.h>
#include <ppu.h>

Bus::Bus() {
    ppu = std::make_shared<PPU>();
}

// 未实现多线程支持
void Bus::cpu_write(uint16_t addr, uint8_t value) {
    if (addr < 0x2000) {
        //Ram
        cpu_ram[addr & 0x7FF] = value;
        return;
    }
}

uint8_t Bus::cpu_read(uint16_t addr) {
    //Ram
    if (addr < 0x2000) {
        return cpu_ram[addr & 0x7FF];
    }
    
    //ROM
    if (addr >= 0x8000) {
        addr = mapper->map_read(addr);
        return cart->prg_rom[addr];
    }
    return 0;
}

void Bus::load_cart(std::shared_ptr<Cartridge>&& cartridge) {
    if (cartridge) {
        cart = std::move(cartridge);
    }
    mapper = MapperFactory::create_mapper(cart);
}

std::shared_ptr<Cartridge>& Bus::get_cart(){
    return cart;
}
