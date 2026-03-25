#include "apu.h"

void APU::cpu_write(uint16_t addr, uint8_t value) {
    (void)addr;
    (void)value;
}

uint8_t APU::cpu_read(uint16_t addr) {
    (void)addr;
    return 0;
}