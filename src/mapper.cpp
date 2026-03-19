#include <mapper.h>

#include <fstream>

#include <cpu.h>
#include <bus.h>
#include <cartridge.h>
#include <log.h>

class Mapper_000 : public Mapper {
private:
    bool is_nrom_128;
public:
    Mapper_000(bool nrom_128): is_nrom_128(nrom_128) {}
    ~Mapper_000() = default;

    //0x8000以上
    virtual uint8_t cpu_map_read(uint16_t addr) override {
        addr -= 0x8000;
        return cart->prg_rom[is_nrom_128 ? (addr & ~0x4000) : addr];
    };
    virtual void cpu_map_write(uint16_t addr, uint8_t value) override {
        (void)addr;
        (void)value;
    }

    virtual uint8_t ppu_map_read(uint16_t addr) override {
        return cart->chr_rom[addr];
    }
    virtual void ppu_map_write(uint16_t addr, uint8_t value) override {
        cart->chr_rom[addr] = value;
    }

    virtual int load_rom(std::ifstream &file) override {
        cart->prg_rom.resize(cart->prg_rom_size);
        cart->chr_rom.resize(cart->chr_rom_size);  
        file.read((char *)cart->prg_rom.data(), cart->prg_rom_size);
        file.read((char *)cart->chr_rom.data(), cart->chr_rom_size);
        return 0;
    }

    virtual void load_cart(std::shared_ptr<Cartridge> _cart) override {
        cart = _cart;
    };
};

class Mapper_002 : public Mapper {
    size_t prg_rom_size = 0;
    uint8_t *prg_rom[2] = {nullptr, nullptr};
    uint8_t bank_select = 0;
public:
    Mapper_002() = default;
    ~Mapper_002() = default;

    virtual void load_cart(std::shared_ptr<Cartridge> _cart) override {
        cart = _cart;
    };

    //0x8000以上
    virtual uint8_t cpu_map_read(uint16_t addr) override {
        addr -= 0x8000;
        return prg_rom[(addr >> 14) & 1][addr & 0x3FFF];
    };
    virtual void cpu_map_write(uint16_t addr, uint8_t value) override {
        if (addr < 0xC000) return;
        prg_rom[0] = cart->prg_rom.data() + (ptrdiff_t)value*16384;
    }

    virtual uint8_t ppu_map_read(uint16_t addr) override {
        return cart->chr_rom[addr];
    }
    virtual void ppu_map_write(uint16_t addr, uint8_t value) override {
        cart->chr_rom[addr] = value;
    }

    virtual int load_rom(std::ifstream &file) override {
        prg_rom_size = cart->prg_rom_size/16384;
        cart->prg_rom.resize(cart->prg_rom_size);
        cart->chr_rom.resize(cart->chr_rom_size);  
        file.read((char *)cart->prg_rom.data(), cart->prg_rom_size);
        prg_rom[0] = cart->prg_rom.data();
        prg_rom[1] = cart->prg_rom.data() + cart->prg_rom_size - 16384;
        return 0;
    }
};

std::shared_ptr<Mapper> MapperFactory::create_mapper(const std::shared_ptr<Cartridge> &cart) noexcept {
    std::shared_ptr<Mapper> mapper = nullptr;
    switch (cart->mapper) {
        case 0:
            mapper = std::make_shared<Mapper_000>(cart->is_nrom128);
            break;
        case 2:
            mapper = std::make_shared<Mapper_002>();
            break;
        default: 
            break;
    }
    mapper->load_cart(cart);
    return mapper;
}
