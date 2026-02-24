#include <controller.h>

uint8_t Controller::cpu_read(uint16_t addr) {
    uint8_t ret = 1;
    switch (addr) {
    case 0x4016:
        ret = (button_index1 < 8) ? ((controller1 >> button_index1) & 1) : 1;
        if (!button_reset)
            button_index1++;
        break;
    case 0x4017:
        ret = (button_index2 < 8) ? ((controller2 >> button_index2) & 1) : 1;
        if (!button_reset)
            button_index2++;
        break;
    default:
        break;
    }
    return ret;
}

void Controller::cpu_write(uint16_t addr, uint8_t value) {
    switch (addr) {
    case 0x4016:
        if (value & 1) {
            button_reset = true;
            button_index1 = 0;
            button_index2 = 0;
        } else {
            button_reset = false;
        }
        break;
    default:
        break;
    }
}

void Controller::key_down(ControllerIndex index, KeyMask key) {
    switch (index) {
    case Controller1:
        controller1 |= (uint8_t)key;
        break;  
    case Controller2:
        controller2 |= (uint8_t)key;
        break;  
    default:
        break;
    }
}

void Controller::key_up(ControllerIndex index, KeyMask key) {
    switch (index) {
    case Controller1:
        controller1 &= ~(uint8_t)key;
        break;  
    case Controller2:
        controller2 &= ~(uint8_t)key;
        break;  
    default:
        break;
    }
}
