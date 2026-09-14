# STM32F407 摇杆采集与协议通信

基于 STM32F407VET6、HAL 和 FreeRTOS 的双轴摇杆演示：ADC 轮询采集、
低通滤波与方向死区、本地 SSD1306 OLED 显示，并通过 V1 二进制协议把
摇杆状态发送到 ESP32 B 板。

## 系统流程

```text
PA0/PA1 ADC1 ──> TaskA ──滤波/死区──> OLED
                         │
                    joyMutex
                         │
                         v
                       TaskB ──V1 frame / USART2──> ESP32 B
                              <────── ACK ────────
```

- `TaskA`：双通道扫描后轮询读取，更新 OLED 和最新摇杆快照
- `TaskB`：复制快照、组帧发送，并在未收到 ACK 时最多重试 3 次
- `joyMutex`：保护 `g_joy` 的跨任务读写，避免读取到半更新数据
- `uartMutex`：串行化两个任务对 USART1 调试口的输出
- `sensor_protocol.c`：V1 帧编码、CRC-16/CCITT 和 ACK 校验

## 硬件连接

| 外设 | STM32 引脚 | 说明 |
|------|------------|------|
| 摇杆 VRx / VRy | PA0 / PA1 | ADC1 IN0 / IN1 |
| 摇杆 SW | PA4 | 内部上拉，按下为低电平 |
| OLED SCL / SDA | PB6 / PB7 | I2C1，SSD1306 地址 `0x3C` |
| 调试串口 TX / RX | PA9 / PA10 | USART1，115200 |
| ESP32 链路 TX / RX | PA2 / PA3 | USART2，115200 |

摇杆电源接 **3.3V**，不要因模块丝印 `+5V` 而向 ADC 输入 5V。

### 与 ESP32 B 联调

| STM32F407 | ESP32 B |
|-----------|---------|
| PA2 (USART2_TX) | GPIO16 (RX2) |
| PA3 (USART2_RX) | GPIO17 (TX2) |
| GND | GND |

协议帧格式：
`AA 55 | Version | Type=0x04 | Sequence | Length | Payload(6) | CRC16`。
ESP32 B 返回 ACK；配套项目见
[esp32-ab-sensor](https://github.com/Cchenyii/esp32-ab-sensor)。

## 开发环境

- STM32CubeIDE 2.2.0
- STM32CubeMX 6.18.1
- STM32CubeF4 Firmware Package 1.28.3
- CMSIS-RTOS v2 / FreeRTOS
- 目标芯片：STM32F407VET6

## 导入、构建与运行

1. 克隆仓库：

```bash
git clone https://github.com/Cchenyii/stm32-f407-joystick.git
cd stm32-f407-joystick
```

2. 在 STM32CubeIDE 中选择
   `File > Import > Existing Projects into Workspace`。
3. 将 `STM32CubeIDE/` 选为工程根目录并导入 `CJY`。
4. 选择 `Debug` 配置，执行 `Project > Build Project`，然后烧录运行。
5. USART1 应输出 `boot`、`oled ok` 和 `TX seq=... ACK`；ESP32 B
   固件版本应不低于 `1.2.0`，其 USB 串口会输出 `JOY seq=...`。

## CubeMX 配置与重新生成

`CJY.ioc` 已包含 ADC1、I2C1、USART1、USART2、PA4 和 FreeRTOS 任务配置。
在 STM32CubeIDE 中打开该文件并选择 **Generate Code**，重新生成后不会
丢失 ADC1/USART2 引脚和初始化代码。

若使用独立 STM32CubeMX，可从仓库根目录运行：

```powershell
STM32CubeMX.exe -q regen_cubemx.txt
```

脚本使用相对路径，不依赖任何个人电脑目录。生成前建议保持 Git 工作区
干净，以便审查 CubeMX 版本升级带来的差异。

## 目录结构

```text
Core/                 应用代码、协议和 OLED 驱动
Drivers/              STM32 HAL 与 CMSIS
Middlewares/          FreeRTOS
STM32CubeIDE/         CubeIDE 工程文件与链接脚本
CJY.ioc               CubeMX 硬件及中间件配置
regen_cubemx.txt      CubeMX 命令行生成脚本
```

## 许可证

本项目原创代码使用 [MIT License](LICENSE)。STMicroelectronics、ARM 和
FreeRTOS 提供的第三方文件继续遵循各自文件头及组件许可证。
