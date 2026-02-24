#pragma once

#include <stdint.h>
#include <memory>
#include <types.h>

class Mapper;
class Bus;

//有点屎山, 这个应该是单例
class CPU {
public:
    void reset();       //CPU重置或启动
    void run1operation();   //运行一个周期
    void trigger_NMI();     //触发NMI中断

public:
    //Registers
    uint8_t a;   //累加器
    uint8_t x;   //索引x
    uint8_t y;   //索引y
    uint8_t s;   //堆栈
    uint16_t pc;  //程序计数器
    uint8_t p;   //状态寄存器
    uint8_t opcode = 0;                 //当前操作码
    uint8_t op_length = 0;              //当前指令长度, 取决于寻址模式
    bool is_branch = false;                 //当前是否进入分支
    bool is_NMI = false;                    //是否产生了NMI中断
    bool is_IRQ = false;                    //是否产生了IRQ/BRK中断
};

CPU& get_cpu();