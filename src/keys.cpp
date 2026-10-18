#include "keys.h"

namespace {
struct KeyName { const wchar_t* name; UINT vk; };

// 规范名称在前，别名在后（VkName 取第一个匹配）
const KeyName kNames[] = {
    {L"Backspace", VK_BACK}, {L"Tab", VK_TAB}, {L"Enter", VK_RETURN}, {L"Pause", VK_PAUSE},
    {L"CapsLock", VK_CAPITAL}, {L"Esc", VK_ESCAPE}, {L"Space", VK_SPACE},
    {L"PageUp", VK_PRIOR}, {L"PageDown", VK_NEXT}, {L"End", VK_END}, {L"Home", VK_HOME},
    {L"Left", VK_LEFT}, {L"Up", VK_UP}, {L"Right", VK_RIGHT}, {L"Down", VK_DOWN},
    {L"Insert", VK_INSERT}, {L"Delete", VK_DELETE}, {L"NumLock", VK_NUMLOCK},
    {L"ScrollLock", VK_SCROLL}, {L"PrintScreen", VK_SNAPSHOT},
    {L"Num*", VK_MULTIPLY}, {L"Num+", VK_ADD}, {L"Num-", VK_SUBTRACT}, {L"Num.", VK_DECIMAL},
    {L"Num/", VK_DIVIDE},
    {L"-", VK_OEM_MINUS}, {L"=", VK_OEM_PLUS}, {L"[", VK_OEM_4}, {L"]", VK_OEM_6},
    {L";", VK_OEM_1}, {L"'", VK_OEM_7}, {L",", VK_OEM_COMMA}, {L".", VK_OEM_PERIOD},
    {L"/", VK_OEM_2}, {L"\\", VK_OEM_5}, {L"`", VK_OEM_3},
    {L"Shift", VK_SHIFT}, {L"Ctrl", VK_CONTROL}, {L"Alt", VK_MENU},
    {L"Mouse3", VK_MBUTTON}, {L"Mouse4", VK_XBUTTON1}, {L"Mouse5", VK_XBUTTON2},
    {L"LButton", VK_LBUTTON}, {L"RButton", VK_RBUTTON},
    // 别名
    {L"Return", VK_RETURN}, {L"Escape", VK_ESCAPE}, {L"Del", VK_DELETE}, {L"Ins", VK_INSERT},
    {L"PgUp", VK_PRIOR}, {L"PgDn", VK_NEXT}, {L"Control", VK_CONTROL}, {L"MButton", VK_MBUTTON},
    {L"XButton1", VK_XBUTTON1}, {L"XButton2", VK_XBUTTON2}, {L"Minus", VK_OEM_MINUS},
    {L"Equal", VK_OEM_PLUS}, {L"Grave", VK_OEM_3}, {L"NumPlus", VK_ADD}, {L"NumMinus", VK_SUBTRACT},
    {L"Spacebar", VK_SPACE}, {L"空格", VK_SPACE}, {L"回车", VK_RETURN},
};
}  // namespace

bool IsModifierVk(UINT vk) {
    switch (vk) {
        case VK_SHIFT: case VK_LSHIFT: case VK_RSHIFT:
        case VK_CONTROL: case VK_LCONTROL: case VK_RCONTROL:
        case VK_MENU: case VK_LMENU: case VK_RMENU:
        case VK_LWIN: case VK_RWIN:
            return true;
    }
    return false;
}

bool IsMouseVk(UINT vk) {
    return vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON || vk == VK_XBUTTON1 ||
           vk == VK_XBUTTON2;
}

std::wstring VkName(UINT vk) {
    if ((vk >= 'A' && vk <= 'Z') || (vk >= '0' && vk <= '9')) return std::wstring(1, (wchar_t)vk);
    if (vk >= VK_F1 && vk <= VK_F24) return L"F" + std::to_wstring(vk - VK_F1 + 1);
    if (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9) return L"Num" + std::to_wstring(vk - VK_NUMPAD0);
    for (const auto& k : kNames)
        if (k.vk == vk) return k.name;
    wchar_t buf[16];
    swprintf(buf, 16, L"VK_%02X", vk);
    return buf;
}

bool VkFromName(const std::wstring& raw, UINT& vk) {
    std::wstring n = str::Trim(raw);
    if (n.empty()) return false;
    if (n.size() == 1) {
        wchar_t c = n[0];
        if (c >= L'a' && c <= L'z') c = (wchar_t)(c - 32);
        if ((c >= L'A' && c <= L'Z') || (c >= L'0' && c <= L'9')) { vk = c; return true; }
    }
    std::wstring l = str::Lower(n);
    if (l.size() >= 2 && l[0] == L'f') {
        int v = 0;
        if (str::ParseInt(l.substr(1), v) && v >= 1 && v <= 24) { vk = VK_F1 + v - 1; return true; }
    }
    if (l.size() == 4 && l.compare(0, 3, L"num") == 0 && l[3] >= L'0' && l[3] <= L'9') {
        vk = VK_NUMPAD0 + (l[3] - L'0');
        return true;
    }
    if (l.size() > 3 && l.compare(0, 3, L"vk_") == 0) {
        wchar_t* e = nullptr;
        long v = wcstol(l.c_str() + 3, &e, 16);
        if (e && !*e && v > 0 && v < 256) { vk = (UINT)v; return true; }
    }
    for (const auto& k : kNames)
        if (str::IEquals(n, k.name)) { vk = k.vk; return true; }
    return false;
}

bool ParseCombo(const std::wstring& src, Combo& out) {
    std::wstring s = str::Trim(src);
    if (s.empty()) return false;
    // 按 '+' 拆分，但保留 "Num+"
    std::vector<std::wstring> parts;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == L'+' || c == L'\xFF0B') {
            if (str::IEquals(str::Trim(cur), L"num")) { cur += L'+'; continue; }
            parts.push_back(cur);
            cur.clear();
        } else {
            cur += c;
        }
    }
    parts.push_back(cur);

    Combo c;
    for (size_t i = 0; i < parts.size(); ++i) {
        std::wstring t = str::Trim(parts[i]);
        if (t.empty()) return false;
        UINT vk = 0;
        if (!VkFromName(t, vk)) return false;
        if (i + 1 < parts.size()) {
            if (vk == VK_CONTROL) c.mods |= KM_CTRL;
            else if (vk == VK_SHIFT) c.mods |= KM_SHIFT;
            else if (vk == VK_MENU) c.mods |= KM_ALT;
            else return false;
        } else {
            c.vk = vk;
        }
    }
    out = c;
    return true;
}

std::wstring ComboToString(const Combo& c) {
    if (c.Empty()) return L"";
    std::wstring s;
    if (c.mods & KM_CTRL) s += L"Ctrl+";
    if (c.mods & KM_ALT) s += L"Alt+";
    if (c.mods & KM_SHIFT) s += L"Shift+";
    return s + VkName(c.vk);
}
