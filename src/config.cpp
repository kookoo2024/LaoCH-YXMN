// 配置文件：UTF-8 纯文本，人类可读，可直接复制分享
// 程序目录下存在 LaoCH.ini 时为便携模式，否则保存到 %APPDATA%\LaoCH-YXMN\LaoCH.ini
#include "config.h"
#include "keys.h"

namespace {

std::wstring ExeDir() {
    wchar_t buf[MAX_PATH];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    std::wstring p(buf, n);
    size_t s = p.find_last_of(L"\\/");
    return s == std::wstring::npos ? L"." : p.substr(0, s);
}

bool FileExists(const std::wstring& p) {
    DWORD a = GetFileAttributesW(p.c_str());
    return a != INVALID_FILE_ATTRIBUTES && !(a & FILE_ATTRIBUTE_DIRECTORY);
}

const wchar_t* ModeKey(Mode m) {
    switch (m) {
        case Mode::Loop: return L"loop";
        case Mode::Hold: return L"hold";
        default: return L"once";
    }
}

Mode ModeFromKey(const std::wstring& s) {
    if (str::IEquals(s, L"loop")) return Mode::Loop;
    if (str::IEquals(s, L"hold")) return Mode::Hold;
    return Mode::Once;
}

bool ParseBool(const std::wstring& s) { return s == L"1" || str::IEquals(s, L"true") || str::IEquals(s, L"yes"); }

}  // namespace

const wchar_t* ModeLabel(Mode m) {
    switch (m) {
        case Mode::Loop: return L"循环";
        case Mode::Hold: return L"按住";
        default: return L"单次";
    }
}

std::wstring ResolveConfigPath(bool* portable) {
    std::wstring local = ExeDir() + L"\\LaoCH.ini";
    if (FileExists(local)) {
        if (portable) *portable = true;
        return local;
    }
    if (portable) *portable = false;
    wchar_t buf[MAX_PATH];
    DWORD n = GetEnvironmentVariableW(L"APPDATA", buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH) {
        if (portable) *portable = true;
        return local;
    }
    std::wstring dir = std::wstring(buf) + L"\\LaoCH-YXMN";
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\LaoCH.ini";
}

bool LoadConfig(const std::wstring& path, Settings& st, std::vector<Macro>& macros) {
    HANDLE f = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER size{};
    GetFileSizeEx(f, &size);
    if (size.QuadPart > 8 * 1024 * 1024) { CloseHandle(f); return false; }
    std::string data((size_t)size.QuadPart, '\0');
    DWORD read = 0;
    BOOL ok = data.empty() ? TRUE : ReadFile(f, &data[0], (DWORD)data.size(), &read, nullptr);
    CloseHandle(f);
    if (!ok) return false;
    data.resize(read);
    if (data.size() >= 3 && (unsigned char)data[0] == 0xEF && (unsigned char)data[1] == 0xBB &&
        (unsigned char)data[2] == 0xBF)
        data.erase(0, 3);

    std::vector<Macro> list;
    int cur = -1;
    bool inSettings = false;
    bool any = false;
    for (auto& rawLine : str::Split(str::FromUtf8(data), L'\n')) {
        std::wstring line = rawLine;
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        size_t first = line.find_first_not_of(L" \t");
        if (first != std::wstring::npos && line[first] == L'|') {
            if (cur >= 0) {
                std::wstring s = line.substr(first + 1);
                if (!s.empty() && s[0] == L' ') s.erase(0, 1);
                list[cur].script += s + L"\r\n";
            }
            continue;
        }
        std::wstring t = str::Trim(line);
        if (t.empty() || t[0] == L';') continue;
        if (str::IEquals(t, L"[settings]")) { inSettings = true; any = true; cur = -1; continue; }
        if (str::IEquals(t, L"[macro]")) { any = true; list.emplace_back(); cur = (int)list.size() - 1; inSettings = false; continue; }
        size_t eq = t.find(L'=');
        if (eq == std::wstring::npos) continue;
        std::wstring k = str::Lower(str::Trim(t.substr(0, eq)));
        std::wstring v = str::Trim(t.substr(eq + 1));
        int iv = 0;
        if (inSettings) {
            if (k == L"toggle") { Combo c; st.toggleKey = ParseCombo(v, c) ? c : Combo{}; }
            else if (k == L"gcd" && str::ParseInt(v, iv)) st.gcdMs = Clamp(iv, 100, 10000);
            else if (k == L"jitter" && str::ParseInt(v, iv)) st.jitterPct = Clamp(iv, 0, 40);
            else if (k == L"window") st.windowMatch = v;
            else if (k == L"foreground_only") st.foregroundOnly = ParseBool(v);
            else if (k == L"humanize") st.humanize = ParseBool(v);
            else if (k == L"sound") st.sound = ParseBool(v);
        } else if (cur >= 0) {
            Macro& m = list[cur];
            if (k == L"name") m.name = v;
            else if (k == L"trigger") { Combo c; m.trigger = ParseCombo(v, c) ? c : Combo{}; }
            else if (k == L"mode") m.mode = ModeFromKey(v);
            else if (k == L"enabled") m.enabled = ParseBool(v);
            else if (k == L"block") m.block = ParseBool(v);
        }
    }
    if (!any) return false;  // 空文件：交给调用方写入示例
    for (auto& m : list) {
        while (m.script.size() >= 2 && m.script.compare(m.script.size() - 2, 2, L"\r\n") == 0)
            m.script.resize(m.script.size() - 2);
        if (m.name.empty()) m.name = L"未命名宏";
    }
    macros = std::move(list);
    return true;
}

bool SaveConfig(const std::wstring& path, const Settings& st, const std::vector<Macro>& macros) {
    std::wstring o;
    o += L"; LaoCH-YXMN 配置文件（UTF-8）。可直接编辑或分享给朋友。\r\n";
    o += L"[settings]\r\n";
    o += L"toggle=" + ComboToString(st.toggleKey) + L"\r\n";
    o += L"gcd=" + std::to_wstring(st.gcdMs) + L"\r\n";
    o += L"jitter=" + std::to_wstring(st.jitterPct) + L"\r\n";
    o += L"window=" + st.windowMatch + L"\r\n";
    o += std::wstring(L"foreground_only=") + (st.foregroundOnly ? L"1" : L"0") + L"\r\n";
    o += std::wstring(L"humanize=") + (st.humanize ? L"1" : L"0") + L"\r\n";
    o += std::wstring(L"sound=") + (st.sound ? L"1" : L"0") + L"\r\n";
    for (const auto& m : macros) {
        o += L"\r\n[macro]\r\n";
        o += L"name=" + m.name + L"\r\n";
        o += L"trigger=" + ComboToString(m.trigger) + L"\r\n";
        o += std::wstring(L"mode=") + ModeKey(m.mode) + L"\r\n";
        o += std::wstring(L"enabled=") + (m.enabled ? L"1" : L"0") + L"\r\n";
        o += std::wstring(L"block=") + (m.block ? L"1" : L"0") + L"\r\n";
        for (auto line : str::Split(m.script, L'\n')) {
            if (!line.empty() && line.back() == L'\r') line.pop_back();
            o += L"| " + line + L"\r\n";
        }
    }
    std::string data = str::ToUtf8(o);
    std::wstring tmp = path + L".tmp";
    HANDLE f = CreateFileW(tmp.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (f == INVALID_HANDLE_VALUE) return false;
    DWORD written = 0;
    BOOL ok = WriteFile(f, data.data(), (DWORD)data.size(), &written, nullptr);
    CloseHandle(f);
    if (!ok || written != data.size()) { DeleteFileW(tmp.c_str()); return false; }
    return MoveFileExW(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

std::vector<Macro> DefaultMacros() {
    std::vector<Macro> v;
    Macro a;
    a.name = L"单体输出循环";
    a.trigger = {VK_F5, 0};
    a.mode = Mode::Loop;
    a.script =
        L"# 按 F5 开始循环，再按 F5 停止\r\n"
        L"# 把 1 / 2 / 3 换成你动作条上的技能键\r\n"
        L"1\r\n"
        L"gcd\r\n"
        L"2\r\n"
        L"gcd\r\n"
        L"3\r\n"
        L"gcd";
    v.push_back(a);

    Macro b;
    b.name = L"一键爆发起手";
    b.trigger = {VK_F6, 0};
    b.mode = Mode::Once;
    b.script =
        L"# 饰品 + 爆发技能 + 两个主技能，按一次打完整套\r\n"
        L"key shift+1      # 饰品（不占公共冷却）\r\n"
        L"wait 80-150\r\n"
        L"key shift+2      # 爆发技能\r\n"
        L"gcd\r\n"
        L"repeat 2\r\n"
        L"  key 4\r\n"
        L"  gcd\r\n"
        L"end";
    v.push_back(b);

    Macro c;
    c.name = L"群体技能（按住）";
    c.trigger = {VK_XBUTTON2, 0};
    c.mode = Mode::Hold;
    c.script =
        L"# 按住鼠标侧键（前进键）持续打群体，松开立即停\r\n"
        L"5\r\n"
        L"gcd\r\n"
        L"6\r\n"
        L"gcd";
    v.push_back(c);

    Macro d;
    d.name = L"鼠标处放地面技能";
    d.trigger = {VK_XBUTTON1, 0};
    d.mode = Mode::Once;
    d.script =
        L"# 地面指示类技能：按技能键后在鼠标当前位置点左键确认\r\n"
        L"7\r\n"
        L"wait 120-200\r\n"
        L"click left";
    v.push_back(d);
    return v;
}
