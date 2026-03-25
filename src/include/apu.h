#pragma once

#include <stdint.h>

class APU {
public:
    APU() = default;
    ~APU() = default;

    void cpu_write(uint16_t addr, uint8_t value);
    uint8_t cpu_read(uint16_t addr);
};