# STM32F407 摇杆采集演示

STM32CubeIDE + HAL + FreeRTOS：双轴摇杆 ADC、方向死区、本地 OLED、UART 调试，并通过 **V1 同构协议** 把摇杆状态发给 ESP32 B 板。

## 硬件

| 外设 | 引脚 |
|------|------|
| USART1 TX/RX | PA9 / PA10（ST-Link VCP 调试，115200） |
| USART2 TX/RX | **PA2 / PA3** → ESP32 B `GPIO17 / GPIO16` |
| 摇杆 VRx / VRy | PA0 / PA1（ADC1） |
| 摇杆 SW | PA4（上拉，按下有效） |
| OLED I2C SCL/SDA | PB6 / PB7（SSD1306 `0x3C`） |

摇杆丝印 `+5V` 仍接 **3.3V**。

### 与 ESP32 B 联调接线

| STM32 | ESP32 B |
|-------|---------|
| PA2 (USART2_TX) | GPIO16 (RX2) |
| PA3 (USART2_RX) | GPIO17 (TX2) |
| GND | GND |

协议与 `esp32-ab-sensor` 的 V1 帧同构：`AA 55 | Ver | Type=0x04 JOYSTICK_DATA | Seq | Len | Payload(6) | CRC16`，B 回 ACK。

## 软件要点

- FreeRTOS：TaskA 采样/滤波/OLED；TaskB 组帧经 USART2 发送并等 ACK（最多 3 次）
- `Core/Src/sensor_protocol.c`：CRC-16/CCITT 与 ESP32 一致
- ADC 轮询双通道（避免 DMA 完成中断饿死）

## 打开工程

1. STM32CubeIDE 导入 `STM32CubeIDE/`
2. Build → Run
3. 调试串口（USART1）应看到 `boot` / `oled ok` / `TX seq=... ACK`
4. 烧录 B 板固件 ≥ `1.2.0`，USB 串口应打印 `JOY seq=...`

配套仓库：https://github.com/Cchenyii/esp32-ab-sensor
