// 动作脚本解析器：每行一个动作，见 README「脚本语法」
#include "script.h"
#include "keys.h"

namespace {

enum Cmd { C_NONE, C_KEY, C_HOLD, C_REL, C_WAIT, C_GCD, C_CLICK, C_DBL, C_MDOWN, C_MUP,
           C_MOVE, C_MOVEREL, C_SCROLL, C_REPEAT, C_END };

struct CmdName { const wchar_t* n; Cmd c; };
const CmdName kCmds[] = {
    {L"key", C_KEY}, {L"tap", C_KEY}, {L"press", C_KEY}, {L"按", C_KEY}, {L"按键", C_KEY},
    {L"hold", C_HOLD}, {L"按住", C_HOLD},
    {L"release", C_REL}, {L"松开", C_REL},
    {L"wait", C_WAIT}, {L"sleep", C_WAIT}, {L"delay", C_WAIT}, {L"等待", C_WAIT}, {L"等", C_WAIT},
    {L"gcd", C_GCD}, {L"公共冷却", C_GCD},
    {L"click", C_CLICK}, {L"点击", C_CLICK},
    {L"dblclick", C_DBL}, {L"doubleclick", C_DBL}, {L"双击", C_DBL},
    {L"mdown", C_MDOWN}, {L"mup", C_MUP},
    {L"move", C_MOVE}, {L"移动", C_MOVE},
    {L"moverel", C_MOVEREL}, {L"偏移", C_MOVEREL},
    {L"scroll", C_SCROLL}, {L"wheel", C_SCROLL}, {L"滚轮", C_SCROLL},
    {L"repeat", C_REPEAT}, {L"loop", C_REPEAT}, {L"重复", C_REPEAT},
    {L"end", C_END}, {L"结束", C_END},
};

Cmd LookupCmd(const std::wstring& t) {
    for (const auto& c : kCmds)
        if (str::IEquals(t, c.n)) return c.c;
    return C_NONE;
}

std::vector<std::wstring> Tokens(const std::wstring& s) {
    std::vector<std::wstring> out;
    std::wstring cur;
    for (wchar_t c : s) {
        if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\x3000') {
            if (!cur.empty()) { out.push_back(cur); cur.clear(); }
        } else {
            cur += c;
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

bool ParseCount(const std::wstring& t, int& n) {
    if (t.size() < 2) return false;
    wchar_t c = t[0];
    if (c != L'x' && c != L'X' && c != L'*' && c != L'\x00D7') return false;
    return str::ParseInt(t.substr(1), n) && n >= 1 && n <= 99;
}

bool ParseButton(const std::wstring& t, int& b) {
    std::wstring l = str::Lower(t);
    if (l == L"left" || l == L"l" || l == L"左" || l == L"左键") { b = BTN_LEFT; return true; }
    if (l == L"right" || l == L"r" || l == L"右" || l == L"右键") { b = BTN_RIGHT; return true; }
    if (l == L"middle" || l == L"m" || l == L"中" || l == L"中键") { b = BTN_MIDDLE; return true; }
    if (l == L"x1" || l == L"mouse4") { b = BTN_X1; return true; }
    if (l == L"x2" || l == L"mouse5") { b = BTN_X2; return true; }
    return false;
}

bool ParseRange(const std::wstring& t, int& a, int& b) {
    size_t p = t.find(L'-');
    if (p == std::wstring::npos) p = t.find(L'~');
    if (p == std::wstring::npos) {
        if (!str::ParseInt(t, a)) return false;
        b = a;
    } else {
        if (!str::ParseInt(t.substr(0, p), a) || !str::ParseInt(t.substr(p + 1), b)) return false;
    }
    return a >= 0 && b >= a && b <= 600000;
}

}  // namespace

bool ParseScript(const std::wstring& src, Program& out, ParseError& err) {
    out = Program{};
    err = ParseError{};
    std::vector<int> stack;
    std::vector<std::wstring> lines = str::Split(src, L'\n');
    int lineNo = 0;

    auto fail = [&](const std::wstring& msg) {
        err.line = lineNo;
        err.msg = msg;
        return false;
    };

    for (auto& raw : lines) {
        ++lineNo;
        std::wstring line = raw;
        size_t c = line.find(L'#');
        if (c != std::wstring::npos) line.resize(c);
        c = line.find(L"//");
        if (c != std::wstring::npos) line.resize(c);
        auto tk = Tokens(line);
        if (tk.empty()) continue;

        Step s;
        s.line = lineNo;
        Cmd cmd = LookupCmd(tk[0]);
        size_t ai = 1;
        if (cmd == C_NONE) {
            Combo k;
            if (ParseCombo(tk[0], k)) { cmd = C_KEY; ai = 0; }
            else return fail(L"无法识别：" + tk[0]);
        }
        size_t argc = tk.size() - ai;

        switch (cmd) {
            case C_KEY: {
                if (argc < 1) return fail(L"缺少按键名");
                if (!ParseCombo(tk[ai], s.key)) return fail(L"无法识别的按键：" + tk[ai]);
                s.op = Op::Tap;
                s.a = 1;
                if (argc >= 2 && !ParseCount(tk[ai + 1], s.a)) return fail(L"次数写法应为 x2 ～ x99");
                if (argc > 2) return fail(L"参数过多");
                out.actions++;
                break;
            }
            case C_HOLD:
            case C_REL: {
                if (argc != 1) return fail(L"需要一个按键，例如 hold shift");
                if (!ParseCombo(tk[ai], s.key)) return fail(L"无法识别的按键：" + tk[ai]);
                if (s.key.mods) return fail(L"按住/松开只能是单个键");
                s.op = cmd == C_HOLD ? Op::Down : Op::Up;
                out.actions++;
                break;
            }
            case C_WAIT: {
                if (argc != 1) return fail(L"写法：wait 800 或 wait 300-500");
                if (!ParseRange(tk[ai], s.a, s.b)) return fail(L"等待时间应为 0 ～ 600000 毫秒");
                s.op = Op::Wait;
                break;
            }
            case C_GCD: {
                if (argc != 0) return fail(L"gcd 不需要参数（时长在全局设置里）");
                s.op = Op::Gcd;
                break;
            }
            case C_CLICK:
            case C_DBL:
            case C_MDOWN:
            case C_MUP: {
                s.a = BTN_LEFT;
                s.b = cmd == C_DBL ? 2 : 1;
                for (size_t i = ai; i < tk.size(); ++i) {
                    int v = 0;
                    if (ParseButton(tk[i], v)) s.a = v;
                    else if ((cmd == C_CLICK) && ParseCount(tk[i], v)) s.b = v;
                    else return fail(L"无法识别的参数：" + tk[i]);
                }
                s.op = cmd == C_MDOWN ? Op::MDown : cmd == C_MUP ? Op::MUp : Op::Click;
                out.actions++;
                break;
            }
            case C_MOVE:
            case C_MOVEREL: {
                if (argc != 2) return fail(cmd == C_MOVE ? L"写法：move 960 540" : L"写法：moverel 40 -10");
                if (!str::ParseInt(tk[ai], s.a) || !str::ParseInt(tk[ai + 1], s.b)) return fail(L"坐标必须是整数");
                if (s.a < -20000 || s.a > 20000 || s.b < -20000 || s.b > 20000) return fail(L"坐标超出范围");
                s.op = cmd == C_MOVE ? Op::Move : Op::MoveRel;
                out.actions++;
                break;
            }
            case C_SCROLL: {
                if (argc != 1 || !str::ParseInt(tk[ai], s.a) || s.a == 0 || s.a < -50 || s.a > 50)
                    return fail(L"写法：scroll 3（向上）或 scroll -3（向下）");
                s.op = Op::Scroll;
                out.actions++;
                break;
            }
            case C_REPEAT: {
                if (argc != 1 || !str::ParseInt(tk[ai], s.a) || s.a < 1 || s.a > 9999)
                    return fail(L"写法：repeat 3（1 ～ 9999）");
                s.op = Op::Repeat;
                stack.push_back((int)out.steps.size());
                break;
            }
            case C_END: {
                if (argc != 0) return fail(L"end 不需要参数");
                if (stack.empty()) return fail(L"多余的 end");
                int r = stack.back();
                stack.pop_back();
                s.op = Op::End;
                s.jump = r;
                out.steps[r].jump = (int)out.steps.size();
                break;
            }
            default:
                return fail(L"无法识别：" + tk[0]);
        }
        out.steps.push_back(s);
    }

    if (!stack.empty()) {
        err.line = out.steps[stack.back()].line;
        err.msg = L"repeat 缺少对应的 end";
        return false;
    }
    if (out.actions == 0) {
        err.line = 0;
        err.msg = L"脚本里还没有动作";
        return false;
    }
    return true;
}
