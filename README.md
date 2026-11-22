# StickS3 Morse Code Translator / 摩斯电码翻译器

在 M5Stack **StickS3**（ESP32-S3-PICO-1-N8R8）上，用按键手敲摩斯电码，屏幕实时翻译成拉丁字母与数字。
A hand-tapped Morse code translator on the M5Stack StickS3: short and long presses of a button are decoded into letters and digits on screen.

## 它做什么

- 正面键（G11）当电键用：短按是 `·`，长按是 `−`，停顿提交字母，更长停顿打空格
- 26 个字母、10 个数字、6 个标点，外加 BT / AR / KN / SK 四个专业符当操作键
- **不用手动配速**：从最近 16 次按下时长里自动反推单位时长 `u`（40–400ms，约 3–30 WPM），手快慢了自己跟着走
- 屏幕实时显示当前码、剩余候选字母、正文、速率与电量
- 侧面键用手势承担辅助操作：单击退格、双击练习模式、三击回听音开关、四击减速、长按清行
- 扬声器跟随手发出回听音（sidetone）
- 参数存 NVS，断电保留；插 USB 可用串口命令精调

## 硬件

| 项 | 值 |
|---|---|
| 主控 | ESP32-S3-PICO-1-N8R8（8MB Flash + 8MB Octal PSRAM，实测确认） |
| 屏幕 | ST7789P3 135×240，代码里 `setRotation(1)` 转成 240×135 横屏 |
| 摩斯键 KEY1 | GPIO 11 |
| 手势键 KEY2 | GPIO 12 |
| 音频 | ES8311 + AW8737 + 8Ω 喇叭 |
| 电源键 | 挂在 M5PM1 PMIC 上，**不是 GPIO**，程序读不到 |
| 电池 | 250mAh |

## 快速开始

环境：PlatformIO 6.x + `platform-espressif32@6.12.0` + Arduino framework 2.0.17 + M5Unified 0.3.0。
Python 侧需要 `intelhex`（`pip install intelhex`），否则打包 bootloader 那一步会失败。

```bash
pio run -e sticks3              # 编译
pio run -e sticks3 -t upload    # 烧录（原生 USB-CDC，插上线即可）
pio device monitor              # 串口控制台，115200
```

不需要板子也能验证解码内核（纯 C++，零硬件依赖）：

```bash
g++ -std=c++17 -Wall -Wextra -O1 -o tools/hosttest/hosttest \
    src/core/*.cpp tools/hosttest/main_test.cpp
tools/hosttest/hosttest
```

## 目录结构

```
src/core/       解码内核：纯 C++，不含任何 Arduino 依赖，可在电脑上直接编译测试
  morse_table   二元前缀树码表（点=左，划=右）+ 实时候选查询
  timing        单位时长估计器与全部时间判据
  keyer         直键状态机：按下分类、间隙边界、追溯补判
  decoder       符号 → 字符 → 文本缓冲
  gesture       副键多击 / 长按手势机
  textbuf       768 字节环形文本缓冲（支持行操作）
  trainer       练习模式（随机目标 + 命中统计）
  engine        门面：事件路由、快照、UI 动作
src/firmware/   硬件适配层：按键扫描、音频、显示、NVS、串口命令
src/main.cpp    FreeRTOS 任务划分与定时器
tools/hosttest/ 上位机手敲仿真与回归用例
docs/usage.md   详细使用说明
```

## 设计要点

**判定只用事件自带的时间戳，不用程序处理到的时刻。** GPIO 中断和一个 2ms 定时器把带时间戳的事件推进同一条 FIFO，解码时所有间隙判据都取事件之间的差值。这样刷屏幕、写 NVS 这些耗时只会增加显示延迟，不会把 `−` 判成 `·`——仿真里有一条专门喂"敲击期间零 tick"的用例来证明这一点。

**双阈值判定，留犹豫区。** `·` 是按下 < 1.6u，`−` 是 > 2.4u，中间落在犹豫区时按"手型延续"归类，所以手抖不会让同一个划一会儿判点一会儿判划。

**只在词边界更新 `u`。** 一个单词内部阈值必须恒定，否则同一个字母的前半和后半用不同判据。更新用 `(当前u + 新估计)/2` 平滑，符号比例法失败时退回间隙均值估计。

**宽松于标准节奏。** 标准摩斯要求字母间隙 3u、单词间隙 7u，这里放宽到 2u 和 5u——手敲的间隙普遍比电键短。

## 当前状态

已验证：

- 固件编译通过，`-Wall` 零警告，RAM 9.1% / Flash 18.4%
- 14 项上位机仿真全部通过：理想/慢/快手速、无 tick 追溯路径、±25% 抖动（100% 正确）、60→220ms 手速漂移（93% 正确）、专业符、非法码、回听音开关键位配平、速率换算、副键手势
- 已烧进真机并跑起来：屏幕六行布局、字体、横屏方向、计数行全部正确，敲 6 个字母 0 错误

待办 / 已知问题：

- **副键键位待确认**：长按清行在真机上没反应，而仿真里同一段事件流是通过的，怀疑是"侧面蓝色键"实际是 PMIC 电源键而非 G12
- **串口控制台读不到输出**：芯片报告 `USB mode: USB-Serial/JTAG`，主机侧 DTR 控制线状态传递不完整时该外设会丢弃输出，需要再定位
- 计数行与警示行共用第 119 条扫描线（字模末行是空白衬底，实际看不出来）
- 两处历史撞码：`.-.-.` 给了 AR（不提供 `+`），`-...` 给了 B（不提供 `=`）

## 许可

暂未指定开源许可证。
