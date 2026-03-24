#pragma once

#include <stdint.h>
#include <queue>
#include <array>
#include <atomic>
#include <mutex>

#include <timing.h>


class APU {
public:
    APU() = default;
    ~APU() = default;
    void cpu_write(uint16_t addr, uint8_t value);
    uint8_t cpu_read(uint16_t addr);
    void run();

    
    struct APUAudioChannel {
        float frq;
        float volume;
        float duty;
    };
    struct APUAudioStream {
        APUAudioChannel squares1;
        APUAudioChannel squares2;
        APUAudioChannel triangle;
        int sample_count;
    };

    bool consume_audio_sample(float *sample, size_t count);    //false失败
    int audio_buffer_size();

private:
    void update_channels();    

private:
    struct FrameCounter {
        int step = 0;
        int mode = 0;
        bool irq_enabled = false;
    } frame_counter;

    struct Envelope {
        bool start = false; //也是一个边沿效应
        bool loop = false;
        bool constant_volume = true;
        uint8_t volume = 0; //同时是开启时的分配器周期P
        uint8_t counter = 0;
        uint8_t divider = 0;
        inline float get_volume() {
            return constant_volume ? volume : counter;
        }
    };
    struct LengthCounter {
        bool halt = true;
        uint8_t counter = 0;
        inline bool muting() {
            return counter == 0;
        }
        void update();
    };
    struct LinearCounter {
        bool control = true;    //貌似是暂停标志的意思
        bool reload_flag = false;
        uint8_t load = 0;
        uint8_t counter = 0;
        inline bool muting() {
            return counter == 0;
        }
        void update();
    };
    struct Sweep {
        bool enabled = false;
        bool negate = false;
        bool reload = false;   //边沿效应触发, 即修改寄存器时有reload
        uint8_t period = 0;
        uint8_t divider = 0;
        uint8_t shift_count = 0;
    };
    
    struct SquareChannel {
        Envelope envelope;
        LengthCounter length_counter;
        Sweep sweep;
        bool enabled = false;
        uint16_t timer; //记得同步frq
        float duty;
        float frq = 0.0f;
        float phase = 0.0f;
        float get_sample();
        inline void set_timer(uint16_t t) {
            timer = t;
            frq = CPU_FRQ / (16.0f * (timer + 1));
        }
        void update_sweep(int id);  //由于两个方波sweep行为不一样
        void update_envelope();
    } square1, square2; 

    struct TriangleChannel {
        LengthCounter length_counter;
        LinearCounter linear_counter;
        bool enabled = false;
        float frq = 0.0f;
        float phase = 0.0f;
        uint16_t timer; //记得同步frq
        uint8_t output; //由于关闭时会持续输出上次的值
        inline void set_timer(uint16_t t) {
            timer = t;
            frq = CPU_FRQ / (32.0f * (timer + 1));
        }
        float get_sample();
    } triangle;

    struct NoiseChannel {
        Envelope envelope;
        LengthCounter length_counter;
        bool enabled = false;
        uint8_t mode;
        uint8_t period;
        float get_sample();
    } noise;

    uint8_t irq_flag = 0;
    uint8_t dmc_irq_flag = 0;

    //方波寄存器
    // uint8_t reg4000, reg4004;
    // uint8_t reg4001, reg4005;
    // uint8_t reg4002, reg4006;
    // uint8_t reg4003, reg4007;
    std::atomic_uint8_t regs[0x17];
};