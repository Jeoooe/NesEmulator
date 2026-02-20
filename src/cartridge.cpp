#include <cartridge.h>
#include <stdint.h>
#include <fstream>
#include <cstring>
#include <memory>
#include <vector>
#include <cassert>

using std::shared_ptr;
using std::unique_ptr;
using std::make_shared;
using std::make_unique;
using std::vector;

struct Nes_header {
    char magic[4];
    uint8_t PRG_rom;    //16kb单位
    uint8_t CHR_rom;    //8kb单位
    uint8_t flag6;
    uint8_t flag7;
    uint8_t PRG_ram;    //8kb单位
    uint8_t flag9;     //0
    uint8_t flag10;     //0
    uint8_t reserved[5];
};

int get_mirroring(Nes_header *header) {
    return (header->flag6 & 0b11);
}


shared_ptr<Cartridge> load_nes_file(const char *filename) {
    std::ifstream file;
    file.open(filename, std::ios::in);
    if (!file.is_open()) {
        //读取错误
        fputs("Error file", stderr);
        exit(1);
    }
    Nes_header header;
    file.read((char *)&header, sizeof(Nes_header));
    //判断文件头magic
    if (strncmp(header.magic, "NES\x1A", 4) != 0) {
        fputs("It is not a nes rom", stderr);
        exit(1);
    }

    //正确nes rom文件

    shared_ptr cart = make_shared<Cartridge>();
    cart->prg_rom_size = header.PRG_rom * 16384;
    cart->chr_rom_size = header.CHR_rom * 8192;
    cart->prg_ram_size = header.PRG_ram ? header.PRG_ram * 8192 : 8192;

    cart->is_nes2 = (header.flag7 & 0b1100) == 0b1000;
    cart->mapper = ((header.flag6 & 0xF0) >> 4) | (header.flag7 & 0xF0);
    cart->mirroring = get_mirroring(&header);
    cart->is_nrom128 = cart->prg_rom_size == 16384;

    assert(cart->prg_rom_size);
    assert(cart->chr_rom_size);

    cart->prg_rom.resize(cart->prg_rom_size);
    cart->chr_rom.resize(cart->chr_rom_size);

    //读取游戏文件
    
    file.read((char *)cart->prg_rom.data(), cart->prg_rom_size);
    file.read((char *)cart->chr_rom.data(), cart->chr_rom_size);
    file.close();
    return cart;
}
