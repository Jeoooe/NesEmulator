#include <apu.h>

#include <mutex>
#include <vector>
#include <cstring>

#include <bus.h>
#include <cpu.h>
#include <const.h>
#include <log.h>
#include <timing.h>

#define STATUS_D 0x10
#define STATUS_N 0x08
#define STATUS_T 0x04
#define STATUS_2 0x02
#define STATUS_1 0x01

#define D0 1
#define D1 2
#define D2 4
#define D3 8
#define D4 16
#define D5 32
#define D6 64
#define D7 128
#define D8 0x100
#define D9 0x200
#define D10 0x400
#define D11 0x800
#define D12 0x1000
#define D13 0x2000
#define D14 0x4000
#define D15 0x8000

//一个样本要多少tick
constexpr inline float TickPerSample = TICK_FRQ / SampleRate;
//那么一帧有多少样本?
//一帧有多少tick
constexpr inline float TickPerFrame = TICK_FRQ / FrameRate;
// constexpr inline float sample_per_frame = tick_per_frame / tick_per_sample;
constexpr inline float SamplePerFrame = SampleRate / FrameRate;

//计算帧计数器每一步的tick数
constexpr inline uint64_t TickPerFrameCount = TICK_FRQ / FrameRate / 4; //每个步骤的tick数, 4步一个周期
constexpr inline float SamplePerFrameCount = SampleRate / FrameRate / 4; //每个步骤的样本数

//APU 内部的一些数据
constexpr inline uint8_t LengthCountTable[32] = {
    10, 254, 20,  2, 40, 4, 80, 6,
    160, 8, 60, 10, 14, 12, 26, 14,
    12, 16, 24, 18, 48, 20, 96, 22,
    192, 24, 72, 26, 16, 28, 32, 30
};

// inline std::vector<float> audio_buffer(static_cast<size_t>(SamplePerFrame*6));
struct AudioBuffer {
    // static constexpr unsigned int capacity = static_cast<size_t>(SamplePerFrame*6);
    static constexpr unsigned int capacity = 4096;
    unsigned int head = 0, tail = 0;
    std::mutex lock;
    float audio_buffer[capacity];
    inline unsigned int size() const {
        return tail > head ? tail - head : tail + capacity - head;
    }
    inline bool push(float sample) noexcept {
        if (size() >= capacity) head = (head + 1) % capacity;
        audio_buffer[tail] = sample;
        tail = (tail + 1) % capacity;
        return true;
    }
    inline bool pop(float *sample) noexcept {
        if (size() == 0) return false;
        *sample = audio_buffer[head];
        head = (head + 1) % capacity; 
        return true;
    }
    bool consume(void *dst, size_t count) {
        if (size() < count) return false;
        if (head < tail || head + count < capacity) {
            memcpy(dst, audio_buffer + head, count * sizeof(float));
        } else {
            memcpy(dst, audio_buffer + head, (capacity - head) * sizeof(float));
            memcpy(dst, audio_buffer, (count - (capacity - head)) * sizeof(float));
        }
        head = (head + count) % capacity;
        return true;
    }
} audio_buffer;
/*
直接采用缓存? 缓存6帧
每一个声道改变时都截断所有流? 先这样实现吧
*/

//需要保证在0x4000 - 0x4017之间
void APU::cpu_write(uint16_t addr, uint8_t value) {
    constexpr float duty_table[4] = {0.125, 0.25, 0.5, 0.75}; 
    addr -= 0x4000;
    regs[addr] = value;
    switch (addr) {
    case 0x17: {
        frame_counter.irq_enabled = value & D6;
        frame_counter.mode = (value >> 7) & 1;
        frame_counter.step = 0;
        break;
    }
    case 0x15: {
        square1.enabled = value & D0;
        if (!(value & D0)) square1.length_counter.counter = 0;
        break;
    }
    //sq1
    case 0x00: {
        square1.duty = duty_table[(value >> 6) & 3];
        square1.length_counter.halt = (value >> 5) & 1;
        square1.envelope.loop = !square1.length_counter.halt;
        square1.envelope.constant_volume = (value >> 4) & 1;
        square1.envelope.volume = value & 0xF;
        break;
    }
    case 0x01: {
        square1.sweep.enabled = value >> 7;
        square1.sweep.period = (value >> 4) & 7;
        square1.sweep.negate = (value >> 3) & 1;
        square1.sweep.shift_count = value & 7;
        break;
    }
    case 0x02: {
        auto timer = square1.timer;
        square1.set_timer((timer & 0x700) | value);
        break;
    }
    case 0x03: {
        auto timer = square1.timer;
        const uint16_t num = (uint16_t)value & 7;
        square1.set_timer((timer & 0xFF) | num << 8);
        square1.length_counter.counter = LengthCountTable[value >> 3];
        break;
    }
    //sq2
    case 0x04: {
        square2.duty = duty_table[(value >> 6) & 3];
        square2.length_counter.halt = (value >> 5) & 1;
        square2.envelope.loop = !square2.length_counter.halt;
        square2.envelope.constant_volume = (value >> 4) & 1;
        square2.envelope.volume = value & 0xF;
        break;
    }
    case 0x05: {
        square2.sweep.enabled = value >> 7;
        square2.sweep.period = (value >> 4) & 7;
        square2.sweep.negate = (value >> 3) & 1;
        square2.sweep.shift_count = value & 7;
        break;
    }
    case 0x06: {
        auto timer = square1.timer;
        square2.set_timer((timer & 0x700) | value);
        break;
    }
    case 0x07: {
        auto timer = square2.timer;
        const uint16_t num = (uint16_t)value & 7;
        square2.set_timer((timer & 0xFF) | num << 8);
        square2.length_counter.counter = LengthCountTable[value >> 3];
        break;
    }
    
    
    default:
        break;
    }
}

uint8_t APU::cpu_read(uint16_t addr) {
    if (addr == 0x4015) {
        uint8_t sq1_flag = square1.length_counter.counter > 0 ? 1 : 0;
        return (
            dmc_irq_flag << 7 |
            irq_flag << 6 | 
            0 | //DMC remaining
            0b0000 |
            sq1_flag
        );
    }
    return 0;
}

// APU::APUAudioChannel make_square_channel(uint8_t reg0, uint8_t reg1, uint8_t reg2, uint8_t reg3) {
//     APU::APUAudioChannel channel{};
//     //频率计算
//     uint16_t timer = ((reg3 & 0x7) << 8) | reg2;
//     if (timer < 8) {
//         channel.frq = 0; //频率过高无法产生声音
//         channel.volume = 0;
//         channel.duty = 0;
//         return channel;
//     }
//     channel.frq = CPU_FRQ / (16.0f * (timer + 1));
//     //音量计算
//     channel.volume = (reg1 & 0xF) * 1.0f; //APU的音量范围是0-15
//     //占空比计算
//     static constexpr float duty_table[4] = {0.125f, 0.25f, 0.5f, 0.75f};
//     channel.duty = duty_table[(reg0 >> 6) & 0x3];
//     return channel;
// }

bool APU::consume_audio_sample(float *sample, size_t count) {
    std::unique_lock<std::mutex> lock{audio_buffer.lock};
    return audio_buffer.consume(sample, count);
}

int APU::audio_buffer_size() {
    return audio_buffer.size();
}

void APU::run() {
    static uint64_t next_tick = TickPerFrameCount;
    auto current_tick = Timing::get_current_tick();
    if (current_tick < next_tick) {
        return;
    }
    //每个帧计数器滴答执行一次
    next_tick += TickPerFrameCount;
    //执行帧计数器的步骤
    frame_counter.step++;
    if (frame_counter.mode == 0) {
        if (frame_counter.step == 3) {
            frame_counter.step = 0;
            if (frame_counter.irq_enabled) {
                irq_flag = 1; 
                Bus::get().get_CPU().trigger_IRQ();
            }
        }
        if (frame_counter.step == 1 || frame_counter.step == 3) {
            //长度计数器和扫描单元 TODO
            square1.length_counter.update();
            square1.update_sweep(1);
            square2.length_counter.update();
            square2.update_sweep(2);
            triangle.length_counter.update();
        }
        square1.update_envelope();
        square2.update_envelope();
        triangle.linear_counter.update();
    } else {
        if (frame_counter.step == 4) {
            frame_counter.step = 0;
            if (frame_counter.irq_enabled) {
                irq_flag = 1;
                Bus::get().get_CPU().trigger_IRQ();
            }
        }
        if (frame_counter.step == 0 || frame_counter.step == 2) {
            //长度计数器和扫描单元 TODO
            square1.length_counter.update();
            square1.update_sweep(1);
        }
        if (frame_counter.step != 4) {
            square1.update_envelope();
            // triangle.linear_counter.counter_signal();
        }
    }
    update_channels();
}

void APU::update_channels() {
    std::unique_lock<std::mutex> lock{audio_buffer.lock};
    static int count = 0;
    static float generated_sample = 0.0f;
    const float target = generated_sample + SamplePerFrameCount;
    for (;count < static_cast<int>(target);count++) {
        float sq1_out = square1.get_sample();
        float sq2_out = square2.get_sample();
        float tri_out = triangle.get_sample();
        float noise_out = 0.0f;
        float dmc_out = 0.0f;
        float pulse_out = 0.00752f * (sq1_out + sq2_out); //方波声道的混音
        float tnd_out = 0.00851f * tri_out + 0.00494f * noise_out + 0.00335f * dmc_out; //三角波、噪声和DMC声道的混音
        float mixed_sample = pulse_out + tnd_out; //最终混音结果
        audio_buffer.push(mixed_sample);
    }
    generated_sample = target;
    if (generated_sample > 10000.0f) {
        count -= 10000;
        generated_sample -= 10000.0f;
    }
}

float APU::SquareChannel::get_sample() {
    if (!enabled || length_counter.muting() || timer < 8 || timer > 0x7FF) {
        return 0.0f;
    }
    float volume = envelope.get_volume();
    float sample = (phase < duty) ? volume : -volume;
    // 更新相位，让它为下一个样本做准备
    phase += frq / SampleRate; // 每个样本的相位增量
    if (phase >= 1.0f) {
        phase -= 1.0f; // 确保相位始终在0到1之间循环
    }
    return sample;
}

float APU::TriangleChannel::get_sample() {
    if (!enabled || length_counter.muting() || linear_counter.muting()) return output;
    //三角波的输出是一个固定的0-15的锯齿波, 频率由timer决定
    output = static_cast<uint8_t>(phase * 32) ^ 0x10; //0-15的锯齿波, 但由于三角波是对称的, 需要异或0x10翻转一下
    // 更新相位，让它为下一个样本做准备
    phase += frq / SampleRate;
    if (phase >= 1.0f) {
        phase -= 1.0f;
    }
    return output;
}


void APU::SquareChannel::update_sweep(int id) {
    if (!sweep.enabled) return;
    if (sweep.divider == 0 && sweep.shift_count != 0) {
        //更新逻辑
        if (timer >= 8) {
            //不是周期过小导致静音
            int16_t num1 = static_cast<int16_t>(timer >> sweep.shift_count);
            if (sweep.negate) {
                num1 = id == 1 ? ~num1 : -num1;
            }
            int16_t num2 = static_cast<int16_t>(timer) + num1;
            if (num2 < 0x800) { //确保不会上溢
                set_timer(static_cast<uint16_t>(num2));
            } 
        } 
    }
    if (sweep.reload || sweep.divider == 0) {
        sweep.divider = sweep.period + 1;
        sweep.reload = false;
    } else {
        sweep.divider--;
    }
}

void APU::LengthCounter::update() {
    if (halt || counter == 0) return;
    counter--;
}


void APU::LinearCounter::update() {
    if (reload_flag) {
        counter = load;
    } else {
        counter--;
    }
    if (!control) {
        reload_flag = false;
    }
}

void APU::SquareChannel::update_envelope() {
    if (envelope.start) {
        envelope.counter = 15;
        envelope.divider = envelope.volume + 1;
        return;
    }
    //分频器获取时钟信号
    if (envelope.divider != 0) {
        envelope.divider--;
        return;
    }
    //分频器=0
    envelope.divider = envelope.volume + 1;
    if (envelope.counter > 0) {
        envelope.counter--;
        return;
    }
    if (envelope.loop) envelope.counter = 15;
}



