#pragma once
#include "common.h"

struct HookTrigger {
    Combo combo;
    int index = -1;
    bool block = true;
};

struct HookConfig {
    bool enabled = true;
    bool fgOnly = true;
    std::vector<std::wstring> patterns;
    Combo toggle;
    std::vector<HookTrigger> triggers;
};

namespace hook {
bool Start(HWND notify);
void Stop();
void Configure(const HookConfig& cfg);
void SetCapture(bool on);
// 当前前台窗口是否允许执行（永远排除本程序自己的窗口）
bool ForegroundAllowed(bool fgOnly, const std::vector<std::wstring>& patterns);
}  // namespace hook
