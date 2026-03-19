#pragma once
#include <memory>
#include <vector>

class Cartridge {
public:
    // uint8_t *prg_rom;
    // uint8_t *chr_rom;
    std::vector<uint8_t> prg_rom;
    std::vector<uint8_t> chr_rom;

    size_t prg_rom_size;
    size_t chr_rom_size;
    size_t prg_ram_size;

    int mapper;     //映射类型
    int mirroring;  //镜像类型
    bool is_nes2;   //是否是nes2.0系统
    bool is_nrom128;    //是否是NROM128系统
    bool use_chr_ram;   //是否使用chr-ram
};

std::shared_ptr<Cartridge> load_nes_file(std::ifstream &file);
