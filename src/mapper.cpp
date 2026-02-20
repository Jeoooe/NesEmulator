#include <mapper.h>
#include <cpu.h>
#include <bus.h>
#include <cartridge.h>

class Mapper_000 : public Mapper {
private:
    bool is_nrom_128;
public:
    Mapper_000(bool nrom_128): is_nrom_128(nrom_128) {}
    virtual uint16_t map_read(uint16_t addr) override {
        if (addr >= 0x8000) {
            addr -= 0x8000;
            return is_nrom_128 ? (addr & ~0x4000) : addr;
        } else {
            return addr;
        }
    }
    virtual uint16_t map_write(uint16_t addr) override {
        return addr;
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
