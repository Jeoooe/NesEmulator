#include <bus.h>

#include <fstream>
#include <cstring>
#include <memory>

#include <apu.h>
#include <mapper.h>
#include <cartridge.h>
#include <ppu.h>
#include <cpu.h>
#include <controller.h>

Bus::Bus() {
    ppu = std::make_shared<PPU>();
    apu = std::make_shared<APU>();
    controller_device = std::make_shared<Controller>();
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
    if (0x2000 <= addr && addr < 0x4000) {
        //PPU registers
        ppu->cpu_write(addr, value);
        return;
    }
    if (addr == 0x4014) {
        ppu->cpu_write(addr, value);
        return;
    }

    //Controller
    if (addr == 0x4016) {
        controller_device->cpu_write(addr, value);
        return;
    }

    //APU
    if (addr >= 0x4000 && addr < 0x4018) {
        apu->cpu_write(addr, value);
        return;
    }
    //Mapper
    if (addr >= 0x6000) {
        mapper->cpu_map_write(addr, value);
    }
}

uint8_t Bus::cpu_read(uint16_t addr) {
    static uint8_t open_bus = 0;
    //Ram
    if (addr < 0x2000) {
        open_bus = cpu_ram[addr & 0x7FF];
    }

    if (0x2000 <= addr && addr < 0x4000) {
        //PPU registers
        open_bus = ppu->cpu_read(addr);
    }

    if (addr == 0x4014) {
        open_bus = ppu->cpu_read(addr);
    }

    //Controller
    if (addr == 0x4016 || addr == 0x4017) {
        open_bus = controller_device->cpu_read(addr);
    } 
    
    //APU
    if (addr == 0x4015) {
        open_bus = apu->cpu_read(addr);
    }

    //ROM
    if (addr >= 0x6000) {
        open_bus = mapper->cpu_map_read(addr);
    }
    return open_bus;
}

void Bus::ppu_write(uint16_t addr, uint8_t value) {
    mapper->ppu_map_write(addr, value);
}

uint8_t Bus::ppu_read(uint16_t addr) {
    return mapper->ppu_map_read(addr);
}

int Bus::load_cart(const char *filename) {
    std::ifstream file;
    file.open(filename, std::ios::binary);
    printf("Open file: %s\n", filename);
    if (!file.is_open()) {
        return -1;
    }
    
    cart = load_nes_file(file);
    if (!cart) {
        return -1;
    }
    mapper = MapperFactory::create_mapper(cart);
    if (mapper->load_rom(file) == -1) {
        return -1;
    };
    file.close();
    return 0;
}
