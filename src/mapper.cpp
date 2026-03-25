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

class Mapper_001 : public Mapper {
    unsigned int prg_count_16kb = 0;
    unsigned int chr_count_8kb = 0;
    unsigned int shift_count = 0;
    bool prg_ram_enable = false;
    uint8_t *prg_rom[2] = {nullptr, nullptr};
    uint8_t *chr_rom[2] = {nullptr, nullptr};
    uint8_t control = 0xC;    //5bit内部寄存器
    uint8_t shift = 0;
    uint8_t prg_bank_mode = 0;
    uint8_t chr_bank_mode = 0;
    std::array<uint8_t, 0x2000> prg_ram;
public:
    Mapper_001() = default;
    ~Mapper_001() = default;
    virtual uint8_t cpu_map_read(uint16_t addr) override {
        if (addr < 0x8000) {
            if (!prg_ram_enable) return 0;
            return prg_ram[addr - 0x6000];
        }
        addr -= 0x8000;
        return prg_rom[(addr >> 14) & 1][addr & 0x3FFF];
    };
    virtual void cpu_map_write(uint16_t addr, uint8_t value) override {
        if (addr < 0x8000) {
            prg_ram[addr - 0x6000] = value;
            return;
        }
        if (value & 0x80) {
            constexpr NtArrangement mirrorings[4] = {
                NtArrangement::OneScreenLower, 
                NtArrangement::OneScreenUpper,
                NtArrangement::Horizontal, 
                NtArrangement::Vertical, 
            };
            cart->nt_arrangement = mirrorings[control & 0b11];
            //复位移位寄存器
            shift_count = 0;    
            control |= 0xC;   //PRG ROM bank mode = 3, 即固定最后一个bank, 其他bank可切换
            return;
        }
        shift_count++;
        shift = (shift >> 1) | ((value & 1) << 4);
        if (shift_count < 5) 
            return;
        shift_count = 0;
        switch (addr >> 13) {
        case 0b100:     //控制寄存器
            control = shift;
            prg_bank_mode = (control >> 2) & 0x3;
            chr_bank_mode = (control >> 4) & 0x1;
            break;
        case 0b101:     //CHR bank 0   
            if (chr_bank_mode == 0) {   //8kb模式
                chr_rom[0] = cart->chr_rom.data() + (shift & 0x1E)*4096;
                chr_rom[1] = cart->chr_rom.data() + ((shift & 0x1E) + 1)*4096;
            } else {
                chr_rom[0] = cart->chr_rom.data() + (shift & 0x1F)*4096;
            }
            break;
        case 0b110:     //CHR bank 1
            if (chr_bank_mode == 1) {   //4kb模式
                chr_rom[1] = cart->chr_rom.data() + (shift & 0x1F)*4096;
            }
            break;
        case 0b111:     //PRG bank
            switch (prg_bank_mode) {
            case 0:
            case 1: // 32kb模式 
                prg_rom[0] = cart->prg_rom.data() + (shift & 0x0E)*16384;
                prg_rom[1] = cart->prg_rom.data() + ((shift & 0x0E) + 1)*16384;
                break;
            case 2: // fix first bank at $8000, switch 16kb bank    
                prg_rom[0] = cart->prg_rom.data();
                prg_rom[1] = cart->prg_rom.data() + (shift & 0x0F)*16384;
                break;
            case 3: // fix last bank at $C000, switch 16kb bank
                prg_rom[0] = cart->prg_rom.data() + (shift & 0x0F)*16384;
                prg_rom[1] = cart->prg_rom.data() + cart->prg_rom_size - 16384;
                break;
            }
            prg_ram_enable = (shift & 0x10) == 0;
            break;
        }
        
    }

    virtual uint8_t ppu_map_read(uint16_t addr) override {
        return chr_rom[(addr >> 12) & 1][addr & 0x0FFF];
    }
    virtual void ppu_map_write(uint16_t addr, uint8_t value) override {
        chr_rom[(addr >> 12) & 1][addr & 0x0FFF] = value;
    }

    virtual int load_rom(std::ifstream &file) override {
        cart->prg_rom.resize(cart->prg_rom_size);
        cart->chr_rom.resize(cart->chr_rom_size);  
        file.read((char *)cart->prg_rom.data(), cart->prg_rom_size);
        file.read((char *)cart->chr_rom.data(), cart->chr_rom_size);
        prg_rom[0] = cart->prg_rom.data();
        prg_rom[1] = cart->prg_rom.data() + cart->prg_rom_size - 16384;
        chr_rom[0] = cart->chr_rom.data();
        chr_rom[1] = cart->chr_rom.data() + (cart->chr_rom_size - 8192);
        return 0;
    }

    virtual void load_cart(std::shared_ptr<Cartridge> _cart) override {
        cart = _cart;
    };
};

class Mapper_002 : public Mapper {
    uint32_t prg_count_16kb = 0;
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
        value &= 0xF;
        prg_rom[0] = cart->prg_rom.data() + (value%prg_count_16kb)*16384;
    }

    virtual uint8_t ppu_map_read(uint16_t addr) override {
        return cart->chr_rom[addr];
    }
    virtual void ppu_map_write(uint16_t addr, uint8_t value) override {
        cart->chr_rom[addr] = value;
    }

    virtual int load_rom(std::ifstream &file) override {
        prg_count_16kb = cart->prg_rom_size/16384;
        cart->prg_rom.resize(cart->prg_rom_size);
        cart->chr_rom.resize(cart->chr_rom_size);  
        file.read((char *)cart->prg_rom.data(), cart->prg_rom_size);
        prg_rom[0] = cart->prg_rom.data();
        prg_rom[1] = cart->prg_rom.data() + cart->prg_rom_size - 16384;
        return 0;
    }
};

class Mapper_003 : public Mapper {
    size_t chr_count_8kb = 0;
    bool is_16kb_rom = false;
    uint8_t *chr_rom = nullptr;
    std::array<uint8_t,2048> prg_ram;
public:
    Mapper_003() = default;
    ~Mapper_003() = default;
    virtual void load_cart(std::shared_ptr<Cartridge> _cart) override {
        cart = _cart;
    };

    //0x8000以上
    virtual uint8_t cpu_map_read(uint16_t addr) override {
        if (addr < 0x8000) {
            return prg_ram[(addr - 0x6000) & 0x7FF];
        }
        addr -= 0x8000;
        return cart->prg_rom[is_16kb_rom ?  (addr & ~0x4000) : addr];
    };
    virtual void cpu_map_write(uint16_t addr, uint8_t value) override {
        if (addr < 0x8000) {
            prg_ram[(addr - 0x6000) & 0x7FF] = value;
            return;
        }
        chr_rom = cart->chr_rom.data() + static_cast<uint16_t>(value%chr_count_8kb)*8192;
    }

    virtual uint8_t ppu_map_read(uint16_t addr) override {
        return chr_rom[addr];
    }
    virtual void ppu_map_write(uint16_t addr, uint8_t value) override {
        chr_rom[addr] = value;
    }

    virtual int load_rom(std::ifstream &file) override {
        chr_count_8kb = cart->chr_rom_size/8192;
        is_16kb_rom = cart->prg_rom_size == 16384;
        cart->prg_rom.resize(cart->prg_rom_size);
        cart->chr_rom.resize(cart->chr_rom_size);  
        file.read((char *)cart->prg_rom.data(), cart->prg_rom_size);
        file.read((char *)cart->chr_rom.data(), cart->chr_rom_size);
        return 0;
    }
};

std::shared_ptr<Mapper> MapperFactory::create_mapper(const std::shared_ptr<Cartridge> &cart) noexcept {
    std::shared_ptr<Mapper> mapper = nullptr;
    switch (cart->mapper) {
        case 0:
            mapper = std::make_shared<Mapper_000>(cart->is_nrom128);
            break;
        case 1:
            mapper = std::make_shared<Mapper_001>();
            break;
        case 2:
            mapper = std::make_shared<Mapper_002>();
            break;
        case 3:
            mapper = std::make_shared<Mapper_003>();
            break;
        default: 
            break;
    }
    mapper->load_cart(cart);
    return mapper;
}
