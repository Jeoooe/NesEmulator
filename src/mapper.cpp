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

    virtual uint8_t ppu_map_read(uint16_t addr) override {
        auto &cart = Bus::get().get_cart();
        if (addr < cart->chr_rom.size()) {
            return cart->chr_rom[addr];
        }
        return 0;
    }
    virtual void ppu_map_write(uint16_t addr, uint8_t value) override {
        (void)addr;
        (void)value;
    }
};

std::shared_ptr<Mapper> MapperFactory::create_mapper(const std::shared_ptr<Cartridge> &cart) {
    switch (cart->mapper) {
        case 0:
            return std::make_shared<Mapper_000>(cart->is_nrom128);

        default:
            return std::shared_ptr<Mapper>();
    }
    return std::shared_ptr<Mapper>();
}
