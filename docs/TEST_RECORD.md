# 实机测试记录

记录对象：本仓库默认分支最终固件（CubeIDE Debug 构建）。  
测试方式：STM32CubeIDE 2.2.0 编译后经 ST-Link 烧录；USART1（PA9/PA10）115200 看调试打印；OLED 目视；与 ESP32 B 板（固件 ≥ 1.2.0）UART 联调。

本文件只写板上实际出现过的现象，不写未统计的成功率或长时间压测数字。

## 1. 构建

| 项 | 结果 |
|----|------|
| 工程 | `STM32CubeIDE/` 中的 `CJY`，配置 `Debug` |
| 编译 | 0 errors，0 warnings |
| 体积（一次 Clean Build） | text 35636 B，data 104 B，bss 22088 B |
| 下载 | ST-Link `Download verified successfully` |

## 2. 上电与本地外设

| 步骤 | 期望 | 结果 |
|------|------|------|
| 复位后 USART1 | `boot` | 通过 |
| SSD1306（PB6/PB7，地址 0x3C） | `oled ok`，屏上 `F407 JOYSTICK` | 通过 |
| USART2 链路提示 | `USART2->ESP32B V1` | 通过 |
| 摇杆 ADC（PA0/PA1） | 串口周期性 `X=... Y=... CENTER/LEFT/RIGHT/UP/DOWN SW=...` | 通过 |
| 死区 | 中心附近小幅晃动保持 `CENTER` | 通过 |
| 方向 | 明确推到四向后打印对应方向名，OLED 同步刷新 | 通过 |
| 按键 PA4 | 按下 `SW=1`，松开 `SW=0` | 通过 |

摇杆模块电源接 3.3 V。ADC 为双通道扫描后轮询读取，不使用 DMA 完成中断。

## 3. 与 ESP32 B 的 V1 联调

接线：PA2 → B GPIO16，PA3 → B GPIO17，GND 共地。B 板需已打印 `UART2 joystick link: RX=16 TX=17`。

| 步骤 | 期望 | 结果 |
|------|------|------|
| STM32 USART1 | `TX seq=1 ACK`，序号递增且持续 `ACK` | 通过 |
| ESP32 B USB | `JOY seq=... X=... Y=... ... SW=...` | 通过 |
| 未接 B 或 TX/RX 反接 | STM32 侧出现 `NOACK` | 通过（用于确认接线） |

协议：`AA 55 | Ver | Type=0x04 | Seq | Len | Payload(6) | CRC16`，B 回 ACK；未收到 ACK 时最多再发 2 次（共 3 次）。

配套 ESP32 记录：[esp32-ab-sensor `docs/TEST_RECORD.md`](https://github.com/Cchenyii/esp32-ab-sensor/blob/master/docs/TEST_RECORD.md)。

## 4. 跨任务状态

`TaskA` 写摇杆快照、`TaskB` 读快照组帧。最终固件用 `joyMutex` 整包拷贝 `g_joy`，USART1 打印用 `uartMutex`。联调期间 OLED 与 UART 帧方向一致，未再出现半更新乱码。

## 5. 未纳入本记录的内容

- 未做 24 小时不间断计数统计。
- CubeMX 重新 Generate Code 以 IDE 打开 `.ioc` 为准；本记录验证的是当前已提交源码与烧录固件。
