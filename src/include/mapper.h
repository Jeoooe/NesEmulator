#pragma once

#include <stdint.h>
#include <memory>

class Cartridge;

//映射器基类
class Mapper {
    friend class MapperFactory;
public:    
    Mapper() = default;
    virtual ~Mapper() = default;
    virtual uint16_t map_read(uint16_t addr) = 0;
    virtual uint16_t map_write(uint16_t addr) = 0;
    virtual uint8_t cpu_map_read(uint16_t addr) = 0;
    virtual void    cpu_map_write(uint16_t addr, uint8_t value) = 0;

    //addr <= 0x2000
    virtual uint8_t ppu_map_read(uint16_t addr) = 0;
    virtual void ppu_map_write(uint16_t addr, uint8_t value) = 0;
protected:
    std::shared_ptr<Cartridge> cart;
};

class MapperFactory {
public:
    static std::shared_ptr<Mapper> create_mapper(const std::shared_ptr<Cartridge>& cart) noexcept;
};

