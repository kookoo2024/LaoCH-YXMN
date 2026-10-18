// LaoCH 键位助手 —— 主界面
// 纯 Win32 + GDI+，无任何第三方依赖。深色主题、Per-Monitor V2 高 DPI、托盘常驻。
#include "common.h"
#include "keys.h"
#include "script.h"
#include "player.h"
#include "hook.h"
#include "config.h"
#include "resource.h"
#include <commctrl.h>
#include <shellapi.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <algorithm>
#include <cmath>
namespace Gdiplus { using std::min; using std::max; }
#include <gdiplus.h>
namespace G = Gdiplus;

#ifndef DWMWA_USE_IMMERSIVE_DARK_MODE
#define DWMWA_USE_IMMERSIVE_DARK_MODE 20
#endif
#ifndef DWMWA_BORDER_COLOR
#define DWMWA_BORDER_COLOR 34
#endif
#ifndef DWMWA_CAPTION_COLOR
#define DWMWA_CAPTION_COLOR 35
#endif

// ───────────────────────── 配色 ─────────────────────────
namespace C {
const COLORREF bg = RGB(17, 19, 24);
const COLORREF panel = RGB(22, 25, 32);
const COLORREF card = RGB(29, 33, 42);
const COLORREF input = RGB(14, 16, 21);
const COLORREF border = RGB(43, 48, 59);
const COLORREF borderHi = RGB(66, 73, 88);
const COLORREF text = RGB(231, 233, 238);
const COLORREF sub = RGB(139, 147, 166);
const COLORREF dim = RGB(88, 95, 111);
const COLORREF accent = RGB(232, 181, 77);
const COLORREF accentHi = RGB(245, 199, 106);
const COLORREF accentLo = RGB(201, 146, 52);
const COLORREF accentText = RGB(30, 23, 10);
const COLORREF ok = RGB(63, 185, 80);
const COLORREF danger = RGB(229, 83, 75);
const COLORREF hover = RGB(38, 43, 54);
const COLORREF sel = RGB(36, 41, 53);
const COLORREF segOn = RGB(45, 51, 64);
}  // namespace C

enum : int {
    IDC_LIST = 100, IDC_NEW, IDC_DUP, IDC_DEL, IDC_NAME, IDC_TRIG, IDC_REC, IDC_ONCE, IDC_LOOP, IDC_HOLD,
    IDC_EN, IDC_BLOCK, IDC_SCRIPT, IDC_GCD, IDC_JIT, IDC_WIN, IDC_TKEY, IDC_TREC, IDC_FG, IDC_HUMAN,
    IDC_SOUND, IDC_POWER,
    ID_TRAY_SHOW = 300, ID_TRAY_TOGGLE, ID_TRAY_EXIT,
};
enum : UINT_PTR { T_SAVE = 1, T_PARSE = 2 };

enum BtnKind { BK_PRIMARY, BK_GHOST, BK_DANGER, BK_SEG, BK_TOGGLE, BK_POWER };
struct BtnInfo {
    BtnKind kind;
    COLORREF bg;
    bool on = false;
    bool hover = false;
};

// ───────────────────────── 全局状态 ─────────────────────────
struct App {
    HINSTANCE hi = nullptr;
    HWND hwnd = nullptr;
    UINT dpi = 96;
    HFONT fUi = nullptr, fUiB = nullptr, fTitle = nullptr, fSmall = nullptr, fMono = nullptr, fMonoS = nullptr;
    int lineH = 18;
    HBRUSH brInput = nullptr, brPanel = nullptr;
    HICON icoBig = nullptr, icoSmall = nullptr;

    Settings st;
    std::vector<Macro> macros;
    int sel = -1;
    bool enabled = true;
    Player player;
    int runIdx = -1;

    HWND list, bNew, bDup, bDel, eName, eTrig, bRec, bSeg[3], tEn, tBlock, eScript;
    HWND eGcd, eJit, eWin, eTKey, bTRec, tFg, tHuman, tSound, bPower;

    RECT rcHeader{}, rcLeft{}, rcRight{}, rcStatus{}, rcListLbl{};
    RECT frName{}, frTrig{}, frSeg{}, frScript{}, cardHelp{}, cardSet{};
    RECT frGcd{}, frJit{}, frWin{}, frTKey{};
    int rx = 0, rw = 0, rowMode = 0, rowScript = 0;

    std::wstring status;
    COLORREF statusColor = C::sub;
    std::wstring scriptInfo;
    bool scriptOk = true;
    int captureFor = 0;  // 0 无 / 1 宏触发键 / 2 总开关
    bool loading = false;
    HWND focusEdit = nullptr;
    std::wstring cfgPath;
    bool portable = false;
    NOTIFYICONDATAW nid{};
    bool trayTipShown = false;
    UINT msgTaskbarCreated = 0;
} g;

static int S(int v) { return MulDiv(v, (int)g.dpi, 96); }

// ───────────────────────── 绘制工具 ─────────────────────────
static G::Color GC(COLORREF c, BYTE a = 255) { return G::Color(a, GetRValue(c), GetGValue(c), GetBValue(c)); }

static void RoundPath(G::GraphicsPath& p, float x, float y, float w, float h, float r) {
    float d = r * 2;
    if (d > w) d = w;
    if (d > h) d = h;
    if (d < 1) { p.AddRectangle(G::RectF(x, y, w, h)); return; }
    p.AddArc(x, y, d, d, 180, 90);
    p.AddArc(x + w - d, y, d, d, 270, 90);
    p.AddArc(x + w - d, y + h - d, d, d, 0, 90);
    p.AddArc(x, y + h - d, d, d, 90, 90);
    p.CloseFigure();
}

static void FillRR(G::Graphics& gr, const RECT& rc, float r, COLORREF c, BYTE a = 255) {
    G::GraphicsPath p;
    RoundPath(p, (float)rc.left, (float)rc.top, (float)(rc.right - rc.left), (float)(rc.bottom - rc.top), r);
    G::SolidBrush b(GC(c, a));
    gr.FillPath(&b, &p);
}

static void StrokeRR(G::Graphics& gr, const RECT& rc, float r, COLORREF c, float w = 1.f) {
    G::GraphicsPath p;
    float h = w / 2;
    RoundPath(p, rc.left + h, rc.top + h, rc.right - rc.left - w, rc.bottom - rc.top - w, r);
    G::Pen pen(GC(c), w);
    gr.DrawPath(&pen, &p);
}

static void Dot(G::Graphics& gr, float cx, float cy, float r, COLORREF c, BYTE a = 255) {
    G::SolidBrush b(GC(c, a));
    gr.FillEllipse(&b, cx - r, cy - r, r * 2, r * 2);
}

static void Txt(HDC dc, const std::wstring& s, RECT rc, HFONT f, COLORREF c, UINT fmt) {
    HGDIOBJ o = SelectObject(dc, f);
    SetTextColor(dc, c);
    SetBkMode(dc, TRANSPARENT);
    DrawTextW(dc, s.c_str(), (int)s.size(), &rc, fmt | DT_NOPREFIX);
    SelectObject(dc, o);
}

static int TxtW(HDC dc, const std::wstring& s, HFONT f) {
    HGDIOBJ o = SelectObject(dc, f);
    SIZE sz{};
    GetTextExtentPoint32W(dc, s.c_str(), (int)s.size(), &sz);
    SelectObject(dc, o);
    return sz.cx;
}

static std::wstring GetText(HWND h) {
    int n = GetWindowTextLengthW(h);
    std::wstring s(n, L'\0');
    if (n) GetWindowTextW(h, &s[0], n + 1);
    return s;
}

static BtnInfo* Info(HWND h) { return reinterpret_cast<BtnInfo*>(GetWindowLongPtrW(h, GWLP_USERDATA)); }
static void SetOn(HWND h, bool on) {
    if (BtnInfo* b = Info(h)) { b->on = on; InvalidateRect(h, nullptr, FALSE); }
}

// ───────────────────────── 暗色模式 ─────────────────────────
static void InitDarkMode() {
    HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
    if (!ux) ux = LoadLibraryW(L"uxtheme.dll");
    if (!ux) return;
    using SetModeFn = int(WINAPI*)(int);
    using FlushFn = void(WINAPI*)();
    if (auto f = (SetModeFn)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(135))) f(2);  // 强制深色
    if (auto fl = (FlushFn)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(136))) fl();
}

static void DarkControl(HWND h) {
    HMODULE ux = GetModuleHandleW(L"uxtheme.dll");
    using AllowFn = BOOL(WINAPI*)(HWND, BOOL);
    if (ux)
        if (auto f = (AllowFn)(void*)GetProcAddress(ux, MAKEINTRESOURCEA(133))) f(h, TRUE);
    SetWindowTheme(h, L"DarkMode_Explorer", nullptr);
}

// ───────────────────────── 字体 ─────────────────────────
static HFONT MkFont(int px, int weight, const wchar_t* face) {
    return CreateFontW(-px, 0, 0, 0, weight, 0, 0, 0, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
                       CLEARTYPE_QUALITY, DEFAULT_PITCH, face);
}

static void CreateFonts() {
    HFONT old[] = {g.fUi, g.fUiB, g.fTitle, g.fSmall, g.fMono, g.fMonoS};
    g.fUi = MkFont(S(14), FW_NORMAL, L"Microsoft YaHei UI");
    g.fUiB = MkFont(S(14), FW_SEMIBOLD, L"Microsoft YaHei UI");
    g.fTitle = MkFont(S(18), FW_BOLD, L"Microsoft YaHei UI");
    g.fSmall = MkFont(S(12), FW_NORMAL, L"Microsoft YaHei UI");
    g.fMono = MkFont(S(15), FW_NORMAL, L"Consolas");
    g.fMonoS = MkFont(S(13), FW_NORMAL, L"Consolas");
    for (HFONT f : old)
        if (f) DeleteObject(f);
    HDC dc = GetDC(nullptr);
    HGDIOBJ o = SelectObject(dc, g.fUi);
    TEXTMETRICW tm{};
    GetTextMetricsW(dc, &tm);
    SelectObject(dc, o);
    ReleaseDC(nullptr, dc);
    g.lineH = tm.tmHeight;
}

static void ApplyFonts() {
    HWND edits[] = {g.eName, g.eTrig, g.eGcd, g.eJit, g.eWin, g.eTKey};
    for (HWND e : edits) {
        SendMessageW(e, WM_SETFONT, (WPARAM)g.fUi, TRUE);
        SendMessageW(e, EM_SETMARGINS, EC_LEFTMARGIN | EC_RIGHTMARGIN, MAKELPARAM(S(2), S(2)));
    }
    SendMessageW(g.eScript, WM_SETFONT, (WPARAM)g.fMono, TRUE);
    SendMessageW(g.list, LB_SETITEMHEIGHT, 0, S(56));
}

// ───────────────────────── 状态 / 托盘 ─────────────────────────
static void TrayUpdate() {
    std::wstring tip = std::wstring(APP_TITLE) + L"\n";
    if (!g.enabled) tip += L"已暂停";
    else if (g.player.Running() && g.runIdx >= 0 && g.runIdx < (int)g.macros.size())
        tip += L"运行中：" + g.macros[g.runIdx].name;
    else tip += L"待命中";
    lstrcpynW(g.nid.szTip, tip.c_str(), ARRAYSIZE(g.nid.szTip));
    g.nid.uFlags = NIF_TIP;
    Shell_NotifyIconW(NIM_MODIFY, &g.nid);
}

static void TrayAdd() {
    g.nid.cbSize = sizeof(g.nid);
    g.nid.hWnd = g.hwnd;
    g.nid.uID = 1;
    g.nid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    g.nid.uCallbackMessage = WM_APP_TRAY;
    g.nid.hIcon = g.icoSmall;
    lstrcpynW(g.nid.szTip, APP_TITLE, ARRAYSIZE(g.nid.szTip));
    Shell_NotifyIconW(NIM_ADD, &g.nid);
    TrayUpdate();
}

static void TrayBalloon(const wchar_t* title, const wchar_t* text) {
    g.nid.uFlags = NIF_INFO;
    lstrcpynW(g.nid.szInfoTitle, title, ARRAYSIZE(g.nid.szInfoTitle));
    lstrcpynW(g.nid.szInfo, text, ARRAYSIZE(g.nid.szInfo));
    g.nid.dwInfoFlags = NIIF_USER | NIIF_NOSOUND;
    g.nid.hBalloonIcon = g.icoBig;
    Shell_NotifyIconW(NIM_MODIFY, &g.nid);
}

static void SetStatus(const std::wstring& s, COLORREF c = C::sub) {
    g.status = s;
    g.statusColor = c;
    InvalidateRect(g.hwnd, &g.rcStatus, FALSE);
}

static void RefreshRunState() {
    InvalidateRect(g.hwnd, &g.rcHeader, FALSE);
    InvalidateRect(g.list, nullptr, FALSE);
    TrayUpdate();
}

// ───────────────────────── 数据与钩子同步 ─────────────────────────
static void MarkDirty() { SetTimer(g.hwnd, T_SAVE, 600, nullptr); }

static void SaveNow() {
    KillTimer(g.hwnd, T_SAVE);
    if (!SaveConfig(g.cfgPath, g.st, g.macros)) SetStatus(L"⚠ 配置保存失败：" + g.cfgPath, C::danger);
}

static void ApplyHook() {
    HookConfig c;
    c.enabled = g.enabled;
    c.fgOnly = g.st.foregroundOnly;
    c.patterns = ParsePatterns(g.st.windowMatch);
    c.toggle = g.st.toggleKey;
    for (int i = 0; i < (int)g.macros.size(); ++i) {
        const Macro& m = g.macros[i];
        if (m.enabled && !m.trigger.Empty() && m.trigger != g.st.toggleKey) c.triggers.push_back({m.trigger, i, m.block});
    }
    hook::Configure(c);
}

static bool HasConflict(int i) {
    const Macro& m = g.macros[i];
    if (!m.enabled || m.trigger.Empty()) return false;
    if (m.trigger == g.st.toggleKey) return true;
    for (int j = 0; j < (int)g.macros.size(); ++j)
        if (j != i && g.macros[j].enabled && g.macros[j].trigger == m.trigger) return true;
    return false;
}

static void ParseCurrent() {
    if (g.sel < 0) { g.scriptInfo.clear(); return; }
    Program p;
    ParseError e;
    g.scriptOk = ParseScript(g.macros[g.sel].script, p, e);
    if (g.scriptOk) {
        g.scriptInfo = L"✓ 语法正确 · " + std::to_wstring(p.actions) + L" 个动作";
    } else {
        g.scriptInfo = (e.line > 0 ? L"第 " + std::to_wstring(e.line) + L" 行：" : std::wstring()) + e.msg;
    }
    RECT r = {g.rx, g.rowScript, g.rx + g.rw, g.rowScript + S(22)};
    InvalidateRect(g.hwnd, &r, FALSE);
}

// ───────────────────────── 运行控制 ─────────────────────────
static void StartMacro(int i) {
    Macro& m = g.macros[i];
    Program prog;
    ParseError e;
    if (!ParseScript(m.script, prog, e)) {
        SetStatus(L"⚠「" + m.name + L"」脚本有误" + (e.line ? L"（第 " + std::to_wstring(e.line) + L" 行）" : L"") +
                      L"：" + e.msg, C::danger);
        return;
    }
    PlayParams pp;
    pp.prog = std::move(prog);
    pp.mode = m.mode;
    pp.gcdMs = g.st.gcdMs;
    pp.jitterPct = g.st.jitterPct;
    pp.humanize = g.st.humanize;
    pp.fgOnly = g.st.foregroundOnly;
    pp.patterns = ParsePatterns(g.st.windowMatch);
    if (g.player.Start(g.hwnd, pp) == 0) {
        SetStatus(L"⚠ 无法启动执行线程", C::danger);
        return;
    }
    g.runIdx = i;
    SetStatus(L"▶ 正在运行「" + m.name + L"」", C::ok);
    RefreshRunState();
}

static void SetPowerText() {
    SetWindowTextW(g.bPower, g.enabled ? L"● 已启用" : L"已暂停");
    SetOn(g.bPower, g.enabled);
}

static void SetEnabled(bool on, bool fromHotkey) {
    g.enabled = on;
    if (!on) g.player.RequestStop();
    ApplyHook();
    SetPowerText();
    std::wstring key = g.st.toggleKey.Empty() ? L"" : ComboToString(g.st.toggleKey);
    if (on) SetStatus(L"已启用 · 进入游戏后按触发键即可", C::ok);
    else SetStatus(L"已暂停 · 所有触发键原样交给游戏" + (key.empty() ? L"" : L"，按 " + key + L" 恢复"), C::accent);
    if (fromHotkey && g.st.sound) MessageBeep(on ? MB_ICONASTERISK : MB_ICONEXCLAMATION);
    RefreshRunState();
}

// ───────────────────────── 界面 <-> 数据 ─────────────────────────
static void UpdateTriggerText() {
    if (g.captureFor == 1) return;
    std::wstring t = g.sel >= 0 ? ComboToString(g.macros[g.sel].trigger) : L"";
    SetWindowTextW(g.eTrig, t.empty() ? L"未设置" : t.c_str());
}

static void UpdateToggleKeyText() {
    if (g.captureFor == 2) return;
    std::wstring t = ComboToString(g.st.toggleKey);
    SetWindowTextW(g.eTKey, t.empty() ? L"未设置" : t.c_str());
}

static void RefreshList() {
    SendMessageW(g.list, WM_SETREDRAW, FALSE, 0);
    SendMessageW(g.list, LB_RESETCONTENT, 0, 0);
    for (size_t i = 0; i < g.macros.size(); ++i) SendMessageW(g.list, LB_ADDSTRING, 0, (LPARAM)i);
    SendMessageW(g.list, LB_SETCURSEL, (WPARAM)g.sel, 0);
    SendMessageW(g.list, WM_SETREDRAW, TRUE, 0);
    InvalidateRect(g.list, nullptr, TRUE);
    InvalidateRect(g.hwnd, &g.rcListLbl, FALSE);
}

static void SetSeg(Mode m) {
    for (int i = 0; i < 3; ++i) SetOn(g.bSeg[i], (int)m == i);
}

static void LoadMacroToUi() {
    g.loading = true;
    bool has = g.sel >= 0 && g.sel < (int)g.macros.size();
    HWND right[] = {g.eName, g.eTrig, g.bRec, g.bSeg[0], g.bSeg[1], g.bSeg[2], g.tEn, g.tBlock, g.eScript};
    for (HWND h : right) ShowWindow(h, has ? SW_SHOW : SW_HIDE);
    EnableWindow(g.bDup, has);
    EnableWindow(g.bDel, has);
    if (has) {
        const Macro& m = g.macros[g.sel];
        SetWindowTextW(g.eName, m.name.c_str());
        UpdateTriggerText();
        SetSeg(m.mode);
        SetOn(g.tEn, m.enabled);
        SetOn(g.tBlock, m.block);
        SetWindowTextW(g.eScript, m.script.c_str());
    }
    g.loading = false;
    ParseCurrent();
    InvalidateRect(g.hwnd, nullptr, FALSE);
}

static void LoadSettingsToUi() {
    g.loading = true;
    SetWindowTextW(g.eGcd, std::to_wstring(g.st.gcdMs).c_str());
    SetWindowTextW(g.eJit, std::to_wstring(g.st.jitterPct).c_str());
    SetWindowTextW(g.eWin, g.st.windowMatch.c_str());
    UpdateToggleKeyText();
    SetOn(g.tFg, g.st.foregroundOnly);
    SetOn(g.tHuman, g.st.humanize);
    SetOn(g.tSound, g.st.sound);
    g.loading = false;
}

static void Select(int i) {
    g.sel = i;
    SendMessageW(g.list, LB_SETCURSEL, (WPARAM)i, 0);
    LoadMacroToUi();
}

// ───────────────────────── 录制按键 ─────────────────────────
static void EndCapture() {
    hook::SetCapture(false);
    g.captureFor = 0;
    SetWindowTextW(g.bRec, L"录制");
    SetWindowTextW(g.bTRec, L"录制");
    UpdateTriggerText();
    UpdateToggleKeyText();
}

static void BeginCapture(int which) {
    if (g.captureFor) {
        bool same = g.captureFor == which;
        EndCapture();
        SetStatus(L"已取消录制");
        if (same) return;
    }
    g.captureFor = which;
    hook::SetCapture(true);
    SetWindowTextW(which == 1 ? g.eTrig : g.eTKey, L"请按下按键…");
    SetWindowTextW(which == 1 ? g.bRec : g.bTRec, L"取消");
    SetStatus(L"录制中：按下键盘按键（可带 Ctrl/Shift/Alt）或鼠标中键/侧键 · Esc 取消 · Backspace 清除", C::accent);
}

static void OnCaptured(UINT vk, UINT mods) {
    int which = g.captureFor;
    if (!which) return;
    EndCapture();
    Combo c{vk, mods};
    if (vk == VK_ESCAPE && mods == 0) {
        SetStatus(L"已取消录制");
        return;
    }
    if (vk == VK_BACK && mods == 0) c = Combo{};
    if (which == 1 && g.sel >= 0) g.macros[g.sel].trigger = c;
    else if (which == 2) g.st.toggleKey = c;
    UpdateTriggerText();
    UpdateToggleKeyText();
    ApplyHook();
    InvalidateRect(g.list, nullptr, FALSE);
    MarkDirty();
    if (c.Empty()) { SetStatus(L"已清除按键"); return; }
    std::wstring name = ComboToString(c);
    if (which == 1 && HasConflict(g.sel))
        SetStatus(L"⚠ " + name + L" 已被其他宏或总开关占用，请换一个", C::danger);
    else
        SetStatus((which == 1 ? L"触发键已设为 " : L"总开关热键已设为 ") + name, C::ok);
}

// ───────────────────────── 控件子类 ─────────────────────────
static LRESULT CALLBACK BtnProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    BtnInfo* bi = Info(h);
    switch (m) {
        case WM_MOUSEMOVE:
            if (bi && !bi->hover) {
                bi->hover = true;
                TRACKMOUSEEVENT t{sizeof(t), TME_LEAVE, h, 0};
                TrackMouseEvent(&t);
                InvalidateRect(h, nullptr, FALSE);
            }
            break;
        case WM_MOUSELEAVE:
            if (bi) { bi->hover = false; InvalidateRect(h, nullptr, FALSE); }
            break;
        case WM_SETCURSOR:
            SetCursor(LoadCursorW(nullptr, IDC_HAND));
            return TRUE;
        case WM_ERASEBKGND:
            return 1;
        case WM_NCDESTROY:
            RemoveWindowSubclass(h, BtnProc, 1);
            delete bi;
            SetWindowLongPtrW(h, GWLP_USERDATA, 0);
            break;
    }
    return DefSubclassProc(h, m, w, l);
}

static LRESULT CALLBACK EditProc(HWND h, UINT m, WPARAM w, LPARAM l, UINT_PTR, DWORD_PTR) {
    switch (m) {
        case WM_KEYDOWN:
            if (w == 'A' && (GetKeyState(VK_CONTROL) & 0x8000)) {
                SendMessageW(h, EM_SETSEL, 0, -1);
                return 0;
            }
            break;
        case WM_CHAR:
            if (w == 1) return 0;  // 吞掉 Ctrl+A 的提示音
            if (w == L'\t' && h == g.eScript) {
                SendMessageW(h, EM_REPLACESEL, TRUE, (LPARAM)L"  ");
                return 0;
            }
            break;
        case WM_NCDESTROY:
            RemoveWindowSubclass(h, EditProc, 2);
            break;
    }
    return DefSubclassProc(h, m, w, l);
}

static HWND MkBtn(const wchar_t* t, int id, BtnKind k, COLORREF bg) {
    HWND h = CreateWindowExW(0, L"BUTTON", t, WS_CHILD | WS_VISIBLE | BS_OWNERDRAW, 0, 0, 10, 10, g.hwnd,
                             (HMENU)(INT_PTR)id, g.hi, nullptr);
    BtnInfo* bi = new BtnInfo{k, bg};
    SetWindowLongPtrW(h, GWLP_USERDATA, (LONG_PTR)bi);
    SetWindowSubclass(h, BtnProc, 1, 0);
    return h;
}

static HWND MkEdit(int id, DWORD extra) {
    HWND h = CreateWindowExW(0, L"EDIT", L"", WS_CHILD | WS_VISIBLE | extra, 0, 0, 10, 10, g.hwnd,
                             (HMENU)(INT_PTR)id, g.hi, nullptr);
    SetWindowSubclass(h, EditProc, 2, 0);
    return h;
}

static void CreateControls() {
    g.list = CreateWindowExW(0, L"LISTBOX", L"",
                             WS_CHILD | WS_VISIBLE | WS_VSCROLL | LBS_OWNERDRAWFIXED | LBS_NOTIFY | LBS_NOINTEGRALHEIGHT,
                             0, 0, 10, 10, g.hwnd, (HMENU)(INT_PTR)IDC_LIST, g.hi, nullptr);
    DarkControl(g.list);
    g.bNew = MkBtn(L"＋ 新建", IDC_NEW, BK_PRIMARY, C::panel);
    g.bDup = MkBtn(L"复制", IDC_DUP, BK_GHOST, C::panel);
    g.bDel = MkBtn(L"删除", IDC_DEL, BK_DANGER, C::panel);

    g.eName = MkEdit(IDC_NAME, ES_AUTOHSCROLL);
    SendMessageW(g.eName, EM_LIMITTEXT, 40, 0);
    g.eTrig = MkEdit(IDC_TRIG, ES_AUTOHSCROLL | ES_READONLY);
    g.bRec = MkBtn(L"录制", IDC_REC, BK_GHOST, C::bg);
    g.bSeg[0] = MkBtn(L"单次", IDC_ONCE, BK_SEG, C::input);
    g.bSeg[1] = MkBtn(L"循环", IDC_LOOP, BK_SEG, C::input);
    g.bSeg[2] = MkBtn(L"按住", IDC_HOLD, BK_SEG, C::input);
    g.tEn = MkBtn(L"启用", IDC_EN, BK_TOGGLE, C::bg);
    g.tBlock = MkBtn(L"拦截触发键", IDC_BLOCK, BK_TOGGLE, C::bg);
    g.eScript = MkEdit(IDC_SCRIPT, ES_MULTILINE | ES_AUTOVSCROLL | ES_WANTRETURN | WS_VSCROLL | ES_NOHIDESEL);
    SendMessageW(g.eScript, EM_LIMITTEXT, 65535, 0);
    DarkControl(g.eScript);

    g.eGcd = MkEdit(IDC_GCD, ES_AUTOHSCROLL | ES_NUMBER);
    SendMessageW(g.eGcd, EM_LIMITTEXT, 5, 0);
    g.eJit = MkEdit(IDC_JIT, ES_AUTOHSCROLL | ES_NUMBER);
    SendMessageW(g.eJit, EM_LIMITTEXT, 2, 0);
    g.eWin = MkEdit(IDC_WIN, ES_AUTOHSCROLL);
    g.eTKey = MkEdit(IDC_TKEY, ES_AUTOHSCROLL | ES_READONLY);
    g.bTRec = MkBtn(L"录制", IDC_TREC, BK_GHOST, C::card);
    g.tFg = MkBtn(L"仅游戏窗口", IDC_FG, BK_TOGGLE, C::card);
    g.tHuman = MkBtn(L"拟人化", IDC_HUMAN, BK_TOGGLE, C::card);
    g.tSound = MkBtn(L"提示音", IDC_SOUND, BK_TOGGLE, C::card);
    g.bPower = MkBtn(L"● 已启用", IDC_POWER, BK_POWER, C::panel);
}

// ───────────────────────── 布局 ─────────────────────────
static void Move(HWND h, int x, int y, int w, int hgt) {
    SetWindowPos(h, nullptr, x, y, std::max(0, w), std::max(0, hgt), SWP_NOZORDER | SWP_NOACTIVATE);
}

static void PlaceEdit(HWND e, const RECT& fr) {
    int h = g.lineH + S(2);
    Move(e, fr.left + S(10), fr.top + (fr.bottom - fr.top - h) / 2, fr.right - fr.left - S(20), h);
}

static void Layout() {
    RECT rc;
    GetClientRect(g.hwnd, &rc);
    int W = rc.right, H = rc.bottom;
    int hh = S(64), sb = S(30), lw = S(290), pad = S(18);
    g.rcHeader = {0, 0, W, hh};
    g.rcStatus = {0, H - sb, W, H};
    g.rcLeft = {0, hh, lw, H - sb};
    g.rcRight = {lw, hh, W, H - sb};

    Move(g.bPower, W - pad - S(128), (hh - S(36)) / 2, S(128), S(36));

    // ── 左侧：全局设置卡片
    int cx = pad, cw = lw - 2 * pad;
    int cardH = S(40) + S(58) * 3 + S(32) * 2 + S(12);
    g.cardSet = {cx, H - sb - pad - cardH, cx + cw, H - sb - pad};
    int ix = cx + S(14), iw = cw - S(28), y = g.cardSet.top + S(40);
    int half = (iw - S(10)) / 2;
    g.frGcd = {ix, y + S(20), ix + half, y + S(52)};
    g.frJit = {ix + half + S(10), y + S(20), ix + iw, y + S(52)};
    y += S(58);
    g.frWin = {ix, y + S(20), ix + iw, y + S(52)};
    y += S(58);
    g.frTKey = {ix, y + S(20), ix + iw - S(68), y + S(52)};
    Move(g.bTRec, ix + iw - S(60), y + S(20), S(60), S(32));
    y += S(60);
    Move(g.tFg, ix, y, half + S(6), S(28));
    Move(g.tHuman, ix + half + S(10), y, iw - half - S(10), S(28));
    y += S(32);
    Move(g.tSound, ix, y, half + S(6), S(28));
    PlaceEdit(g.eGcd, g.frGcd);
    PlaceEdit(g.eJit, g.frJit);
    PlaceEdit(g.eWin, g.frWin);
    PlaceEdit(g.eTKey, g.frTKey);

    // ── 左侧：宏列表
    g.rcListLbl = {cx, hh + S(14), cx + cw, hh + S(40)};
    int by = g.cardSet.top - S(14) - S(34);
    int ly = hh + S(44);
    Move(g.list, cx, ly, cw, by - S(10) - ly);
    int bw = (cw - S(16)) / 3;
    Move(g.bNew, cx, by, bw, S(34));
    Move(g.bDup, cx + bw + S(8), by, bw, S(34));
    Move(g.bDel, cx + 2 * (bw + S(8)), by, cw - 2 * (bw + S(8)), S(34));

    // ── 右侧：宏编辑
    g.rx = lw + S(26);
    g.rw = W - g.rx - S(26);
    int rx = g.rx, rw = g.rw;
    y = hh + S(18);
    int nameW = (rw - S(16)) * 55 / 100;
    g.frName = {rx, y + S(22), rx + nameW, y + S(58)};
    int tx = rx + nameW + S(16);
    g.frTrig = {tx, y + S(22), rx + rw - S(82), y + S(58)};
    Move(g.bRec, rx + rw - S(74), y + S(22), S(74), S(36));
    PlaceEdit(g.eName, g.frName);
    PlaceEdit(g.eTrig, g.frTrig);

    y += S(76);
    g.rowMode = y;
    int segW = S(270);
    g.frSeg = {rx, y + S(22), rx + segW, y + S(58)};
    int sw = (segW - S(8)) / 3;
    for (int i = 0; i < 3; ++i) Move(g.bSeg[i], rx + S(4) + i * sw, y + S(26), i == 2 ? segW - S(8) - 2 * sw : sw, S(28));
    Move(g.tEn, rx + segW + S(24), y + S(26), S(84), S(28));
    Move(g.tBlock, rx + segW + S(116), y + S(26), S(130), S(28));

    y += S(76);
    g.rowScript = y;
    int helpW = rw > S(560) ? S(262) : S(220);
    g.frScript = {rx, y + S(24), rx + rw - helpW - S(16), H - sb - S(20)};
    g.cardHelp = {rx + rw - helpW, y + S(24), rx + rw, H - sb - S(20)};
    RECT s = g.frScript;
    Move(g.eScript, s.left + S(12), s.top + S(10), s.right - s.left - S(16), s.bottom - s.top - S(18));
    RECT er = {0, 0, s.right - s.left - S(16) - GetSystemMetricsForDpi(SM_CXVSCROLL, g.dpi), s.bottom - s.top - S(18)};
    SendMessageW(g.eScript, EM_SETRECT, 0, (LPARAM)&er);

    InvalidateRect(g.hwnd, nullptr, FALSE);
}

// ───────────────────────── 主窗口绘制 ─────────────────────────
struct HelpRow { const wchar_t* cmd; const wchar_t* desc; };
static const HelpRow kHelp[] = {
    {L"1 / shift+2", L"按一下按键"},
    {L"key 3 x2", L"连按 2 次"},
    {L"hold shift", L"按住（release 松开）"},
    {L"wait 800", L"等待毫秒，自动抖动"},
    {L"wait 300-500", L"区间内随机等待"},
    {L"gcd", L"等一个公共冷却"},
    {L"click right", L"鼠标点击 left/right"},
    {L"dblclick", L"双击"},
    {L"move 960 540", L"拟人轨迹移到坐标"},
    {L"moverel 40 0", L"相对移动（转视角）"},
    {L"scroll -2", L"滚轮，负数向下"},
    {L"repeat 3 … end", L"重复一段动作"},
    {L"# 文字", L"注释"},
};

static void DrawFrame(G::Graphics& gr, const RECT& fr, HWND edit) {
    FillRR(gr, fr, (float)S(7), C::input);
    StrokeRR(gr, fr, (float)S(7), g.focusEdit == edit && edit ? C::accent : C::border);
}

static void Label(HDC dc, const RECT& fr, const std::wstring& t) {
    RECT r = {fr.left, fr.top - S(22), fr.right + S(400), fr.top - S(4)};
    Txt(dc, t, r, g.fSmall, C::sub, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
}

static void PaintMain(HDC dc, const RECT& rc) {
    bool has = g.sel >= 0 && g.sel < (int)g.macros.size();
    int hh = g.rcHeader.bottom;
    std::wstring pill;
    COLORREF pillDot;
    if (!g.enabled) {
        std::wstring k = ComboToString(g.st.toggleKey);
        pill = k.empty() ? L"已暂停" : L"已暂停 · 按 " + k + L" 恢复";
        pillDot = C::danger;
    } else if (g.player.Running() && g.runIdx >= 0 && g.runIdx < (int)g.macros.size()) {
        pill = L"运行中 · " + g.macros[g.runIdx].name;
        pillDot = C::ok;
    } else {
        pill = L"待命中";
        pillDot = C::accent;
    }
    int pillW = TxtW(dc, pill, g.fUi) + S(40);
    RECT rPill;
    {
        RECT pb;
        GetWindowRect(g.bPower, &pb);
        MapWindowPoints(nullptr, g.hwnd, (POINT*)&pb, 2);
        rPill = {pb.left - S(14) - pillW, (hh - S(32)) / 2, pb.left - S(14), (hh + S(32)) / 2};
    }
    RECT logo = {S(20), (hh - S(36)) / 2, S(56), (hh + S(36)) / 2};

    {
        G::Graphics gr(dc);
        gr.SetSmoothingMode(G::SmoothingModeNone);
        G::SolidBrush bBg(GC(C::bg)), bPanel(GC(C::panel));
        gr.FillRectangle(&bBg, 0, 0, (INT)rc.right, (INT)rc.bottom);
        gr.FillRectangle(&bPanel, 0, 0, (INT)rc.right, hh);
        gr.FillRectangle(&bPanel, (INT)g.rcLeft.left, (INT)g.rcLeft.top, (INT)(g.rcLeft.right - g.rcLeft.left),
                         (INT)(g.rcLeft.bottom - g.rcLeft.top));
        gr.FillRectangle(&bPanel, 0, (INT)g.rcStatus.top, (INT)rc.right, (INT)(g.rcStatus.bottom - g.rcStatus.top));
        G::Pen line(GC(C::border), 1.f);
        gr.DrawLine(&line, 0, hh - 1, (INT)rc.right, hh - 1);
        gr.DrawLine(&line, (INT)g.rcLeft.right - 1, hh, (INT)g.rcLeft.right - 1, (INT)g.rcStatus.top);
        gr.DrawLine(&line, 0, (INT)g.rcStatus.top, (INT)rc.right, (INT)g.rcStatus.top);

        gr.SetSmoothingMode(G::SmoothingModeAntiAlias);
        // 标志：金色键帽 + 播放符号
        {
            G::GraphicsPath p;
            RoundPath(p, (float)logo.left, (float)logo.top, (float)S(36), (float)S(36), (float)S(10));
            G::LinearGradientBrush lg(G::PointF((float)logo.left, (float)logo.top),
                                      G::PointF((float)logo.left, (float)logo.bottom), GC(C::accentHi), GC(C::accentLo));
            gr.FillPath(&lg, &p);
            RECT in = {logo.left + S(8), logo.top + S(8), logo.right - S(8), logo.bottom - S(8)};
            StrokeRR(gr, in, (float)S(4), C::accentText, (float)S(2));
            float cx = (logo.left + logo.right) / 2.f + S(1), cy = (logo.top + logo.bottom) / 2.f, r = (float)S(5);
            G::PointF tri[3] = {G::PointF(cx - r * 0.7f, cy - r), G::PointF(cx - r * 0.7f, cy + r), G::PointF(cx + r, cy)};
            G::SolidBrush tb(GC(C::accentText));
            gr.FillPolygon(&tb, tri, 3);
        }
        FillRR(gr, rPill, (float)S(16), C::card);
        StrokeRR(gr, rPill, (float)S(16), C::border);
        Dot(gr, (float)rPill.left + S(16), (rPill.top + rPill.bottom) / 2.f, (float)S(4), pillDot);
        Dot(gr, (float)rPill.left + S(16), (rPill.top + rPill.bottom) / 2.f, (float)S(8), pillDot, 40);

        // 全局设置卡片
        FillRR(gr, g.cardSet, (float)S(10), C::card);
        StrokeRR(gr, g.cardSet, (float)S(10), C::border);
        DrawFrame(gr, g.frGcd, g.eGcd);
        DrawFrame(gr, g.frJit, g.eJit);
        DrawFrame(gr, g.frWin, g.eWin);
        DrawFrame(gr, g.frTKey, g.captureFor == 2 ? g.eTKey : nullptr);
        if (g.captureFor == 2) StrokeRR(gr, g.frTKey, (float)S(7), C::accent);

        if (has) {
            DrawFrame(gr, g.frName, g.eName);
            DrawFrame(gr, g.frTrig, nullptr);
            if (g.captureFor == 1) StrokeRR(gr, g.frTrig, (float)S(7), C::accent);
            FillRR(gr, g.frSeg, (float)S(8), C::input);
            StrokeRR(gr, g.frSeg, (float)S(8), C::border);
            DrawFrame(gr, g.frScript, g.eScript);
            FillRR(gr, g.cardHelp, (float)S(10), C::card);
            StrokeRR(gr, g.cardHelp, (float)S(10), C::border);
        }
    }

    // ── 文字
    RECT r = {S(68), hh / 2 - S(21), S(420), hh / 2 + S(2)};
    Txt(dc, APP_TITLE, r, g.fTitle, C::text, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
    r = {S(69), hh / 2 + S(2), S(420), hh / 2 + S(20)};
    Txt(dc, L"魔兽世界 · 拟人化按键宏", r, g.fSmall, C::sub, DT_LEFT | DT_TOP | DT_SINGLELINE);
    r = {rPill.left + S(30), rPill.top, rPill.right - S(8), rPill.bottom};
    Txt(dc, pill, r, g.fUi, C::text, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);

    Txt(dc, L"我的宏", g.rcListLbl, g.fUiB, C::text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    Txt(dc, std::to_wstring(g.macros.size()) + L" 个", g.rcListLbl, g.fSmall, C::sub, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);

    r = {g.cardSet.left + S(14), g.cardSet.top + S(10), g.cardSet.right - S(14), g.cardSet.top + S(34)};
    Txt(dc, L"全局设置", r, g.fUiB, C::text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
    Label(dc, g.frGcd, L"公共冷却（毫秒）");
    Label(dc, g.frJit, L"随机抖动（%）");
    Label(dc, g.frWin, L"游戏窗口标题（用 | 分隔）");
    Label(dc, g.frTKey, L"总开关热键");

    if (has) {
        Label(dc, g.frName, L"宏名称");
        Label(dc, g.frTrig, L"触发键");
        static const wchar_t* desc[] = {L"按一次触发键，完整执行一遍；执行中再按一次可中止",
                                        L"按一次开始循环，再按一次停止",
                                        L"按住触发键时循环执行，松开立即停止"};
        const Macro& m = g.macros[g.sel];
        RECT lr = {g.rx, g.rowMode, g.rx + g.rw, g.rowMode + S(18)};
        Txt(dc, L"运行模式", lr, g.fSmall, C::sub, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
        lr.left += TxtW(dc, L"运行模式", g.fSmall) + S(10);
        Txt(dc, std::wstring(L"·  ") + desc[(int)m.mode], lr, g.fSmall, C::dim, DT_LEFT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT sr = {g.rx, g.rowScript, g.frScript.right, g.rowScript + S(20)};
        Txt(dc, L"动作脚本", sr, g.fSmall, C::sub, DT_LEFT | DT_BOTTOM | DT_SINGLELINE);
        sr.left += TxtW(dc, L"动作脚本", g.fSmall) + S(16);
        Txt(dc, g.scriptInfo, sr, g.fSmall, g.scriptOk ? C::ok : C::danger,
            DT_RIGHT | DT_BOTTOM | DT_SINGLELINE | DT_END_ELLIPSIS);

        RECT hc = g.cardHelp;
        r = {hc.left + S(16), hc.top + S(12), hc.right - S(12), hc.top + S(36)};
        Txt(dc, L"语法速查", r, g.fUiB, C::text, DT_LEFT | DT_VCENTER | DT_SINGLELINE);
        int y = hc.top + S(44), rowH = S(23);
        int colW = (hc.right - hc.left) * 46 / 100;
        for (const auto& row : kHelp) {
            if (y + rowH > hc.bottom - S(40)) break;
            RECT a = {hc.left + S(16), y, hc.left + S(10) + colW, y + rowH};
            Txt(dc, row.cmd, a, g.fMonoS, C::accent, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            RECT b = {hc.left + S(16) + colW, y, hc.right - S(10), y + rowH};
            Txt(dc, row.desc, b, g.fSmall, C::sub, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
            y += rowH;
        }
        r = {hc.left + S(16), hc.bottom - S(36), hc.right - S(12), hc.bottom - S(10)};
        Txt(dc, L"每行一个动作 · 修改自动保存", r, g.fSmall, C::dim, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    } else {
        r = g.rcRight;
        Txt(dc, L"还没有宏。点左侧「＋ 新建」开始吧。", r, g.fUi, C::sub, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 状态栏
    r = {S(16), g.rcStatus.top, g.rcStatus.right - S(260), g.rcStatus.bottom};
    Txt(dc, g.status, r, g.fSmall, g.statusColor, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    r = {g.rcStatus.right - S(300), g.rcStatus.top, g.rcStatus.right - S(16), g.rcStatus.bottom};
    std::wstring right = std::wstring(g.portable ? L"便携模式" : L"配置保存在 AppData") + L"  ·  v" + APP_VERSION;
    Txt(dc, right, r, g.fSmall, C::dim, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
}

// ───────────────────────── 自绘按钮 / 列表项 ─────────────────────────
static void DrawButton(const DRAWITEMSTRUCT* d) {
    BtnInfo* bi = Info(d->hwndItem);
    if (!bi) return;
    int w = d->rcItem.right - d->rcItem.left, h = d->rcItem.bottom - d->rcItem.top;
    HDC mem = CreateCompatibleDC(d->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(d->hDC, w, h);
    HGDIOBJ ob = SelectObject(mem, bmp);
    bool dis = (d->itemState & ODS_DISABLED) != 0;
    bool press = (d->itemState & ODS_SELECTED) != 0;
    std::wstring txt = GetText(d->hwndItem);
    RECT box = {0, 0, w, h};
    COLORREF tc = C::text;
    HFONT font = g.fUi;
    RECT tr = box;
    UINT fmt = DT_CENTER | DT_VCENTER | DT_SINGLELINE;
    {
        G::Graphics gr(mem);
        G::SolidBrush bb(GC(bi->bg));
        gr.FillRectangle(&bb, 0, 0, w, h);
        gr.SetSmoothingMode(G::SmoothingModeAntiAlias);
        float rad = (float)S(7);
        switch (bi->kind) {
            case BK_PRIMARY:
                FillRR(gr, box, rad, dis ? C::border : press ? C::accentLo : bi->hover ? C::accentHi : C::accent);
                tc = dis ? C::dim : C::accentText;
                font = g.fUiB;
                break;
            case BK_GHOST:
            case BK_DANGER:
                FillRR(gr, box, rad, press ? C::sel : bi->hover ? C::hover : bi->bg == C::card ? C::input : C::card);
                StrokeRR(gr, box, rad, bi->hover ? C::borderHi : C::border);
                tc = dis ? C::dim : bi->kind == BK_DANGER ? (bi->hover ? C::danger : RGB(214, 120, 112)) : C::text;
                break;
            case BK_SEG:
                if (bi->on) {
                    FillRR(gr, box, (float)S(6), C::segOn);
                    tc = C::accent;
                    font = g.fUiB;
                } else {
                    if (bi->hover) FillRR(gr, box, (float)S(6), C::hover);
                    tc = bi->hover ? C::text : C::sub;
                }
                break;
            case BK_TOGGLE: {
                int th = S(18), tw = S(34);
                RECT t = {0, (h - th) / 2, tw, (h - th) / 2 + th};
                FillRR(gr, t, th / 2.f, bi->on ? C::accent : (bi->hover ? C::borderHi : C::border));
                float kx = bi->on ? (float)(t.right - th / 2) : (float)(t.left + th / 2);
                Dot(gr, kx, (t.top + t.bottom) / 2.f, th / 2.f - S(3), bi->on ? C::accentText : C::text);
                tr.left = tw + S(8);
                fmt = DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS;
                tc = dis ? C::dim : bi->on ? C::text : C::sub;
                break;
            }
            case BK_POWER:
                if (bi->on) {
                    FillRR(gr, box, (float)S(18), press ? C::accentLo : bi->hover ? C::accentHi : C::accent);
                    tc = C::accentText;
                } else {
                    FillRR(gr, box, (float)S(18), bi->hover ? C::hover : C::card);
                    StrokeRR(gr, box, (float)S(18), C::danger, 1.5f);
                    tc = C::danger;
                }
                font = g.fUiB;
                break;
        }
    }
    Txt(mem, txt, tr, font, tc, fmt);
    BitBlt(d->hDC, d->rcItem.left, d->rcItem.top, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
}

static void DrawListItem(const DRAWITEMSTRUCT* d) {
    int w = d->rcItem.right - d->rcItem.left, h = d->rcItem.bottom - d->rcItem.top;
    if (w <= 0 || h <= 0) return;
    HDC mem = CreateCompatibleDC(d->hDC);
    HBITMAP bmp = CreateCompatibleBitmap(d->hDC, w, h);
    HGDIOBJ ob = SelectObject(mem, bmp);
    int i = (int)d->itemID;
    bool valid = i >= 0 && i < (int)g.macros.size();
    bool selected = (d->itemState & ODS_SELECTED) != 0;
    bool running = valid && g.player.Running() && g.runIdx == i;
    RECT box = {0, S(3), w, h - S(3)};
    {
        G::Graphics gr(mem);
        G::SolidBrush bb(GC(C::panel));
        gr.FillRectangle(&bb, 0, 0, w, h);
        gr.SetSmoothingMode(G::SmoothingModeAntiAlias);
        if (valid && selected) {
            FillRR(gr, box, (float)S(8), C::sel);
            StrokeRR(gr, box, (float)S(8), C::border);
            RECT bar = {S(1), box.top + S(12), S(4), box.bottom - S(12)};
            FillRR(gr, bar, (float)S(2), C::accent);
        }
        if (running) {
            Dot(gr, (float)(w - S(18)), h / 2.f, (float)S(4), C::ok);
            Dot(gr, (float)(w - S(18)), h / 2.f, (float)S(8), C::ok, 50);
        }
    }
    if (valid) {
        const Macro& m = g.macros[i];
        RECT r = {S(14), box.top + S(7), w - S(34), box.top + S(27)};
        Txt(mem, m.name, r, g.fUiB, m.enabled ? C::text : C::dim, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        std::wstring trig = m.trigger.Empty() ? L"未设置触发键" : ComboToString(m.trigger);
        std::wstring subl = trig + L"  ·  " + ModeLabel(m.mode);
        COLORREF sc = C::sub;
        if (!m.enabled) { subl += L"  ·  已停用"; sc = C::dim; }
        else if (HasConflict(i)) { subl += L"  ·  按键冲突"; sc = C::danger; }
        r = {S(14), box.top + S(27), w - S(34), box.bottom - S(5)};
        Txt(mem, subl, r, g.fSmall, sc, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    }
    BitBlt(d->hDC, d->rcItem.left, d->rcItem.top, w, h, mem, 0, 0, SRCCOPY);
    SelectObject(mem, ob);
    DeleteObject(bmp);
    DeleteDC(mem);
}

// ───────────────────────── 命令处理 ─────────────────────────
static void OnCommand(int id, int code, HWND ctl) {
    if (code == EN_SETFOCUS || code == EN_KILLFOCUS) {
        g.focusEdit = code == EN_SETFOCUS ? ctl : nullptr;
        if (code == EN_KILLFOCUS && (id == IDC_GCD || id == IDC_JIT)) LoadSettingsToUi();  // 回显修正后的数值
        InvalidateRect(g.hwnd, nullptr, FALSE);
        return;
    }
    if (code == EN_CHANGE) {
        if (g.loading) return;
        int v = 0;
        switch (id) {
            case IDC_NAME:
                if (g.sel >= 0) { g.macros[g.sel].name = GetText(g.eName); InvalidateRect(g.list, nullptr, FALSE); MarkDirty(); }
                break;
            case IDC_SCRIPT:
                if (g.sel >= 0) { g.macros[g.sel].script = GetText(g.eScript); SetTimer(g.hwnd, T_PARSE, 220, nullptr); MarkDirty(); }
                break;
            case IDC_GCD:
                if (str::ParseInt(GetText(g.eGcd), v)) { g.st.gcdMs = Clamp(v, 100, 10000); MarkDirty(); }
                break;
            case IDC_JIT:
                if (str::ParseInt(GetText(g.eJit), v)) { g.st.jitterPct = Clamp(v, 0, 40); MarkDirty(); }
                break;
            case IDC_WIN:
                g.st.windowMatch = GetText(g.eWin);
                ApplyHook();
                MarkDirty();
                break;
        }
        return;
    }
    if (id == IDC_LIST) {
        if (code == LBN_SELCHANGE) {
            int i = (int)SendMessageW(g.list, LB_GETCURSEL, 0, 0);
            if (i != g.sel) {
                if (g.captureFor) EndCapture();
                g.sel = i;
                LoadMacroToUi();
            }
        }
        return;
    }
    if (code != BN_CLICKED && code != BN_DOUBLECLICKED) return;

    switch (id) {
        case IDC_NEW: {
            Macro m;
            m.name = L"新宏 " + std::to_wstring(g.macros.size() + 1);
            m.script = L"# 每行一个动作，右侧有语法速查\r\n1\r\ngcd\r\n2";
            g.macros.push_back(m);
            RefreshList();
            Select((int)g.macros.size() - 1);
            ApplyHook();
            MarkDirty();
            SetFocus(g.eName);
            SendMessageW(g.eName, EM_SETSEL, 0, -1);
            SetStatus(L"已新建宏，记得设置触发键", C::ok);
            break;
        }
        case IDC_DUP: {
            if (g.sel < 0) break;
            Macro m = g.macros[g.sel];
            m.name += L" 副本";
            m.trigger = Combo{};
            g.macros.insert(g.macros.begin() + g.sel + 1, m);
            if (g.runIdx > g.sel) ++g.runIdx;
            RefreshList();
            Select(g.sel + 1);
            ApplyHook();
            MarkDirty();
            SetStatus(L"已复制，副本的触发键需要重新设置");
            break;
        }
        case IDC_DEL: {
            if (g.sel < 0) break;
            std::wstring q = L"确定删除「" + g.macros[g.sel].name + L"」吗？此操作无法撤销。";
            if (MessageBoxW(g.hwnd, q.c_str(), APP_TITLE, MB_OKCANCEL | MB_ICONWARNING) != IDOK) break;
            if (g.runIdx == g.sel) { g.player.Stop(); g.runIdx = -1; }
            else if (g.runIdx > g.sel) --g.runIdx;
            g.macros.erase(g.macros.begin() + g.sel);
            int ns = std::min(g.sel, (int)g.macros.size() - 1);
            RefreshList();
            Select(ns);
            ApplyHook();
            MarkDirty();
            RefreshRunState();
            SetStatus(L"已删除");
            break;
        }
        case IDC_REC: BeginCapture(1); InvalidateRect(g.hwnd, nullptr, FALSE); break;
        case IDC_TREC: BeginCapture(2); InvalidateRect(g.hwnd, nullptr, FALSE); break;
        case IDC_ONCE:
        case IDC_LOOP:
        case IDC_HOLD:
            if (g.sel >= 0) {
                g.macros[g.sel].mode = (Mode)(id - IDC_ONCE);
                SetSeg(g.macros[g.sel].mode);
                InvalidateRect(g.list, nullptr, FALSE);
                RECT r = {g.rx, g.rowMode, g.rx + g.rw, g.rowMode + S(20)};
                InvalidateRect(g.hwnd, &r, FALSE);
                MarkDirty();
            }
            break;
        case IDC_EN:
            if (g.sel >= 0) {
                g.macros[g.sel].enabled = !g.macros[g.sel].enabled;
                SetOn(g.tEn, g.macros[g.sel].enabled);
                if (!g.macros[g.sel].enabled && g.runIdx == g.sel) g.player.RequestStop();
                ApplyHook();
                InvalidateRect(g.list, nullptr, FALSE);
                MarkDirty();
            }
            break;
        case IDC_BLOCK:
            if (g.sel >= 0) {
                g.macros[g.sel].block = !g.macros[g.sel].block;
                SetOn(g.tBlock, g.macros[g.sel].block);
                ApplyHook();
                MarkDirty();
            }
            break;
        case IDC_FG:
            g.st.foregroundOnly = !g.st.foregroundOnly;
            SetOn(g.tFg, g.st.foregroundOnly);
            ApplyHook();
            MarkDirty();
            SetStatus(g.st.foregroundOnly ? L"只在标题匹配的游戏窗口里响应触发键" : L"在任何窗口都会响应触发键（本程序窗口除外）",
                      g.st.foregroundOnly ? C::sub : C::accent);
            break;
        case IDC_HUMAN:
            g.st.humanize = !g.st.humanize;
            SetOn(g.tHuman, g.st.humanize);
            MarkDirty();
            SetStatus(g.st.humanize ? L"拟人化已开启：随机按压时长、节奏抖动、曲线鼠标轨迹" : L"拟人化已关闭：固定节奏，精确执行");
            break;
        case IDC_SOUND:
            g.st.sound = !g.st.sound;
            SetOn(g.tSound, g.st.sound);
            MarkDirty();
            break;
        case IDC_POWER:
            SetEnabled(!g.enabled, false);
            break;
    }
}

static void ShowMain() {
    ShowWindow(g.hwnd, SW_SHOW);
    if (IsIconic(g.hwnd)) ShowWindow(g.hwnd, SW_RESTORE);
    SetForegroundWindow(g.hwnd);
}

static void TrayMenu() {
    HMENU m = CreatePopupMenu();
    AppendMenuW(m, MF_STRING, ID_TRAY_SHOW, L"显示主界面");
    AppendMenuW(m, MF_STRING | (g.enabled ? MF_CHECKED : 0), ID_TRAY_TOGGLE, L"启用按键宏");
    AppendMenuW(m, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(m, MF_STRING, ID_TRAY_EXIT, L"退出");
    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(g.hwnd);
    int cmd = TrackPopupMenu(m, TPM_RETURNCMD | TPM_RIGHTBUTTON | TPM_NONOTIFY, pt.x, pt.y, 0, g.hwnd, nullptr);
    DestroyMenu(m);
    if (cmd == ID_TRAY_SHOW) ShowMain();
    else if (cmd == ID_TRAY_TOGGLE) SetEnabled(!g.enabled, false);
    else if (cmd == ID_TRAY_EXIT) DestroyWindow(g.hwnd);
}

// ───────────────────────── 窗口过程 ─────────────────────────
static LRESULT CALLBACK WndProc(HWND h, UINT msg, WPARAM w, LPARAM l) {
    if (g.msgTaskbarCreated && msg == g.msgTaskbarCreated) {
        TrayAdd();
        return 0;
    }
    switch (msg) {
        case WM_CREATE: {
            g.hwnd = h;
            g.dpi = GetDpiForWindow(h);
            BOOL dark = TRUE;
            DwmSetWindowAttribute(h, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
            COLORREF cap = C::panel, bor = C::border;
            DwmSetWindowAttribute(h, DWMWA_CAPTION_COLOR, &cap, sizeof(cap));
            DwmSetWindowAttribute(h, DWMWA_BORDER_COLOR, &bor, sizeof(bor));
            g.brInput = CreateSolidBrush(C::input);
            g.brPanel = CreateSolidBrush(C::panel);
            CreateFonts();
            CreateControls();
            ApplyFonts();

            g.cfgPath = ResolveConfigPath(&g.portable);
            if (!LoadConfig(g.cfgPath, g.st, g.macros)) {
                g.macros = DefaultMacros();
                SaveConfig(g.cfgPath, g.st, g.macros);
            }
            LoadSettingsToUi();
            SetPowerText();
            g.sel = g.macros.empty() ? -1 : 0;
            RefreshList();
            LoadMacroToUi();

            if (!hook::Start(h)) SetStatus(L"⚠ 无法安装键盘钩子，触发键将不起作用", C::danger);
            else SetStatus(L"就绪 · 进入游戏后按触发键即可，按 " +
                           (g.st.toggleKey.Empty() ? std::wstring(L"总开关") : ComboToString(g.st.toggleKey)) +
                           L" 可随时暂停全部宏");
            ApplyHook();
            g.msgTaskbarCreated = RegisterWindowMessageW(L"TaskbarCreated");
            TrayAdd();

            // 按 DPI 设定初始尺寸并居中
            MONITORINFO mi{sizeof(mi)};
            GetMonitorInfoW(MonitorFromWindow(h, MONITOR_DEFAULTTOPRIMARY), &mi);
            RECT wa = mi.rcWork;
            int ww = std::min(S(1100), (int)(wa.right - wa.left)), wh = std::min(S(740), (int)(wa.bottom - wa.top));
            SetWindowPos(h, nullptr, wa.left + (wa.right - wa.left - ww) / 2, wa.top + (wa.bottom - wa.top - wh) / 2, ww, wh,
                         SWP_NOZORDER | SWP_NOACTIVATE);
            Layout();
            return 0;
        }
        case WM_SIZE:
            if (w == SIZE_MINIMIZED) {
                ShowWindow(h, SW_HIDE);
                if (!g.trayTipShown) {
                    g.trayTipShown = true;
                    TrayBalloon(APP_TITLE, L"已收到托盘，按键宏仍在后台工作。双击托盘图标可重新打开。");
                }
            } else {
                Layout();
            }
            return 0;
        case WM_GETMINMAXINFO: {
            auto* mmi = reinterpret_cast<MINMAXINFO*>(l);
            mmi->ptMinTrackSize.x = S(920);
            mmi->ptMinTrackSize.y = S(660);
            return 0;
        }
        case WM_DPICHANGED: {
            g.dpi = HIWORD(w);
            CreateFonts();
            ApplyFonts();
            auto* r = reinterpret_cast<RECT*>(l);
            SetWindowPos(h, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top, SWP_NOZORDER | SWP_NOACTIVATE);
            Layout();
            return 0;
        }
        case WM_ERASEBKGND:
            return 1;
        case WM_PAINT: {
            PAINTSTRUCT ps;
            HDC dc = BeginPaint(h, &ps);
            RECT rc;
            GetClientRect(h, &rc);
            if (rc.right > 0 && rc.bottom > 0) {
                HDC mem = CreateCompatibleDC(dc);
                HBITMAP bmp = CreateCompatibleBitmap(dc, rc.right, rc.bottom);
                HGDIOBJ ob = SelectObject(mem, bmp);
                PaintMain(mem, rc);
                BitBlt(dc, ps.rcPaint.left, ps.rcPaint.top, ps.rcPaint.right - ps.rcPaint.left,
                       ps.rcPaint.bottom - ps.rcPaint.top, mem, ps.rcPaint.left, ps.rcPaint.top, SRCCOPY);
                SelectObject(mem, ob);
                DeleteObject(bmp);
                DeleteDC(mem);
            }
            EndPaint(h, &ps);
            return 0;
        }
        case WM_CTLCOLOREDIT:
        case WM_CTLCOLORSTATIC: {
            HDC dc = (HDC)w;
            HWND c = (HWND)l;
            COLORREF tc = C::text;
            if (c == g.eTrig || c == g.eTKey) {
                bool capturing = (c == g.eTrig && g.captureFor == 1) || (c == g.eTKey && g.captureFor == 2);
                std::wstring t = GetText(c);
                tc = capturing ? C::accent : (t == L"未设置" ? C::dim : C::accent);
            }
            SetTextColor(dc, tc);
            SetBkColor(dc, C::input);
            return (LRESULT)g.brInput;
        }
        case WM_CTLCOLORLISTBOX:
            SetBkColor((HDC)w, C::panel);
            return (LRESULT)g.brPanel;
        case WM_MEASUREITEM: {
            auto* mi = reinterpret_cast<MEASUREITEMSTRUCT*>(l);
            if (mi->CtlType == ODT_LISTBOX) mi->itemHeight = S(56);
            return TRUE;
        }
        case WM_DRAWITEM: {
            auto* d = reinterpret_cast<DRAWITEMSTRUCT*>(l);
            if (d->CtlType == ODT_LISTBOX) DrawListItem(d);
            else if (d->CtlType == ODT_BUTTON) DrawButton(d);
            return TRUE;
        }
        case WM_COMMAND:
            OnCommand(LOWORD(w), HIWORD(w), (HWND)l);
            return 0;
        case WM_TIMER:
            if (w == T_SAVE) SaveNow();
            else if (w == T_PARSE) { KillTimer(h, T_PARSE); ParseCurrent(); }
            return 0;
        case WM_ACTIVATE:
            if (LOWORD(w) == WA_INACTIVE && g.captureFor) {
                EndCapture();
                SetStatus(L"已取消录制");
                InvalidateRect(h, nullptr, FALSE);
            }
            break;

        case WM_APP_TRIGGER: {
            int i = (int)w;
            bool down = l != 0;
            if (!g.enabled || i < 0 || i >= (int)g.macros.size()) return 0;
            const Macro& m = g.macros[i];
            bool runningThis = g.player.Running() && g.runIdx == i;
            if (m.mode == Mode::Hold) {
                if (down && !runningThis) StartMacro(i);
                else if (!down && runningThis) g.player.RequestStop();
            } else if (down) {
                if (runningThis) g.player.RequestStop();
                else StartMacro(i);
            }
            return 0;
        }
        case WM_APP_PLAYER: {
            if ((int)l != g.player.Generation()) return 0;
            std::wstring name = (g.runIdx >= 0 && g.runIdx < (int)g.macros.size()) ? g.macros[g.runIdx].name : L"";
            g.runIdx = -1;
            switch ((int)w) {
                case SR_DONE: SetStatus(L"✓「" + name + L"」执行完毕"); break;
                case SR_USER: SetStatus(L"■ 已停止「" + name + L"」"); break;
                case SR_FOCUS: SetStatus(L"⚠ 游戏窗口不在前台，已自动停止「" + name + L"」", C::accent); break;
            }
            RefreshRunState();
            return 0;
        }
        case WM_APP_TOGGLE:
            SetEnabled(!g.enabled, true);
            return 0;
        case WM_APP_CAPTURE:
            OnCaptured((UINT)w, (UINT)l);
            InvalidateRect(h, nullptr, FALSE);
            return 0;
        case WM_APP_TRAY:
            if (LOWORD(l) == WM_LBUTTONDBLCLK || LOWORD(l) == WM_LBUTTONUP) ShowMain();
            else if (LOWORD(l) == WM_RBUTTONUP || LOWORD(l) == WM_CONTEXTMENU) TrayMenu();
            return 0;

        case WM_CLOSE:
            DestroyWindow(h);
            return 0;
        case WM_DESTROY:
            g.player.Stop();
            hook::Stop();
            SaveNow();
            Shell_NotifyIconW(NIM_DELETE, &g.nid);
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProcW(h, msg, w, l);
}

int WINAPI wWinMain(HINSTANCE hi, HINSTANCE, LPWSTR, int show) {
    HANDLE mutex = CreateMutexW(nullptr, TRUE, L"LaoCH-YXMN-SingleInstance");
    if (GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND other = FindWindowW(APP_CLASS, nullptr)) {
            ShowWindow(other, SW_SHOW);
            ShowWindow(other, SW_RESTORE);
            SetForegroundWindow(other);
        }
        return 0;
    }
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX icc{sizeof(icc), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&icc);
    G::GdiplusStartupInput gsi;
    ULONG_PTR token = 0;
    G::GdiplusStartup(&token, &gsi, nullptr);
    InitDarkMode();

    g.hi = hi;
    g.icoBig = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                 GetSystemMetrics(SM_CYICON), LR_DEFAULTCOLOR);
    g.icoSmall = (HICON)LoadImageW(hi, MAKEINTRESOURCEW(IDI_APP), IMAGE_ICON, GetSystemMetrics(SM_CXSMICON),
                                   GetSystemMetrics(SM_CYSMICON), LR_DEFAULTCOLOR);
    WNDCLASSEXW wc{sizeof(wc)};
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hi;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    wc.hIcon = g.icoBig;
    wc.hIconSm = g.icoSmall;
    wc.lpszClassName = APP_CLASS;
    RegisterClassExW(&wc);

    HWND h = CreateWindowExW(0, APP_CLASS, APP_TITLE, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, CW_USEDEFAULT, CW_USEDEFAULT,
                             1100, 740, nullptr, nullptr, hi, nullptr);
    if (!h) return 1;
    ShowWindow(h, show == SW_SHOWMINIMIZED || show == SW_MINIMIZE ? SW_SHOWNORMAL : show);
    UpdateWindow(h);

    MSG msg;
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    G::GdiplusShutdown(token);
    if (mutex) CloseHandle(mutex);
    return 0;
}
