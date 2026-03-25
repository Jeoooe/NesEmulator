//一些全局常数
#pragma once

enum class NtArrangement {
    OneScreenLower = 0,
    OneScreenUpper,
    Vertical,
    Horizontal,
    FourScreen
};

inline constexpr int width  = 256;
inline constexpr int height = 240;

// #define TEST_WINDOW

// #define NO_UI
// #define DISASSEMBLE