<p align="center"><img src="docs/icon.png" width="96" alt="LaoCH 键位助手"></p>

<h1 align="center">LaoCH 键位助手</h1>
<p align="center">为《魔兽世界》设计的拟人化按键宏 · 纯 C++ / Win32 · 零依赖 · 单文件约 400 KB</p>

---

一套固定的技能顺序按得手酸？把它写成几行脚本，绑到一个键或鼠标侧键上，按一下就替你按完。
节奏、按压时长、鼠标轨迹都模拟真人习惯，而不是机械地"哒哒哒"。

## 特色

| | |
|---|---|
| **零依赖** | 只用 Windows 自带的系统库（user32 / gdi32 / gdiplus / comctl32 …），静态链接运行库，拷过去双击就能用，无需安装 |
| **小巧高效** | 单个 exe 约 400 KB；空闲时 CPU 占用为 0；全局钩子运行在独立高优先级线程，只做最少的工作，不拖慢系统输入；高精度计时只在宏运行时开启 |
| **拟人化输入** | 按压时长呈对数正态分布（约 35～140 ms）；组合键按真人顺序先按修饰键；等待自带高斯抖动和偶发"犹豫"；公共冷却只会略晚、不会提前；鼠标走随机贝塞尔曲线，速度符合最小加加速度模型，带微颤，长距离会轻微冲过再修正 |
| **三种模式** | 单次（按一下打完一整套）· 循环（按一下开始，再按停止）· 按住（按住时循环，松开立即停） |
| **游戏内安全阀** | 默认只在标题含「魔兽世界 / World of Warcraft」的窗口生效；切出游戏自动停止；停止时自动松开所有被按住的键，绝不"粘键"；Pause 键一键暂停全部宏 |
| **触发灵活** | 任意按键、Ctrl/Shift/Alt 组合、鼠标中键与侧键（Mouse4 / Mouse5）；可选择拦截触发键，不让游戏收到 |
| **现代界面** | 深色主题、Per-Monitor V2 高 DPI 清晰显示、实时语法检查、按键冲突提示、修改自动保存、托盘常驻、单实例 |
| **扫描码输入** | 以硬件扫描码发送按键，兼容使用 Raw Input / DirectInput 的游戏 |

## 快速开始

1. 下载 `LaoCH-YXMN.exe`（见 Releases，或按下文自行编译），双击运行。
2. 首次运行会带 4 个示例宏：单体循环（F5）、一键爆发（F6）、按住群体（鼠标前进侧键）、鼠标处放地面技能（鼠标后退侧键）。
3. 把脚本里的 `1` `2` `3` 换成你动作条上的实际按键，进入游戏按触发键即可。
4. 随时按 **Pause** 暂停 / 恢复全部宏（可在「全局设置」里改）。

> 如果游戏以管理员身份运行，本程序也需要以管理员身份运行，否则 Windows 会拦截模拟输入。

## 脚本语法

每行一个动作，`#` 后面是注释。指令不区分大小写，也支持中文别名（按、等待、点击、重复…）。

| 写法 | 作用 |
|---|---|
| `1` / `shift+2` / `ctrl+alt+F3` | 按一下（直接写按键名就是按一下） |
| `key 3 x2` | 连按 2 次（x2 ～ x99） |
| `hold shift` / `release shift` | 按住 / 松开某个键 |
| `wait 800` | 等待 800 毫秒（拟人化开启时自动 ±抖动） |
| `wait 300-500` | 在 300～500 毫秒之间随机等待 |
| `gcd` | 等待一个公共冷却（时长在全局设置里，默认 1500ms） |
| `click` / `click right` / `click left x2` | 鼠标点击 |
| `dblclick` | 双击 |
| `mdown left` / `mup left` | 按住 / 松开鼠标键 |
| `move 960 540` | 沿拟人曲线把鼠标移到屏幕坐标 |
| `moverel 40 0` | 相对移动（右键拖视角时可用于转向） |
| `scroll 3` / `scroll -3` | 滚轮向上 / 向下 |
| `repeat 3` … `end` | 重复中间的动作，可嵌套 |

**按键名**：`A`～`Z`、`0`～`9`、`F1`～`F24`、`Num0`～`Num9`、`Num+ Num- Num* Num/ Num.`、
`Space Enter Tab Esc Backspace`、`Up Down Left Right`、`Insert Delete Home End PageUp PageDown`、
`- = [ ] ; ' , . / \`、反引号键 `Grave`、`Mouse3 Mouse4 Mouse5`，以及 `VK_xx`（十六进制虚拟键码）。
方向键要写 `key up`（单独一行的 `up` 不算按键名）。

### 示例：冰霜法师单体

```text
# F5 开始循环，再按 F5 停止
key 1        # 寒冰箭
gcd
key 2        # 冰枪术
gcd
repeat 2
  key 3      # 冰川尖刺
  gcd
end
```

### 示例：鼠标指向处放暴风雪

```text
key shift+4
wait 120-200
click left
```

## 配置文件

* 程序目录下有 `LaoCH.ini` 时为**便携模式**，配置就存在这里（新建一个空文件即可开启）。
* 否则保存在 `%APPDATA%\LaoCH-YXMN\LaoCH.ini`。
* 纯 UTF-8 文本，可直接编辑，也可以整个发给朋友分享你的宏。

## 自行编译

**Visual Studio 2019/2022**（推荐）：打开「x64 Native Tools Command Prompt」，在仓库根目录运行 `build.bat`。
或者：`cmake -S . -B build -A x64 && cmake --build build --config Release`。

**MinGW-w64**（Linux 交叉编译或 MSYS2）：`./build-mingw.sh`

推送到 `main` 时 GitHub Actions 会自动编译，exe 在 Actions 的 Artifacts 里；发布 Release 时会自动附上 exe。

## 代码结构

```
src/
  main.cpp     界面：自绘深色控件、布局、托盘、事件
  player.cpp   拟人化输入引擎（按键 / 鼠标 / 计时）
  hook.cpp     全局键盘 / 鼠标低级钩子（独立线程）
  script.cpp   动作脚本解析器
  keys.cpp     按键名 <-> 虚拟键码
  config.cpp   配置读写与示例宏
res/           图标、清单（高 DPI / 通用控件 v6）、版本信息
```

## 重要提示

暴雪的《魔兽世界》使用条款禁止用第三方程序自动执行游戏操作，尤其是"一次按键触发多个动作"和无人值守的循环。
拟人化可以让操作更自然，但**不能保证不被检测**，使用本工具存在账号被处罚的风险，请自行判断、自负责任。
建议只在需要减轻手部负担时使用，人在电脑前，不要挂机。

## 许可

Copyright © 2026 LaoCH. 保留所有权利。
