// 全局低级键盘/鼠标钩子：在独立的高优先级线程上运行，只做极少的工作，
// 把触发事件投递给 UI 线程，避免拖慢系统输入。
#include "hook.h"
#include "keys.h"

namespace {
SRWLOCK g_lock = SRWLOCK_INIT;
HookConfig g_cfg;
HWND g_notify = nullptr;
volatile LONG g_capture = 0;
HANDLE g_thread = nullptr;
DWORD g_tid = 0;
HANDLE g_ready = nullptr;
HHOOK g_kb = nullptr, g_ms = nullptr;
int g_active[256];      // 当前按下的触发键 -> 宏序号
bool g_blocked[256];    // 该次按下是否被拦截
bool g_toggleDown = false;

UINT CurMods() {
    UINT m = 0;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) m |= KM_CTRL;
    if (GetAsyncKeyState(VK_SHIFT) & 0x8000) m |= KM_SHIFT;
    if (GetAsyncKeyState(VK_MENU) & 0x8000) m |= KM_ALT;
    return m;
}

// 返回 true 表示吞掉该事件
bool Handle(UINT vk, bool down) {
    if (vk == 0 || vk > 255) return false;

    if (g_capture) {
        if (IsModifierVk(vk)) return false;
        if (down) {
            InterlockedExchange(&g_capture, 0);
            PostMessageW(g_notify, WM_APP_CAPTURE, vk, CurMods());
        }
        return true;
    }
    if (IsModifierVk(vk)) return false;

    if (!down) {
        if (g_toggleDown) {
            AcquireSRWLockShared(&g_lock);
            UINT tvk = g_cfg.toggle.vk;
            ReleaseSRWLockShared(&g_lock);
            if (vk == tvk) { g_toggleDown = false; return true; }
        }
        int idx = g_active[vk];
        if (idx >= 0) {
            g_active[vk] = -1;
            PostMessageW(g_notify, WM_APP_TRIGGER, (WPARAM)idx, 0);
            return g_blocked[vk];
        }
        return false;
    }

    // 按下（含系统自动重复）
    if (g_active[vk] >= 0) return g_blocked[vk];

    bool swallow = false;
    AcquireSRWLockShared(&g_lock);
    UINT mods = CurMods();
    if (!g_cfg.toggle.Empty() && vk == g_cfg.toggle.vk && mods == g_cfg.toggle.mods) {
        if (!g_toggleDown) {
            g_toggleDown = true;
            PostMessageW(g_notify, WM_APP_TOGGLE, 0, 0);
        }
        swallow = true;
    } else if (g_cfg.enabled) {
        for (const auto& t : g_cfg.triggers) {
            if (t.combo.vk == vk && t.combo.mods == mods) {
                if (hook::ForegroundAllowed(g_cfg.fgOnly, g_cfg.patterns)) {
                    g_active[vk] = t.index;
                    g_blocked[vk] = t.block;
                    PostMessageW(g_notify, WM_APP_TRIGGER, (WPARAM)t.index, 1);
                    swallow = t.block;
                }
                break;
            }
        }
    }
    ReleaseSRWLockShared(&g_lock);
    return swallow;
}

LRESULT CALLBACK KbProc(int code, WPARAM w, LPARAM l) {
    if (code == HC_ACTION) {
        auto* k = reinterpret_cast<KBDLLHOOKSTRUCT*>(l);
        if (!(k->flags & LLKHF_INJECTED)) {
            bool down = (w == WM_KEYDOWN || w == WM_SYSKEYDOWN);
            if (Handle(k->vkCode, down)) return 1;
        }
    }
    return CallNextHookEx(nullptr, code, w, l);
}

LRESULT CALLBACK MsProc(int code, WPARAM w, LPARAM l) {
    if (code == HC_ACTION && w != WM_MOUSEMOVE) {
        auto* m = reinterpret_cast<MSLLHOOKSTRUCT*>(l);
        if (!(m->flags & LLMHF_INJECTED)) {
            UINT vk = 0;
            bool down = false;
            switch (w) {
                case WM_MBUTTONDOWN: vk = VK_MBUTTON; down = true; break;
                case WM_MBUTTONUP: vk = VK_MBUTTON; break;
                case WM_XBUTTONDOWN:
                case WM_XBUTTONUP:
                    vk = HIWORD(m->mouseData) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2;
                    down = (w == WM_XBUTTONDOWN);
                    break;
            }
            if (vk && Handle(vk, down)) return 1;
        }
    }
    return CallNextHookEx(nullptr, code, w, l);
}

DWORD WINAPI HookThread(LPVOID) {
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
    MSG msg;
    PeekMessageW(&msg, nullptr, 0, 0, PM_NOREMOVE);
    HINSTANCE hi = GetModuleHandleW(nullptr);
    g_kb = SetWindowsHookExW(WH_KEYBOARD_LL, KbProc, hi, 0);
    g_ms = SetWindowsHookExW(WH_MOUSE_LL, MsProc, hi, 0);
    SetEvent(g_ready);
    while (GetMessageW(&msg, nullptr, 0, 0) > 0) DispatchMessageW(&msg);
    if (g_kb) UnhookWindowsHookEx(g_kb);
    if (g_ms) UnhookWindowsHookEx(g_ms);
    g_kb = g_ms = nullptr;
    return 0;
}
}  // namespace

namespace hook {

bool Start(HWND notify) {
    g_notify = notify;
    for (int i = 0; i < 256; ++i) { g_active[i] = -1; g_blocked[i] = false; }
    g_ready = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    g_thread = CreateThread(nullptr, 0, HookThread, nullptr, 0, &g_tid);
    if (!g_thread) return false;
    WaitForSingleObject(g_ready, 3000);
    CloseHandle(g_ready);
    g_ready = nullptr;
    return g_kb != nullptr;
}

void Stop() {
    if (!g_thread) return;
    PostThreadMessageW(g_tid, WM_QUIT, 0, 0);
    WaitForSingleObject(g_thread, 2000);
    CloseHandle(g_thread);
    g_thread = nullptr;
}

void Configure(const HookConfig& cfg) {
    AcquireSRWLockExclusive(&g_lock);
    g_cfg = cfg;
    ReleaseSRWLockExclusive(&g_lock);
}

void SetCapture(bool on) { InterlockedExchange(&g_capture, on ? 1 : 0); }

bool ForegroundAllowed(bool fgOnly, const std::vector<std::wstring>& patterns) {
    HWND fg = GetForegroundWindow();
    if (!fg) return !fgOnly;
    DWORD pid = 0;
    GetWindowThreadProcessId(fg, &pid);
    if (pid == GetCurrentProcessId()) return false;
    if (!fgOnly || patterns.empty()) return true;
    wchar_t buf[256];
    int n = GetWindowTextW(fg, buf, 256);
    if (n <= 0) return false;
    CharLowerBuffW(buf, (DWORD)n);
    std::wstring title(buf, (size_t)n);
    for (const auto& p : patterns)
        if (title.find(p) != std::wstring::npos) return true;
    return false;
}

}  // namespace hook
