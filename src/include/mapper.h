#pragma once

#include <stdint.h>
#include <memory>

class Cartridge;

//映射器基类
class Mapper {
public:    
    virtual uint16_t map_read(uint16_t addr) = 0;
    virtual uint16_t map_write(uint16_t addr) = 0;
};

class MapperFactory {
public:
    static std::shared_ptr<Mapper> create_mapper(const std::shared_ptr<Cartridge>& cart);
};

