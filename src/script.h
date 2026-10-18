#pragma once
#include "common.h"

enum class Op : uint8_t { Tap, Down, Up, Wait, Gcd, Click, MDown, MUp, Move, MoveRel, Scroll, Repeat, End };
enum : int { BTN_LEFT = 0, BTN_RIGHT, BTN_MIDDLE, BTN_X1, BTN_X2 };

struct Step {
    Op op = Op::Wait;
    Combo key;
    int a = 0, b = 0;
    int jump = -1;   // Repeat <-> End 互相指向
    int line = 0;
};

struct Program {
    std::vector<Step> steps;
    int actions = 0;  // 实际输入动作数
};

struct ParseError {
    int line = 0;
    std::wstring msg;
};

bool ParseScript(const std::wstring& src, Program& out, ParseError& err);
