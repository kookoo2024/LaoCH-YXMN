// LaoCH-YXMN — 共享类型与工具函数
#pragma once
#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#ifndef WINVER
#define WINVER 0x0A00
#endif
#include <windows.h>
#include <string>
#include <vector>
#include <cstdint>
#include <cwchar>

#define APP_TITLE   L"LaoCH 键位助手"
#define APP_VERSION L"1.0.0"
#define APP_CLASS   L"LaoCH_YXMN_Main"

enum : UINT {
    WM_APP_TRIGGER = WM_APP + 1,  // wParam = 宏序号, lParam = 1 按下 / 0 松开
    WM_APP_TOGGLE,                // 总开关热键
    WM_APP_CAPTURE,               // wParam = vk, lParam = 修饰键
    WM_APP_PLAYER,                // wParam = 停止原因, lParam = 运行代号
    WM_APP_TRAY,                  // 托盘回调
};

enum : UINT { KM_CTRL = 1, KM_SHIFT = 2, KM_ALT = 4 };

struct Combo {
    UINT vk = 0;
    UINT mods = 0;
    bool Empty() const { return vk == 0; }
    bool operator==(const Combo& o) const { return vk == o.vk && mods == o.mods; }
    bool operator!=(const Combo& o) const { return !(*this == o); }
};

enum class Mode : int { Once = 0, Loop = 1, Hold = 2 };

struct Macro {
    std::wstring name;
    Combo trigger;
    Mode mode = Mode::Once;
    bool enabled = true;
    bool block = true;     // 拦截触发键，不让游戏收到
    std::wstring script;   // 内部以 \r\n 分行
};

struct Settings {
    Combo toggleKey{VK_PAUSE, 0};
    int gcdMs = 1500;
    int jitterPct = 12;
    std::wstring windowMatch = L"魔兽世界|World of Warcraft";
    bool foregroundOnly = true;
    bool humanize = true;
    bool sound = true;
};

namespace str {

inline std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t\r\n\x3000");
    if (a == std::wstring::npos) return L"";
    size_t b = s.find_last_not_of(L" \t\r\n\x3000");
    return s.substr(a, b - a + 1);
}

inline std::wstring Lower(std::wstring s) {
    if (!s.empty()) CharLowerBuffW(&s[0], (DWORD)s.size());
    return s;
}

inline std::vector<std::wstring> Split(const std::wstring& s, wchar_t sep) {
    std::vector<std::wstring> out;
    size_t pos = 0;
    for (;;) {
        size_t p = s.find(sep, pos);
        if (p == std::wstring::npos) { out.push_back(s.substr(pos)); break; }
        out.push_back(s.substr(pos, p - pos));
        pos = p + 1;
    }
    return out;
}

inline std::wstring FromUtf8(const std::string& s) {
    if (s.empty()) return L"";
    int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), nullptr, 0);
    std::wstring w(n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), (int)s.size(), &w[0], n);
    return w;
}

inline std::string ToUtf8(const std::wstring& w) {
    if (w.empty()) return "";
    int n = WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    std::string s(n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), (int)w.size(), &s[0], n, nullptr, nullptr);
    return s;
}

inline bool ParseInt(const std::wstring& s, int& out) {
    std::wstring t = Trim(s);
    if (t.empty()) return false;
    wchar_t* end = nullptr;
    long v = wcstol(t.c_str(), &end, 10);
    if (!end || *end) return false;
    out = (int)v;
    return true;
}

inline bool IEquals(const std::wstring& a, const wchar_t* b) {
    return CompareStringOrdinal(a.c_str(), (int)a.size(), b, -1, TRUE) == CSTR_EQUAL;
}

}  // namespace str

// "魔兽世界|World of Warcraft" -> {"魔兽世界", "world of warcraft"}
inline std::vector<std::wstring> ParsePatterns(const std::wstring& s) {
    std::vector<std::wstring> out;
    for (auto& p : str::Split(s, L'|')) {
        std::wstring t = str::Lower(str::Trim(p));
        if (!t.empty()) out.push_back(t);
    }
    return out;
}

template <class T> inline T Clamp(T v, T lo, T hi) { return v < lo ? lo : (v > hi ? hi : v); }
