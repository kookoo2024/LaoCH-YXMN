// 拟人化输入引擎
//  · 按键按压时长呈对数正态分布（约 35～140ms），组合键按真人顺序先按修饰键
//  · 等待时间带高斯抖动，偶尔出现短暂"犹豫"
//  · 公共冷却只会略晚于冷却结束，不会提前（与真人反应一致）
//  · 鼠标沿随机三次贝塞尔曲线移动，速度符合最小加加速度模型，带轻微手抖，
//    长距离移动会先略微冲过目标再修正
//  · 高精度计时：运行期间临时提升系统计时器精度，空闲时恢复，不常驻耗电
#include "player.h"
#include "hook.h"
#include "keys.h"
#include <mmsystem.h>
#include <cmath>
#include <algorithm>

namespace {

bool IsExtendedVk(UINT vk) {
    switch (vk) {
        case VK_INSERT: case VK_DELETE: case VK_HOME: case VK_END: case VK_PRIOR: case VK_NEXT:
        case VK_LEFT: case VK_RIGHT: case VK_UP: case VK_DOWN: case VK_DIVIDE: case VK_RCONTROL:
        case VK_RMENU: case VK_NUMLOCK: case VK_SNAPSHOT: case VK_LWIN: case VK_RWIN: case VK_APPS:
            return true;
    }
    return false;
}

void SendAbs(double x, double y) {
    int vx = GetSystemMetrics(SM_XVIRTUALSCREEN), vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int vw = GetSystemMetrics(SM_CXVIRTUALSCREEN), vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = (LONG)std::lround((x - vx) * 65535.0 / std::max(1, vw - 1));
    in.mi.dy = (LONG)std::lround((y - vy) * 65535.0 / std::max(1, vh - 1));
    in.mi.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &in, sizeof(in));
}

void SendRel(int dx, int dy) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.dx = dx;
    in.mi.dy = dy;
    in.mi.dwFlags = MOUSEEVENTF_MOVE;
    SendInput(1, &in, sizeof(in));
}

void SendWheel(int delta) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    in.mi.mouseData = (DWORD)delta;
    in.mi.dwFlags = MOUSEEVENTF_WHEEL;
    SendInput(1, &in, sizeof(in));
}

inline uint64_t Rotl(uint64_t x, int k) { return (x << k) | (x >> (64 - k)); }

bool IsActionOp(Op op) {
    return op == Op::Tap || op == Op::Down || op == Op::Up || op == Op::Click || op == Op::MDown ||
           op == Op::MUp || op == Op::Move || op == Op::MoveRel || op == Op::Scroll;
}

}  // namespace

Player::~Player() {
    Stop();
    if (stop_) CloseHandle(stop_);
}

int Player::Start(HWND notify, const PlayParams& p) {
    Stop();
    if (!stop_) stop_ = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    ResetEvent(stop_);
    notify_ = notify;
    p_ = p;
    reason_ = SR_DONE;
    heldKeys_.clear();
    heldBtns_ = 0;
    QueryPerformanceFrequency(&freq_);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    rng_[0] ^= (uint64_t)c.QuadPart * 0x9E3779B97F4A7C15ull;
    rng_[1] ^= ((uint64_t)GetTickCount64() << 21) ^ (uint64_t)GetCurrentThreadId() ^ 1ull;
    for (int i = 0; i < 8; ++i) U();
    ++gen_;
    running_ = true;
    thread_ = CreateThread(nullptr, 0, Entry, this, 0, nullptr);
    if (!thread_) {
        running_ = false;
        return 0;
    }
    return gen_;
}

void Player::RequestStop() {
    if (running_ && stop_) {
        reason_ = SR_USER;
        SetEvent(stop_);
    }
}

void Player::Stop() {
    if (!thread_) return;
    if (running_) {
        reason_ = SR_USER;
        SetEvent(stop_);
    }
    WaitForSingleObject(thread_, 3000);
    CloseHandle(thread_);
    thread_ = nullptr;
}

DWORD WINAPI Player::Entry(LPVOID self) {
    static_cast<Player*>(self)->Run();
    return 0;
}

void Player::Run() {
    const int gen = gen_;
    const HWND notify = notify_;
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);
    timeBeginPeriod(1);
    for (;;) {
        double t0 = Now();
        if (!Exec()) break;
        if (p_.mode == Mode::Once) break;
        double el = Now() - t0;
        if (el < 60 && !Nap(60 - el)) break;  // 防止空转刷键
    }
    ReleaseAll();
    timeEndPeriod(1);
    int r = reason_.load();
    running_ = false;
    PostMessageW(notify, WM_APP_PLAYER, (WPARAM)r, (LPARAM)gen);
}

bool Player::Exec() {
    const auto& st = p_.prog.steps;
    std::vector<int> counters(st.size(), 0);
    size_t pc = 0;
    while (pc < st.size()) {
        if (!Alive()) return false;
        const Step& s = st[pc];
        bool ok = true;
        switch (s.op) {
            case Op::Tap: ok = Tap(s.key, s.a); break;
            case Op::Down: KeyEvt(s.key.vk, false); break;
            case Op::Up: KeyEvt(s.key.vk, true); break;
            case Op::Wait:
                ok = (s.a == s.b) ? HumanWait(s.a) : Nap(Rand(s.a, s.b));
                break;
            case Op::Gcd: {
                double extra = p_.humanize ? Rand(15.0, 15.0 + p_.gcdMs * p_.jitterPct / 100.0 * 0.6) : 0.0;
                ok = Nap(p_.gcdMs + extra);
                break;
            }
            case Op::Click: ok = Click(s.a, s.b); break;
            case Op::MDown: BtnEvt(s.a, false); break;
            case Op::MUp: BtnEvt(s.a, true); break;
            case Op::Move: ok = MoveTo(s.a, s.b); break;
            case Op::MoveRel: ok = MoveRel(s.a, s.b); break;
            case Op::Scroll: ok = Scroll(s.a); break;
            case Op::Repeat: counters[pc] = s.a; break;
            case Op::End: {
                int r = s.jump;
                if (--counters[r] > 0) { pc = (size_t)r + 1; continue; }
                break;
            }
        }
        if (!ok) return false;
        if (IsActionOp(s.op) && pc + 1 < st.size() && IsActionOp(st[pc + 1].op) && !Gap()) return false;
        ++pc;
    }
    return true;
}

bool Player::Alive() {
    if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return false;
    if (!hook::ForegroundAllowed(p_.fgOnly, p_.patterns)) {
        reason_ = SR_FOCUS;
        SetEvent(stop_);
        return false;
    }
    return true;
}

double Player::Now() const {
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart * 1000.0 / (double)freq_.QuadPart;
}

double Player::U() {
    uint64_t s0 = rng_[0], s1 = rng_[1];
    uint64_t r = s0 + s1;
    s1 ^= s0;
    rng_[0] = Rotl(s0, 55) ^ s1 ^ (s1 << 14);
    rng_[1] = Rotl(s1, 36);
    return (double)(r >> 11) * (1.0 / 9007199254740992.0);
}

double Player::N() {
    double u1 = U();
    if (u1 < 1e-12) u1 = 1e-12;
    double u2 = U();
    return std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586 * u2);
}

// 可被立即打断的高精度等待：大段交给内核，最后约 1ms 自旋对齐
bool Player::Nap(double ms) {
    double end = Now() + ms;
    for (;;) {
        double rem = end - Now();
        if (rem > 1.6) {
            if (WaitForSingleObject(stop_, (DWORD)(rem - 1.0)) == WAIT_OBJECT_0) return false;
        } else if (rem > 0) {
            if (WaitForSingleObject(stop_, 0) == WAIT_OBJECT_0) return false;
            YieldProcessor();
        } else {
            return WaitForSingleObject(stop_, 0) != WAIT_OBJECT_0;
        }
    }
}

bool Player::HumanWait(double ms) {
    if (p_.humanize && ms > 0) {
        double j = p_.jitterPct / 100.0;
        ms *= 1.0 + Clamp(N() * j * 0.5, -j, j);
        if (U() < 0.025) ms += Rand(40, 140);  // 偶尔的犹豫
    }
    return Nap(ms);
}

bool Player::Gap() {
    if (!p_.humanize) return Nap(8);
    return Nap(Clamp(std::exp(std::log(34.0) + 0.3 * N()), 16.0, 80.0));
}

double Player::HoldMs() {
    if (!p_.humanize) return 30;
    return Clamp(std::exp(std::log(62.0) + 0.22 * N()), 32.0, 140.0);
}

bool Player::Tap(const Combo& k, int count) {
    UINT mods[3];
    int nm = 0;
    if (k.mods & KM_CTRL) mods[nm++] = VK_CONTROL;
    if (k.mods & KM_ALT) mods[nm++] = VK_MENU;
    if (k.mods & KM_SHIFT) mods[nm++] = VK_SHIFT;
    for (int i = 0; i < count; ++i) {
        if (i > 0 && !Alive()) return false;
        for (int m = 0; m < nm; ++m) {
            KeyEvt(mods[m], false);
            if (!Nap(p_.humanize ? Rand(14, 38) : 10)) return false;
        }
        KeyEvt(k.vk, false);
        if (!Nap(HoldMs())) return false;
        KeyEvt(k.vk, true);
        for (int m = nm - 1; m >= 0; --m) {
            if (!Nap(p_.humanize ? Rand(10, 30) : 6)) return false;
            KeyEvt(mods[m], true);
        }
        if (i + 1 < count && !Nap(p_.humanize ? Rand(70, 140) : 60)) return false;
    }
    return true;
}

bool Player::Click(int btn, int count) {
    for (int i = 0; i < count; ++i) {
        if (i > 0 && !Alive()) return false;
        BtnEvt(btn, false);
        double hold = p_.humanize ? Clamp(std::exp(std::log(78.0) + 0.2 * N()), 45.0, 150.0) : 40.0;
        if (!Nap(hold)) return false;
        BtnEvt(btn, true);
        if (i + 1 < count && !Nap(p_.humanize ? Rand(85, 150) : 70)) return false;
    }
    return true;
}

bool Player::MoveTo(int tx, int ty) {
    POINT p;
    GetCursorPos(&p);
    double sx = p.x, sy = p.y, dx = tx - sx, dy = ty - sy, d = std::hypot(dx, dy);
    if (d < 1) return true;
    if (!p_.humanize) {
        SendAbs(tx, ty);
        return Nap(5);
    }
    bool overshoot = d > 220 && U() < 0.55;
    double ex = tx, ey = ty;
    if (overshoot) {
        double o = Rand(4.0, std::min(14.0, d * 0.04));
        ex += dx / d * o + N() * 1.5;
        ey += dy / d * o + N() * 1.5;
    }
    if (!Curve(sx, sy, ex, ey, true)) return false;
    if (overshoot) {
        if (!Nap(Rand(25, 60))) return false;
        if (!Curve(ex, ey, tx, ty, true)) return false;
    }
    SendAbs(tx, ty);
    return true;
}

bool Player::MoveRel(int dx, int dy) {
    if (!p_.humanize) {
        SendRel(dx, dy);
        return Nap(5);
    }
    return Curve(0, 0, dx, dy, false);
}

bool Player::Curve(double sx, double sy, double ex, double ey, bool absolute) {
    double dx = ex - sx, dy = ey - sy, d = std::hypot(dx, dy);
    if (d < 0.5) return true;
    // Fitts 定律估算移动耗时
    double T = (70.0 + 105.0 * std::log2(d / 10.0 + 1.0)) * Rand(0.85, 1.15);
    T = Clamp(T, 60.0, 900.0);
    double nx = -dy / d, ny = dx / d;
    double bend = d * Rand(0.05, 0.22) * (U() < 0.5 ? -1.0 : 1.0);
    double k1 = Rand(0.2, 0.4), k2 = Rand(0.6, 0.8), b2 = bend * Rand(0.3, 1.0);
    double c1x = sx + dx * k1 + nx * bend, c1y = sy + dy * k1 + ny * bend;
    double c2x = sx + dx * k2 + nx * b2, c2y = sy + dy * k2 + ny * b2;
    int steps = std::max(6, (int)(T / 7.0));
    double t0 = Now();
    double lastX = sx, lastY = sy, accX = 0, accY = 0;
    for (int i = 1; i <= steps; ++i) {
        if (!Alive()) return false;
        double t = (double)i / steps;
        double s = t * t * t * (10.0 + t * (-15.0 + 6.0 * t));  // 最小加加速度
        double u = 1.0 - s;
        double x = u * u * u * sx + 3 * u * u * s * c1x + 3 * u * s * s * c2x + s * s * s * ex;
        double y = u * u * u * sy + 3 * u * u * s * c1y + 3 * u * s * s * c2y + s * s * s * ey;
        if (i < steps) { x += N() * 0.35; y += N() * 0.35; }  // 手部微颤
        if (absolute) {
            SendAbs(x, y);
        } else {
            accX += x - lastX;
            accY += y - lastY;
            int ix = (int)std::lround(accX), iy = (int)std::lround(accY);
            if (ix || iy) { SendRel(ix, iy); accX -= ix; accY -= iy; }
            lastX = x;
            lastY = y;
        }
        double target = t0 + T * i / steps, now = Now();
        if (target > now && !Nap(target - now)) return false;
    }
    return true;
}

bool Player::Scroll(int n) {
    int dir = n > 0 ? 1 : -1;
    for (int i = 0; i < std::abs(n); ++i) {
        if (i > 0 && !Alive()) return false;
        SendWheel(dir * WHEEL_DELTA);
        if (i + 1 < std::abs(n) && !Nap(p_.humanize ? Rand(35, 80) : 20)) return false;
    }
    return true;
}

void Player::KeyEvt(UINT vk, bool up) {
    if (IsMouseVk(vk)) {
        int b = vk == VK_LBUTTON ? BTN_LEFT : vk == VK_RBUTTON ? BTN_RIGHT : vk == VK_MBUTTON ? BTN_MIDDLE
              : vk == VK_XBUTTON1 ? BTN_X1 : BTN_X2;
        BtnEvt(b, up);
        return;
    }
    INPUT in{};
    in.type = INPUT_KEYBOARD;
    UINT sc = MapVirtualKeyW(vk, MAPVK_VK_TO_VSC_EX);
    bool ext = (sc & 0xFF00) == 0xE000 || IsExtendedVk(vk);
    in.ki.wVk = (WORD)vk;
    if ((sc & 0xFF) == 0 || vk == VK_PAUSE) {
        in.ki.dwFlags = (up ? KEYEVENTF_KEYUP : 0) | (ext ? KEYEVENTF_EXTENDEDKEY : 0);
    } else {
        // 用扫描码发送：游戏通过 Raw Input / DirectInput 读取的是扫描码
        in.ki.wScan = (WORD)(sc & 0xFF);
        in.ki.dwFlags = KEYEVENTF_SCANCODE | (up ? KEYEVENTF_KEYUP : 0) | (ext ? KEYEVENTF_EXTENDEDKEY : 0);
    }
    SendInput(1, &in, sizeof(in));
    auto it = std::find(heldKeys_.begin(), heldKeys_.end(), vk);
    if (!up && it == heldKeys_.end()) heldKeys_.push_back(vk);
    if (up && it != heldKeys_.end()) heldKeys_.erase(it);
}

void Player::BtnEvt(int b, bool up) {
    INPUT in{};
    in.type = INPUT_MOUSE;
    switch (b) {
        case BTN_LEFT: in.mi.dwFlags = up ? MOUSEEVENTF_LEFTUP : MOUSEEVENTF_LEFTDOWN; break;
        case BTN_RIGHT: in.mi.dwFlags = up ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_RIGHTDOWN; break;
        case BTN_MIDDLE: in.mi.dwFlags = up ? MOUSEEVENTF_MIDDLEUP : MOUSEEVENTF_MIDDLEDOWN; break;
        default:
            in.mi.dwFlags = up ? MOUSEEVENTF_XUP : MOUSEEVENTF_XDOWN;
            in.mi.mouseData = b == BTN_X1 ? XBUTTON1 : XBUTTON2;
            break;
    }
    SendInput(1, &in, sizeof(in));
    if (up) heldBtns_ &= ~(1u << b);
    else heldBtns_ |= (1u << b);
}

// 停止时把脚本按住的键全部松开，避免"粘键"
void Player::ReleaseAll() {
    std::vector<UINT> keys = heldKeys_;
    for (auto it = keys.rbegin(); it != keys.rend(); ++it) KeyEvt(*it, true);
    for (int b = 0; b < 5; ++b)
        if (heldBtns_ & (1u << b)) BtnEvt(b, true);
}
