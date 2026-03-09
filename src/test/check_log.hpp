#include <string>
#include <vector>
#include <cstdint>

#include <bus.h>

class PPU;
class Cartridge;
class Disassembler {
public:
    Disassembler(): ppu(Bus::get().get_PPU()), cart(Bus::get().get_cart()) {}
    ~Disassembler() = default;

    void list_lines() {
        
    }

    void step() {

    }

    std::shared_ptr<PPU> ppu;
    std::shared_ptr<Cartridge> cart;
    bool enabled = false;
};