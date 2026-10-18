#pragma once
#include "common.h"
#include "script.h"
#include <atomic>

enum StopReason : int { SR_DONE = 0, SR_USER = 1, SR_FOCUS = 2 };

struct PlayParams {
    Program prog;
    Mode mode = Mode::Once;
    int gcdMs = 1500;
    int jitterPct = 12;
    bool humanize = true;
    bool fgOnly = true;
    std::vector<std::wstring> patterns;
};

// 在后台线程执行动作脚本，模拟真人的按键时长、节奏和鼠标轨迹。
class Player {
public:
    ~Player();
    int Start(HWND notify, const PlayParams& p);  // 返回本次运行代号
    void Stop();                                  // 阻塞直到线程退出
    void RequestStop();                           // 立即返回
    bool Running() const { return running_.load(); }
    int Generation() const { return gen_; }

private:
    static DWORD WINAPI Entry(LPVOID self);
    void Run();
    bool Exec();
    bool Alive();
    bool Nap(double ms);
    bool HumanWait(double ms);
    bool Gap();
    double HoldMs();
    bool Tap(const Combo& k, int count);
    bool Click(int btn, int count);
    bool MoveTo(int x, int y);
    bool MoveRel(int dx, int dy);
    bool Curve(double sx, double sy, double ex, double ey, bool absolute);
    bool Scroll(int n);
    void KeyEvt(UINT vk, bool up);
    void BtnEvt(int btn, bool up);
    void ReleaseAll();
    double Now() const;
    double U();
    double N();
    double Rand(double a, double b) { return a + (b - a) * U(); }

    HANDLE thread_ = nullptr;
    HANDLE stop_ = nullptr;
    HWND notify_ = nullptr;
    std::atomic<bool> running_{false};
    std::atomic<int> reason_{SR_DONE};
    int gen_ = 0;
    PlayParams p_;
    std::vector<UINT> heldKeys_;
    unsigned heldBtns_ = 0;
    LARGE_INTEGER freq_{};
    uint64_t rng_[2] = {0x9E3779B97F4A7C15ull, 0xBF58476D1CE4E5B9ull};
};
