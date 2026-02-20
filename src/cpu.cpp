#include <cpu.h>
#include <stdint.h>
#include <string>
#include <bus.h>
#include <types.h>
#include <log.h>

#ifdef DEBUG_V1
//Debug
#include <cassert>
#include "test/check_log.hpp"
#endif

//NES十进制模式, 即禁止十进制模式
#define NES_DECIMAL_MODE 

//标志位
#define N (1<<7)
#define V (1<<6)
#define B (1<<4)
#define D (1<<3)
#define I (1<<2)
#define Z (1<<1)
#define C (1   )

#define SET_FLAG(flag, condition) if (condition) cpu.p |= flag; else cpu.p &= ~flag
#define GET_FLAG(flag) (cpu.p & flag)

#define IS_NEG(u8_val) (u8_val & 0x80)

#define PUSH(value)  \
cpu.bus->cpu_write((uint16_t)cpu.s | 0x100, value); \
--cpu.s
#define POP() cpu.bus->cpu_read((uint16_t)(++cpu.s) | 0x100)

using std::shared_ptr;

typedef void (*InstructionFunc)(void);     //指令对应函数指针
typedef uint16_t (*AddrMode)(void);         //寻址方式, 返回地址

constexpr int code_count = 256;

struct Instruction {
    InstructionFunc func;
    AddrMode addr_mode;
    cycle_t cycle;
};

extern Instruction instructions[]; //指令集

CPU cpu;

CPU& get_cpu() {
    return cpu;
}

void CPU::reset() {
    uint8_t reset_low = bus->cpu_read(0xFFFC);
    uint8_t reset_high = bus->cpu_read(0xFFFD);
    uint16_t reset_vector = ((uint16_t)reset_high << 8) | reset_low;
    pc = reset_vector;
    p = I | 0x20;
    s = 0xFD;
    next_clock_cycle = 7;
    cycle_counter = 0;
    opcode = 0;
    is_branch = false;
    op_length = 0;
    is_NMI = false;
    is_IRQ = false;
}

void opcode_log() {
    auto tmp = cpu.op_length;
    auto cycle = cpu.next_clock_cycle;
    (void)instructions[cpu.opcode].addr_mode();
    switch (cpu.op_length) {
    case 1:
    LOG("%04X %02X       -- A:%02X X:%02X Y:%02X P:%02X SP:%02X   CYC:%d\n", cpu.pc, 
        cpu.bus->cpu_read(cpu.pc),
        cpu.a, cpu.x, cpu.y, cpu.p, cpu.s, cpu.cycle_counter
    );
    break;
    case 2:
    LOG("%04X %02X %02X    -- A:%02X X:%02X Y:%02X P:%02X SP:%02X   CYC:%d\n", cpu.pc, 
        cpu.bus->cpu_read(cpu.pc),
        cpu.bus->cpu_read(cpu.pc + 1),
        cpu.a, cpu.x, cpu.y, cpu.p, cpu.s, cpu.cycle_counter
    );
    break;
    case 3:
    LOG("%04X %02X %02X %02X -- A:%02X X:%02X Y:%02X P:%02X SP:%02X   CYC:%d\n", cpu.pc, 
        cpu.bus->cpu_read(cpu.pc),
        cpu.bus->cpu_read(cpu.pc + 1),
        cpu.bus->cpu_read(cpu.pc + 2),
        cpu.a, cpu.x, cpu.y, cpu.p, cpu.s, cpu.cycle_counter
    );
    break;
    default:
    LOG("%04X %02X %02X %02X -- A:%02X X:%02X Y:%02X P:%02X SP:%02X   CYC:%d\n", cpu.pc, 
        cpu.bus->cpu_read(cpu.pc),
        cpu.bus->cpu_read(cpu.pc + 1),
        cpu.bus->cpu_read(cpu.pc + 2),
        cpu.a, cpu.x, cpu.y, cpu.p, cpu.s, cpu.cycle_counter
    );
    }
    cpu.op_length = tmp;
    cpu.next_clock_cycle = cycle;
}


void CPU::run1cycle() {
    #ifdef DEBUG_V1
    static auto logger = std::make_unique<CPULogChecker>();
    if (!logger->is_loadded()) {
        if (!logger->loadLogFile("nestest.log.txt")) {
            throw std::runtime_error("No file");
        }
    }
    #endif

    if (cycle_counter >= next_clock_cycle ) {
        if (pc == 0xFFFC) { //复位中断
            cpu.reset();
            goto cycle_end;
        }
        if (is_NMI) {
            //如果有NMI中断来
            uint16_t low = bus->cpu_read(0xFFFA);
            uint16_t high = (u_int16_t)bus->cpu_read(0xFFFB) << 8;
            PUSH(pc);
            PUSH(p);
            pc = low | high;
            is_NMI = false;
            next_clock_cycle += 7;
            goto cycle_end;
        } 
        if (is_IRQ) {  //这里应该有个IRQ屏蔽位?
            //如果有NMI中断来
            uint16_t low = bus->cpu_read(0xFFFA);
            uint16_t high = (u_int16_t)bus->cpu_read(0xFFFB) << 8;
            PUSH(pc);
            PUSH(p);
            pc = low | high;
            is_IRQ = false;
            next_clock_cycle += 7;
            goto cycle_end;
        }
        opcode = bus->cpu_read(pc);

        #ifdef DEBUG_V1
        opcode_log();
        bool correct = logger->check(cpu.pc, cpu.cycle_counter);
        #endif
        
        instructions[opcode].func();
        //进行周期计算
        next_clock_cycle += instructions[opcode].cycle;
        //判断是否有分支
        if (!is_branch) {
            pc += op_length;
        }
        is_branch = false;
    }
cycle_end:
    cycle_counter++;
}

void CPU::set_bus(std::shared_ptr<Bus> bus) {
    this->bus = std::move(bus);
}

void CPU::trigger_NMI() {
    this->is_NMI = true;
}

/*
    下面是指令集
*/

/* 读取, 存储指令 */
/* 初次设计, 打算在进入指令函数之前先修改pc, 即pc指向当前指令 */

uint16_t acc();

void LDA() {
    uint8_t value = cpu.bus->cpu_read(instructions[cpu.opcode].addr_mode());
    SET_FLAG(N, IS_NEG(value));
    SET_FLAG(Z, value == 0);
    cpu.a = value;
}

void LDX() {
    uint8_t value = cpu.bus->cpu_read(instructions[cpu.opcode].addr_mode());
    SET_FLAG(N, IS_NEG(value));
    SET_FLAG(Z, value == 0);
    cpu.x = value;
}

void LDY() {
    uint8_t value = cpu.bus->cpu_read(instructions[cpu.opcode].addr_mode());
    SET_FLAG(N, IS_NEG(value));
    SET_FLAG(Z, value == 0);
    cpu.y = value;
}

void STA() {
    auto cycle = cpu.next_clock_cycle;
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    cpu.next_clock_cycle = cycle;
    cpu.bus->cpu_write(addr, cpu.a);
}

void STX() {
    auto cycle = cpu.next_clock_cycle;
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    cpu.next_clock_cycle = cycle;
    cpu.bus->cpu_write(addr, cpu.x);
}

void STY() {
    auto cycle = cpu.next_clock_cycle;
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    cpu.next_clock_cycle = cycle;
    cpu.bus->cpu_write(addr, cpu.y);
}

void STZ() {
    auto cycle = cpu.next_clock_cycle;
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    cpu.next_clock_cycle = cycle;
    cpu.bus->cpu_write(addr, 0);
}

void PHA() {
    cpu.op_length = 1;
    PUSH(cpu.a);
}

void PHX() {
    cpu.op_length = 1;
    PUSH(cpu.x);
}

void PHY() {
    cpu.op_length = 1;
    PUSH(cpu.y);
}

void PHP() {
    cpu.op_length = 1;
    PUSH(cpu.p | B);
}

void PLA() {
    cpu.op_length = 1;
    cpu.a = POP();
    SET_FLAG(N, IS_NEG(cpu.a));
    SET_FLAG(Z, cpu.a == 0);
}

void PLX() {
    cpu.op_length = 1;
    cpu.x = POP();
    SET_FLAG(N, IS_NEG(cpu.x));
    SET_FLAG(Z, cpu.x == 0);
}

void PLY() {
    cpu.op_length = 1;
    cpu.y = POP();
    SET_FLAG(N, IS_NEG(cpu.y));
    SET_FLAG(Z, cpu.y == 0);
}

void PLP() {
    cpu.op_length = 1;
    uint8_t b = cpu.p & B;
    cpu.p = POP();
    //忽略保留位和B标志
    cpu.p |= 0x20;
    if (b) {
        cpu.p |= B;
    } else {
        cpu.p &= ~B;
    }
}

void TSX() {
    cpu.op_length = 1;
    cpu.x = cpu.s;
    SET_FLAG(N, IS_NEG(cpu.x));
    SET_FLAG(Z, cpu.x == 0);
}

void TXS() {
    cpu.op_length = 1;
    cpu.s = cpu.x;
}

void INA() {
    cpu.op_length = 1;
    cpu.a++;
    SET_FLAG(N, IS_NEG(cpu.a));
    SET_FLAG(Z, cpu.a == 0);
}

void INX() {
    cpu.op_length = 1;
    cpu.x++;
    SET_FLAG(N, IS_NEG(cpu.x));
    SET_FLAG(Z, cpu.x == 0);
}

void INY() {
    cpu.op_length = 1;
    cpu.y++;
    SET_FLAG(N, IS_NEG(cpu.y));
    SET_FLAG(Z, cpu.y == 0);
}

void DEA() {
    cpu.op_length = 1;
    cpu.a--;
    SET_FLAG(N, IS_NEG(cpu.a));
    SET_FLAG(Z, cpu.a == 0);
}

void DEX() {
    cpu.op_length = 1;
    cpu.x--;
    SET_FLAG(N, IS_NEG(cpu.x));
    SET_FLAG(Z, cpu.x == 0);
}

void DEY() {
    cpu.op_length = 1;
    cpu.y--;
    SET_FLAG(N, IS_NEG(cpu.y));
    SET_FLAG(Z, cpu.y == 0);
}

void INC() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value = cpu.bus->cpu_read(addr) + 1;
    SET_FLAG(N, IS_NEG(value));
    SET_FLAG(Z, value == 0);
    cpu.bus->cpu_write(addr, value); 
}

void DEC() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value = cpu.bus->cpu_read(addr) - 1;
    SET_FLAG(N, IS_NEG(value));
    SET_FLAG(Z, value == 0);
    cpu.bus->cpu_write(addr, value); 
}

void ASL() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value;
    bool is_acc = instructions[cpu.opcode].addr_mode == acc;
    if (is_acc) value = cpu.a;
    else value = cpu.bus->cpu_read(addr);
    SET_FLAG(C, value >> 7);
    value <<= 1;
    SET_FLAG(N, IS_NEG(value));
    SET_FLAG(Z, value == 0);
    if (is_acc) cpu.a = value;
    else cpu.bus->cpu_write(addr, value);
}

void LSR() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value;
    bool is_acc = instructions[cpu.opcode].addr_mode == acc;
    if (is_acc) value = cpu.a;
    else value = cpu.bus->cpu_read(addr);
    SET_FLAG(C, value & 1);
    value >>= 1;
    SET_FLAG(N, IS_NEG(value));
    SET_FLAG(Z, value == 0);
    if (is_acc) cpu.a = value;
    else cpu.bus->cpu_write(addr, value);
}

void ROL() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value;
    bool is_acc = cpu.op_length == 1;
    if (is_acc) value = cpu.a;
    else value = cpu.bus->cpu_read(addr);
    uint8_t c = GET_FLAG(C);
    SET_FLAG(C, value >> 7);
    value <<= 1;
    value |= c;
    SET_FLAG(N, IS_NEG(value));
    SET_FLAG(Z, value == 0);
    if (is_acc) cpu.a = value;
    else cpu.bus->cpu_write(addr, value);
}

void ROR() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value;
    bool is_acc = cpu.op_length == 1;
    if (is_acc) value = cpu.a;  //累加器寻址
    else value = cpu.bus->cpu_read(addr);
    uint8_t c = GET_FLAG(C);
    SET_FLAG(C, value & 1);
    value >>= 1;
    value |= c << 7;
    SET_FLAG(N, IS_NEG(value));
    SET_FLAG(Z, value == 0);
    if (is_acc) cpu.a = value;
    else cpu.bus->cpu_write(addr, value);
}

void AND() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value = cpu.bus->cpu_read(addr);
    cpu.a = cpu.a & value;
    SET_FLAG(N, IS_NEG(cpu.a));
    SET_FLAG(Z, cpu.a == 0);
}

void ORA() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value = cpu.bus->cpu_read(addr);
    cpu.a = cpu.a | value;
    SET_FLAG(N, IS_NEG(cpu.a));
    SET_FLAG(Z, cpu.a == 0);
}

void EOR() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value = cpu.bus->cpu_read(addr);
    cpu.a = cpu.a ^ value;
    SET_FLAG(N, IS_NEG(cpu.a));
    SET_FLAG(Z, cpu.a == 0);
}

void BIT() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value = cpu.bus->cpu_read(addr);
    SET_FLAG(Z, (cpu.a & value) == 0);
    SET_FLAG(N, value & (1<<7));
    SET_FLAG(V, value & (1<<6));
}

void CMP() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value = cpu.bus->cpu_read(addr);
    SET_FLAG(C, cpu.a >= value);
    value = cpu.a - value;
    SET_FLAG(Z, value == 0);
    SET_FLAG(N, IS_NEG(value));
}

void CPX() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value = cpu.bus->cpu_read(addr);
    SET_FLAG(C, cpu.x >= value);
    value = cpu.x - value;
    SET_FLAG(Z, value == 0);
    SET_FLAG(N, IS_NEG(value));
}

void CPY() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t value = cpu.bus->cpu_read(addr);
    SET_FLAG(C, cpu.y >= value);
    value = cpu.y - value;
    SET_FLAG(Z, value == 0);
    SET_FLAG(N, IS_NEG(value));
}

void TRB() {

}

void TSB() {

}

void RMB() {
}

void SMB() {
}


void ADC() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t m = cpu.bus->cpu_read(addr);
    #ifndef NES_DECIMAL_MODE
    if (!GET_FLAG(D)) {
        //普通计算
        uint8_t c = (cpu.p & C) ? 1 : 0;
        uint16_t result = (uint16_t)cpu.a + (uint16_t)m + (uint16_t)c;
        SET_FLAG(C, result > 0xFF);
        SET_FLAG(V, (~(cpu.a ^ m)) & (cpu.a ^ (uint8_t)result) & 0x80);
        cpu.a = (uint8_t)result;
        SET_FLAG(Z, cpu.a == 0);
        SET_FLAG(N, IS_NEG(cpu.a));
    } else {
        //十进制计算
        uint8_t c = (cpu.p & C) ? 1 : 0;
        uint8_t la = cpu.a & 0x0F, lm = m & 0x0F;
        uint8_t result = la + lm + c;
        if (result > 9) {
            result += 6;
            c = 1;
        } else {
            c = 0;
        }
        la = cpu.a >> 4, lm = m >> 4;
        uint8_t result_high = la + lm + c;
        if (result_high > 9) {
            result_high += 6;
            SET_FLAG(C, 1);
        } else {
            SET_FLAG(C, 0);
        }
        result |= result_high << 4;
        cpu.a = result;
        SET_FLAG(Z, cpu.a == 0);
        SET_FLAG(N, IS_NEG(cpu.a));
        SET_FLAG(V, 0);
    }
    #else
    uint8_t c = (cpu.p & C) ? 1 : 0;
    uint16_t result = (uint16_t)cpu.a + (uint16_t)m + (uint16_t)c;
    SET_FLAG(C, result > 0xFF);
    SET_FLAG(V, (~(cpu.a ^ m)) & (cpu.a ^ (uint8_t)result) & 0x80);
    cpu.a = (uint8_t)result;
    SET_FLAG(Z, cpu.a == 0);
    SET_FLAG(N, IS_NEG(cpu.a));
    #endif
}

void SBC() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint8_t m = cpu.bus->cpu_read(addr);
    #ifndef NES_DECIMAL_MODE
    if (!GET_FLAG(D)) {
        //普通模式
        uint8_t c = (cpu.p & C) ? 0 : 1;    //这里是要取反的
        uint16_t result = (uint16_t)cpu.a - (uint16_t)m - (uint16_t)c;
        SET_FLAG(C, cpu.a >= m - c);
        SET_FLAG(V, (cpu.a ^ m) & (cpu.a ^ (uint8_t)result) & 0x80);
        cpu.a = (uint8_t)result;
        SET_FLAG(Z, cpu.a == 0);
        SET_FLAG(N, IS_NEG(cpu.a));
    } else {
        //十进制计算
        uint8_t c = (cpu.p & C) ? 0 : 1;
        uint8_t result = cpu.a - m - c;
        uint8_t result_low = result & 0x0F;
        uint8_t result_high = result >> 4;
        if (result_low > 9) {
            result_low -= 6;
        } 
        if (result_high > 9) {
            result_high -= 6;
            SET_FLAG(C, 0);
        } else {
            SET_FLAG(C, 1);
        }
        result = (result_high << 4) | result_low;
        cpu.a = result;
        SET_FLAG(Z, cpu.a == 0);
        SET_FLAG(N, IS_NEG(cpu.a));
        SET_FLAG(V, 0);
    }
    #else
    uint8_t c = (cpu.p & C) ? 0 : 1;    //这里是要取反的
    uint16_t result = (uint16_t)cpu.a - (uint16_t)m - (uint16_t)c;
    SET_FLAG(C, cpu.a >= m - c);
    SET_FLAG(V, (cpu.a ^ m) & (cpu.a ^ (uint8_t)result) & 0x80);
    cpu.a = (uint8_t)result;
    SET_FLAG(Z, cpu.a == 0);
    SET_FLAG(N, IS_NEG(cpu.a));
    #endif
}

void JMP() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    cpu.pc = addr;
    cpu.is_branch = true;
}

void JSR() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    uint16_t value = cpu.pc + 2;
    PUSH((uint8_t)(value >> 8));
    PUSH((uint8_t)(value & 0xFF));
    cpu.pc = addr;
    cpu.is_branch = true;
}

void RTS() {
    cpu.op_length = 1;
    uint16_t low = POP();
    uint16_t high = POP();
    cpu.pc = low | (high << 8);
    cpu.pc += 1;    //跳到JSR的下一条指令
    cpu.is_branch = true;
}

void RTI() {
    cpu.op_length = 2;
    uint8_t b = cpu.p & B;
    cpu.p = POP() | 0x20;
    if (b) cpu.p |= B; 
    else cpu.p &= ~B;
    uint16_t low = (uint16_t)POP();
    uint16_t high = (uint16_t)POP();
    cpu.pc = (high << 8) | low;
    cpu.is_branch = true;
}

void BRA() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    cpu.pc = cpu.pc + 2 + addr;
    cpu.is_branch = true;
}

void BEQ() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    if (GET_FLAG(Z)) {
        cpu.is_branch = true;
        cpu.next_clock_cycle++;
        cpu.pc += 2;
        addr = addr + cpu.pc;
        //判断是否跨页
        if ((addr & 0xFF00) != (cpu.pc & 0xFF00)) cpu.next_clock_cycle++;
        cpu.pc = addr; 
    }
}

void BNE() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    if (!GET_FLAG(Z)) {
        cpu.is_branch = true;
        cpu.next_clock_cycle++;
        cpu.pc += 2;
        addr = addr + cpu.pc;
        //判断是否跨页
        if ((addr & 0xFF00) != (cpu.pc & 0xFF00)) cpu.next_clock_cycle++;
        cpu.pc = addr;
    }
}

void BCC() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    if (!GET_FLAG(C)) {
        cpu.is_branch = true;
        cpu.next_clock_cycle++;
        cpu.pc += 2;
        addr = addr + cpu.pc;
        //判断是否跨页
        if ((addr & 0xFF00) != (cpu.pc & 0xFF00)) cpu.next_clock_cycle++;
        cpu.pc = addr;
    }
}

void BCS() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    if (GET_FLAG(C)) {
        cpu.is_branch = true;
        cpu.next_clock_cycle++;   
        cpu.pc += 2;
        addr = addr + cpu.pc;
        //判断是否跨页
        if ((addr & 0xFF00) != (cpu.pc & 0xFF00)) cpu.next_clock_cycle++;
        cpu.pc = addr;
    }
}

void BVC() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    if (!GET_FLAG(V)) {
        cpu.is_branch = true;
        cpu.next_clock_cycle++;
        cpu.pc += 2;
        addr = addr + cpu.pc;
        //判断是否跨页
        if ((addr & 0xFF00) != (cpu.pc & 0xFF00)) cpu.next_clock_cycle++;
        cpu.pc = addr;
    }
}

void BVS() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    if (GET_FLAG(V)) {
        cpu.is_branch = true;
        cpu.next_clock_cycle++;
        cpu.pc += 2;
        addr = addr + cpu.pc;
        //判断是否跨页
        if ((addr & 0xFF00) != (cpu.pc & 0xFF00)) cpu.next_clock_cycle++;
        cpu.pc = addr;
    }
}

void BMI() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    if (GET_FLAG(N)) {
        cpu.is_branch = true;
        cpu.next_clock_cycle++;
        cpu.pc += 2;
        addr = addr + cpu.pc;
        //判断是否跨页
        if ((addr & 0xFF00) != (cpu.pc & 0xFF00)) cpu.next_clock_cycle++;
        cpu.pc = addr;
    }
}

void BPL() {
    uint16_t addr = instructions[cpu.opcode].addr_mode();
    if (!GET_FLAG(N)) {
        cpu.is_branch = true;
        cpu.next_clock_cycle++;
        cpu.pc += 2;
        addr = addr + cpu.pc;
        //判断是否跨页
        if ((addr & 0xFF00) != (cpu.pc & 0xFF00)) cpu.next_clock_cycle++;
        cpu.pc = addr;
    }
}

void BBR() {
}
void BBS() {
}

void CLC() {
    cpu.op_length = 1;
    SET_FLAG(C, 0);
}

void CLD() {
    cpu.op_length = 1;
    SET_FLAG(D, 0);
}

void CLI() {
    cpu.op_length = 1;
    SET_FLAG(I, 0);
}

void CLV() {
    cpu.op_length = 1;
    SET_FLAG(V, 0);
}

void SEC() {
    cpu.op_length = 1;
    SET_FLAG(C, 1);
}

void SED() {
    cpu.op_length = 1;
    SET_FLAG(D, 1);
}

void SEI() {
    cpu.op_length = 1;
    SET_FLAG(I, 1);
}

void TAX() {
    cpu.op_length = 1;
    cpu.x = cpu.a;
    SET_FLAG(N, IS_NEG(cpu.x));
    SET_FLAG(Z, cpu.x == 0);
}

void TAY() {
    cpu.op_length = 1;
    cpu.y = cpu.a;
    SET_FLAG(N, IS_NEG(cpu.y));
    SET_FLAG(Z, cpu.y == 0);
}

void TXA() {
    cpu.op_length = 1;
    cpu.a = cpu.x;
    SET_FLAG(N, IS_NEG(cpu.a));
    SET_FLAG(Z, cpu.a == 0);
}

void TYA() {
    cpu.op_length = 1;
    cpu.a = cpu.y;
    SET_FLAG(N, IS_NEG(cpu.a));
    SET_FLAG(Z, cpu.a == 0);
}

void NOP() {
    (void)instructions[cpu.opcode].addr_mode();
}

void BRK() {
    cpu.op_length = 1;
    cpu.s = cpu.pc;
    cpu.pc = 0xFFFE;
    SET_FLAG(B, 1);
}

void XXX() {
    //这里应该打印错误信息, 或者日志
    // throw std::runtime_error("Unknown Error");
}

//下面是寻址模式

uint16_t acc() {
    cpu.op_length = 1;
    return 0;
}

uint16_t imp() {
    cpu.op_length = 1;
    return 0;
}

uint16_t imm() {
    cpu.op_length = 2;
    return cpu.pc + 1;
}

uint16_t zp() {
    cpu.op_length = 2;
    return (uint16_t)cpu.bus->cpu_read(cpu.pc + 1);
}

uint16_t zpx() {
    cpu.op_length = 2;
    uint8_t addr = cpu.bus->cpu_read(cpu.pc + 1) + cpu.x;
    return (uint16_t)addr;
}

uint16_t zpy() {
    cpu.op_length = 2;
    uint8_t addr = cpu.bus->cpu_read(cpu.pc + 1) + cpu.y;
    return (uint16_t)addr;
}

uint16_t abs() {
    cpu.op_length = 3;
    return (uint16_t)cpu.bus->cpu_read(cpu.pc + 1) | ((uint16_t)cpu.bus->cpu_read(cpu.pc + 2) << 8);
}

uint16_t abx() {
    cpu.op_length = 3;
    uint16_t l = (uint16_t)cpu.bus->cpu_read(cpu.pc + 1);
    uint16_t h = (uint16_t)cpu.bus->cpu_read(cpu.pc + 2) << 8;
    uint16_t addr = (h | l) + cpu.x;
    if ((addr & 0xFF00) != h) cpu.next_clock_cycle++;
    return addr;
}

uint16_t aby() {
    cpu.op_length = 3;
    uint16_t l = (uint16_t)cpu.bus->cpu_read(cpu.pc + 1);
    uint16_t h = (uint16_t)cpu.bus->cpu_read(cpu.pc + 2) << 8;
    uint16_t addr = (h | l) + cpu.y;
    if ((addr & 0xFF00) != h) cpu.next_clock_cycle++;
    return addr;
}

uint16_t ind() {
    cpu.op_length = 3;
    uint16_t low = (uint16_t)cpu.bus->cpu_read(cpu.pc + 1);
    uint16_t high = (uint16_t)cpu.bus->cpu_read(cpu.pc + 2);
    uint16_t addr = (high<<8) | low;
    low = (uint16_t)cpu.bus->cpu_read(addr);
    //这里要复现bug, 2FF + 1 = 200
    uint8_t addr_low = addr & 0xFF;
    addr_low++;
    addr = (addr & 0xFF00) | (uint16_t)addr_low;
    high = (uint16_t)cpu.bus->cpu_read(addr);
    addr = (high<<8) | low;
    return addr;
}

uint16_t izx() {
    cpu.op_length = 2;
    uint8_t addr = cpu.bus->cpu_read(cpu.pc + 1) + cpu.x;
    uint16_t low = (uint16_t)cpu.bus->cpu_read(addr);   
    addr++;
    uint16_t high = (uint16_t)cpu.bus->cpu_read(addr);
    return (high<<8) | low;
}

uint16_t izy() {
    cpu.op_length = 2;
    uint8_t m = cpu.bus->cpu_read(cpu.pc + 1);
    uint16_t low = (uint16_t)cpu.bus->cpu_read(m);
    m++;
    uint16_t high = (uint16_t)cpu.bus->cpu_read(m)<<8;
    uint16_t addr = (high | low) + (uint16_t)cpu.y;
    if ((addr & 0xFF00) != high) cpu.next_clock_cycle++;
    return addr;
}

uint16_t rel() {
    cpu.op_length = 2;
    uint16_t addr = (uint16_t)cpu.bus->cpu_read(cpu.pc + 1);
    if (IS_NEG(addr)) {
        addr |= 0xFF00;
    }
    return addr;
}

Instruction instructions[code_count] = {
    // 0x00 - 0x0F
    {/* BRK imp */ BRK, imp, 7}, {/* ORA izx */ ORA, izx, 6}, 
    {/* ??? */ XXX, imp, 2}, {/* SLO izx */ XXX, izx, 8},
    {/* NOP zp */ NOP, zp, 3}, {/* ORA zp */ ORA, zp, 3},
    {/* ASL zp */ ASL, zp, 5}, {/* SLO zp */ XXX, zp, 5},
    {/* PHP imp */ PHP, imp, 3}, {/* ORA imm */ ORA, imm, 2},
    {/* ASL A */ ASL, acc, 2}, {/* ANC imm */ XXX, imm, 2},
    {/* NOP abs */ NOP, abs, 4}, {/* ORA abs */ ORA, abs, 4},
    {/* ASL abs */ ASL, abs, 6}, {/* SLO abs */ XXX, abs, 6},

    // 0x10 - 0x1F
    {/* BPL rel */ BPL, rel, 2}, {/* ORA izy */ ORA, izy, 5},
    {/* ??? */ XXX, imp, 2}, {/* SLO izy */ XXX, izy, 8},
    {/* NOP zpx */ NOP, zpx, 4}, {/* ORA zpx */ ORA, zpx, 4},
    {/* ASL zpx */ ASL, zpx, 6}, {/* SLO zpx */ XXX, zpx, 6},
    {/* CLC imp */ CLC, imp, 2}, {/* ORA aby */ ORA, aby, 4},
    {/* NOP imp */ NOP, imp, 2}, {/* SLO aby */ XXX, aby, 7},
    {/* NOP abx */ NOP, abx, 4}, {/* ORA abx */ ORA, abx, 4},
    {/* ASL abx */ ASL, abx, 7}, {/* SLO abx */ XXX, abx, 7},

    // 0x20 - 0x2F
    {/* JSR abs */ JSR, abs, 6}, {/* AND izx */ AND, izx, 6},
    {/* ??? */ XXX, imp, 2}, {/* RLA izx */ XXX, izx, 8},
    {/* BIT zp */ BIT, zp, 3}, {/* AND zp */ AND, zp, 3},
    {/* ROL zp */ ROL, zp, 5}, {/* RLA zp */ XXX, zp, 5},
    {/* PLP imp */ PLP, imp, 4}, {/* AND imm */ AND, imm, 2},
    {/* ROL A */ ROL, acc, 2}, {/* ANC imm */ XXX, imm, 2},
    {/* BIT abs */ BIT, abs, 4}, {/* AND abs */ AND, abs, 4},
    {/* ROL abs */ ROL, abs, 6}, {/* RLA abs */ XXX, abs, 6},

    // 0x30 - 0x3F
    {/* BMI rel */ BMI, rel, 2}, {/* AND izy */ AND, izy, 5},
    {/* ??? */ XXX, imp, 2}, {/* RLA izy */ XXX, izy, 8},
    {/* NOP zpx */ NOP, zpx, 4}, {/* AND zpx */ AND, zpx, 4},
    {/* ROL zpx */ ROL, zpx, 6}, {/* RLA zpx */ XXX, zpx, 6},
    {/* SEC imp */ SEC, imp, 2}, {/* AND aby */ AND, aby, 4},
    {/* NOP imp */ NOP, imp, 2}, {/* RLA aby */ XXX, aby, 7},
    {/* NOP abx */ NOP, abx, 4}, {/* AND abx */ AND, abx, 4},
    {/* ROL abx */ ROL, abx, 7}, {/* RLA abx */ XXX, abx, 7},

    // 0x40 - 0x4F
    {/* RTI imp */ RTI, imp, 6}, {/* EOR izx */ EOR, izx, 6},
    {/* ??? */ XXX, imp, 2}, {/* SRE izx */ XXX, izx, 8},
    {/* NOP zp */ NOP, zp, 3}, {/* EOR zp */ EOR, zp, 3},
    {/* LSR zp */ LSR, zp, 5}, {/* SRE zp */ XXX, zp, 5},
    {/* PHA imp */ PHA, imp, 3}, {/* EOR imm */ EOR, imm, 2},
    {/* LSR A */ LSR, acc, 2}, {/* ALR imm */ XXX, imm, 2},
    {/* JMP abs */ JMP, abs, 3}, {/* EOR abs */ EOR, abs, 4},
    {/* LSR abs */ LSR, abs, 6}, {/* SRE abs */ XXX, abs, 6},

    // 0x50 - 0x5F
    {/* BVC rel */ BVC, rel, 2}, {/* EOR izy */ EOR, izy, 5},
    {/* ??? */ XXX, imp, 2}, {/* SRE izy */ XXX, izy, 8},
    {/* NOP zpx */ NOP, zpx, 4}, {/* EOR zpx */ EOR, zpx, 4},
    {/* LSR zpx */ LSR, zpx, 6}, {/* SRE zpx */ XXX, zpx, 6},
    {/* CLI imp */ CLI, imp, 2}, {/* EOR aby */ EOR, aby, 4},
    {/* NOP imp */ NOP, imp, 2}, {/* SRE aby */ XXX, aby, 7},
    {/* NOP abx */ NOP, abx, 4}, {/* EOR abx */ EOR, abx, 4},
    {/* LSR abx */ LSR, abx, 7}, {/* SRE abx */ XXX, abx, 7},

    // 0x60 - 0x6F
    {/* RTS imp */ RTS, imp, 6}, {/* ADC izx */ ADC, izx, 6},
    {/* ??? */ XXX, imp, 2}, {/* RRA izx */ XXX, izx, 8},
    {/* NOP zp */ NOP, zp, 3}, {/* ADC zp */ ADC, zp, 3},
    {/* ROR zp */ ROR, zp, 5}, {/* RRA zp */ XXX, zp, 5},
    {/* PLA imp */ PLA, imp, 4}, {/* ADC imm */ ADC, imm, 2},
    {/* ROR A */ ROR, acc, 2}, {/* ARR imm */ XXX, imm, 2},
    {/* JMP ind */ JMP, ind, 5}, {/* ADC abs */ ADC, abs, 4},
    {/* ROR abs */ ROR, abs, 6}, {/* RRA abs */ XXX, abs, 6},

    // 0x70 - 0x7F
    {/* BVS rel */ BVS, rel, 2}, {/* ADC izy */ ADC, izy, 5},
    {/* ??? */ XXX, imp, 2}, {/* RRA izy */ XXX, izy, 8},
    {/* NOP zpx */ NOP, zpx, 4}, {/* ADC zpx */ ADC, zpx, 4},
    {/* ROR zpx */ ROR, zpx, 6}, {/* RRA zpx */ XXX, zpx, 6},
    {/* SEI imp */ SEI, imp, 2}, {/* ADC aby */ ADC, aby, 4},
    {/* NOP imp */ NOP, imp, 2}, {/* RRA aby */ XXX, aby, 7},
    {/* NOP abx */ NOP, abx, 4}, {/* ADC abx */ ADC, abx, 4},
    {/* ROR abx */ ROR, abx, 7}, {/* RRA abx */ XXX, abx, 7},

    // 0x80 - 0x8F
    {/* Nop imm */ NOP, imm, 2}, {/* STA izx */ STA, izx, 6},
    {/* NOP imm */ NOP, imm, 2}, {/* SAX izx */ XXX, izx, 6},
    {/* STY zp */ STY, zp, 3}, {/* STA zp */ STA, zp, 3},
    {/* STX zp */ STX, zp, 3}, {/* SAX zp */ XXX, zp, 3},
    {/* DEY imp */ DEY, imp, 2}, {/* NOP imm */ NOP, imm, 2},
    {/* TXA imp */ TXA, imp, 2}, {/* XAA imm */ XXX, imm, 2},
    {/* STY abs */ STY, abs, 4}, {/* STA abs */ STA, abs, 4},
    {/* STX abs */ STX, abs, 4}, {/* SAX abs */ XXX, abs, 4},

    // 0x90 - 0x9F
    {/* BCC rel */ BCC, rel, 2}, {/* STA izy */ STA, izy, 6},
    {/* ??? */ XXX, imp, 2}, {/* SHA izy */ XXX, izy, 6},
    {/* STY zpx */ STY, zpx, 4}, {/* STA zpx */ STA, zpx, 4},
    {/* STX zpy */ STX, zpy, 4}, {/* SAX zpy */ XXX, zpy, 4},
    {/* TYA imp */ TYA, imp, 2}, {/* STA aby */ STA, aby, 5},
    {/* TXS imp */ TXS, imp, 2}, {/* TAS aby */ XXX, aby, 5},
    {/* SHY abx */ XXX, abx, 5}, {/* STA abx */ STA, abx, 5},
    {/* SHX aby */ XXX, aby, 5}, {/* SHA aby */ XXX, aby, 5},

    // 0xA0 - 0xAF
    {/* LDY imm */ LDY, imm, 2}, {/* LDA izx */ LDA, izx, 6},
    {/* LDX imm */ LDX, imm, 2}, {/* LAX izx */ XXX, izx, 6},
    {/* LDY zp */ LDY, zp, 3}, {/* LDA zp */ LDA, zp, 3},
    {/* LDX zp */ LDX, zp, 3}, {/* LAX zp */ XXX, zp, 3},
    {/* TAY imp */ TAY, imp, 2}, {/* LDA imm */ LDA, imm, 2},
    {/* TAX imp */ TAX, imp, 2}, {/* LAX imm */ XXX, imm, 2},
    {/* LDY abs */ LDY, abs, 4}, {/* LDA abs */ LDA, abs, 4},
    {/* LDX abs */ LDX, abs, 4}, {/* LAX abs */ XXX, abs, 4},

    // 0xB0 - 0xBF
    {/* BCS rel */ BCS, rel, 2}, {/* LDA izy */ LDA, izy, 5},
    {/* ??? */ XXX, imp, 2}, {/* LAX izy */ XXX, izy, 5},
    {/* LDY zpx */ LDY, zpx, 4}, {/* LDA zpx */ LDA, zpx, 4},
    {/* LDX zpy */ LDX, zpy, 4}, {/* LAX zpy */ XXX, zpy, 4},
    {/* CLV imp */ CLV, imp, 2}, {/* LDA aby */ LDA, aby, 4},
    {/* TSX imp */ TSX, imp, 2}, {/* LAS aby */ XXX, aby, 4},
    {/* LDY abx */ LDY, abx, 4}, {/* LDA abx */ LDA, abx, 4},
    {/* LDX aby */ LDX, aby, 4}, {/* LAX aby */ XXX, aby, 4},

    // 0xC0 - 0xCF
    {/* CPY imm */ CPY, imm, 2}, {/* CMP izx */ CMP, izx, 6},
    {/* NOP imm */ NOP, imm, 2}, {/* DCP izx */ XXX, izx, 8},
    {/* CPY zp */ CPY, zp, 3}, {/* CMP zp */ CMP, zp, 3},
    {/* DEC zp */ DEC, zp, 5}, {/* DCP zp */ XXX, zp, 5},
    {/* INY imp */ INY, imp, 2}, {/* CMP imm */ CMP, imm, 2},
    {/* DEX imp */ DEX, imp, 2}, {/* AXS imm */ XXX, imm, 2},
    {/* CPY abs */ CPY, abs, 4}, {/* CMP abs */ CMP, abs, 4},
    {/* DEC abs */ DEC, abs, 6}, {/* DCP abs */ XXX, abs, 6},

    // 0xD0 - 0xDF
    {/* BNE rel */ BNE, rel, 2}, {/* CMP izy */ CMP, izy, 5},
    {/* ??? */ XXX, imp, 2}, {/* DCP izy */ XXX, izy, 8},
    {/* NOP zpx */ NOP, zpx, 4}, {/* CMP zpx */ CMP, zpx, 4},
    {/* DEC zpx */ DEC, zpx, 6}, {/* DCP zpx */ XXX, zpx, 6},
    {/* CLD imp */ CLD, imp, 2}, {/* CMP aby */ CMP, aby, 4},
    {/* NOP imp */ NOP, imp, 2}, {/* DCP aby */ XXX, aby, 7},
    {/* NOP abx */ NOP, abx, 4}, {/* CMP abx */ CMP, abx, 4},
    {/* DEC abx */ DEC, abx, 7}, {/* DCP abx */ XXX, abx, 7},

    // 0xE0 - 0xEF
    {/* CPX imm */ CPX, imm, 2}, {/* SBC izx */ SBC, izx, 6},
    {/* NOP imm */ NOP, imm, 2}, {/* ISB izx */ XXX, izx, 8},
    {/* CPX zp */ CPX, zp, 3}, {/* SBC zp */ SBC, zp, 3},
    {/* INC zp */ INC, zp, 5}, {/* ISB zp */ XXX, zp, 5},
    {/* INX imp */ INX, imp, 2}, {/* SBC imm */ SBC, imm, 2},
    {/* NOP imp */ NOP, imp, 2}, {/* SBC imm */ SBC, imm, 2},
    {/* CPX abs */ CPX, abs, 4}, {/* SBC abs */ SBC, abs, 4},
    {/* INC abs */ INC, abs, 6}, {/* ISB abs */ XXX, abs, 6},

    // 0xF0 - 0xFF
    {/* BEQ rel */ BEQ, rel, 2}, {/* SBC izy */ SBC, izy, 5},
    {/* ??? */ XXX, imp, 2}, {/* ISB izy */ XXX, izy, 8},
    {/* NOP zpx */ NOP, zpx, 4}, {/* SBC zpx */ SBC, zpx, 4},
    {/* INC zpx */ INC, zpx, 6}, {/* ISB zpx */ XXX, zpx, 6},
    {/* SED imp */ SED, imp, 2}, {/* SBC aby */ SBC, aby, 4},
    {/* NOP imp */ NOP, imp, 2}, {/* ISB aby */ XXX, aby, 7},
    {/* NOP abx */ NOP, abx, 4}, {/* SBC abx */ SBC, abx, 4},
    {/* INC abx */ INC, abx, 7}, {/* ISB abx */ XXX, abx, 7},
};