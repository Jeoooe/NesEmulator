#include <mapper.h>
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
    virtual uint16_t map_read(uint16_t addr) override {
        if (addr >= 0x8000) {
            //ROM部分
            addr -= 0x8000;
            return is_nrom_128 ? (addr & ~0x4000) : addr;
        } else {
            return addr;
        }
    }
    virtual uint16_t map_write(uint16_t addr) override {
        return addr;
    }

    //0x8000以上
    virtual uint8_t cpu_map_read(uint16_t addr) override {
        addr -= 0x8000;
        return cart->prg_rom[is_nrom_128 ? (addr & ~0x4000) : addr];
    };
    virtual void cpu_map_write(uint16_t addr, uint8_t value) override {
        addr -= 0x8000;
        cart->prg_rom[is_nrom_128 ? (addr & ~0x4000) : addr] = value;
    }

    virtual uint8_t ppu_map_read(uint16_t addr) override {
        return cart->chr_rom[addr];
    }
    virtual void ppu_map_write(uint16_t addr, uint8_t value) override {
        cart->chr_rom[addr] = value;
    }
};

std::shared_ptr<Mapper> MapperFactory::create_mapper(const std::shared_ptr<Cartridge> &cart) noexcept {
    std::shared_ptr<Mapper> mapper = nullptr;
    switch (cart->mapper) {
        case 0:
            mapper = std::make_shared<Mapper_000>(cart->is_nrom128);

        default: 
        break;
    }
    mapper->cart = cart;
    return mapper;
}
