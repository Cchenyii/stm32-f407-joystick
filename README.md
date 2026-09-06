# STM32F407 摇杆采集演示

STM32CubeIDE + HAL + FreeRTOS 小项目：双轴摇杆 ADC 采样、方向死区判断、UART 打印、SSD1306 OLED 显示。

## 硬件

| 外设 | 引脚 |
|------|------|
| USART1 TX/RX | PA9 / PA10（ST-Link VCP，115200） |
| 摇杆 VRx / VRy | PA0 / PA1（ADC1） |
| 摇杆 SW | PA4（上拉输入，按下为有效） |
| OLED I2C SCL/SDA | PB6 / PB7（I2C1，SSD1306 地址 0x3C） |

摇杆模块丝印常为 `+5V`，接 **3.3V**，避免模拟输出超过 MCU 耐压。

## 软件要点

- FreeRTOS（CMSIS-RTOS v2）：TaskA 采集/显示，TaskB 心跳
- UART 互斥输出，避免双任务抢串口乱码
- ADC 轮询双通道 + 简单低通滤波 + 死区方向（CENTER/LEFT/RIGHT/UP/DOWN）
- 精简 SSD1306 驱动（`Core/Src/ssd1306.c`）

## 打开工程

1. 用 **STM32CubeIDE** 导入：`STM32CubeIDE/` 目录（不要只打开外层空文件夹）
2. Build → Run（ST-Link）
3. 串口：**COM口 / 115200**，应看到 `boot` / `oled ok` 以及 `X=... Y=... DIR SW=...`

## 说明

- `CJY.ioc` 当前以 I2C/USART/FreeRTOS 为主；ADC 与摇杆 GPIO 有部分在 USER CODE 中手动维护。重新 GENERATE 前请备份。
- 后续可扩展：队列解耦采样/显示、与 ESP32 做 UART 协议联动。
