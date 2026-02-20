#include <ppu.h>
#include <bus.h>

uint8_t PPU::cpu_read(uint16_t addr) {
    if (addr >= 0x2000 && addr < 0x4000) {
        addr &= 0x7;
        switch (addr) {
            case 0: //2000
            
        }
        return reg[addr];
    }
    return 0;   //这里不应该到达
}

void PPU::cpu_write(uint16_t addr, uint8_t value) {
    if (addr >= 0x2000 && addr < 0x4000) {
        addr &= 0x7;
        reg[addr] = value;
    }
    //这里不应该到达
}
