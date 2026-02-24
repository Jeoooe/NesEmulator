/*
    对Nes手柄的实现
*/
#pragma once
#include <stdint.h>

// IO: $4016, $4017

//按键掩码
enum class KeyMask {
    A       = 0x1U,
    B       = 0x2U,
    Select  = 0x4U,
    Start   = 0x8U,
    Up      = 0x10U,
    Down    = 0x20U,
    Left    = 0x40U,
    Right   = 0x80U
};

class Controller {
public:
    enum ControllerIndex {
        Controller1,
        Controller2
    };
    uint8_t cpu_read(uint16_t addr);
    void    cpu_write(uint16_t addr, uint8_t value);
    void    key_down(ControllerIndex index, KeyMask key);
    void    key_up(ControllerIndex index, KeyMask key);
    
private:
    bool button_reset       = false;     //读取是否重置
    uint8_t button_index1    = 0;     //读取序列的索引
    uint8_t button_index2    = 0;     //读取序列的索引
    uint8_t controller1     = 0;
    uint8_t controller2     = 0;
};