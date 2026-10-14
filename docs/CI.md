# STM32 电脑端回归与固件 CI

host-tests 编译实际 Core/Src/uart_session.c（通过测试翻译单元包含一次）与 session_wire.c。
mock/ 仅模拟 HAL 和 RTOS 接口：时钟、UART 收发、DMA NDTR 及内存屏障。协议解析、重试、会话状态、DMA 游标和 FIFO 算法均使用生产源码。

23 项 CTest 用例覆盖初始化边界、DMA 分片/跨尾/重复通知、FIFO 满后恢复、UART/DMA 错误恢复、接收重启失败、固定种子的 2000 轮分片、握手重试、错误/过期 ACK、超时与重新握手、B 重启、传感器交错收发、去重、序号环绕、过期及字段校验。

```sh
cmake -S host-tests -B build-host -DCMAKE_BUILD_TYPE=Debug
cmake --build build-host --parallel
ctest --test-dir build-host --output-on-failure
```

Windows MinGW 用户可运行 `host-tests/run.ps1`。
Linux CI 使用 GCC 与 Clang、严格编译警告和 ASan/UBSan，并保存 JUnit 报告 14 天。

firmware/ 提供独立的 ARM GNU CMake 构建，编译完整 STM32F407 应用、HAL、FreeRTOS 和启动/链接文件；不需要运行 CubeIDE。

```sh
cmake -S firmware -B build-firmware -DCMAKE_TOOLCHAIN_FILE=arm-none-eabi.cmake
cmake --build build-firmware --parallel
```

固件与 B 原生 ESP-IDF 会话协议配套。USART2_RX 为 DMA1 Stream5 / Channel4 循环模式，DMA/USART IRQ 同为优先级 6，TaskB 负责解析与恢复。
HAL 模拟不验证 DMA 实际时序、串口电气、OLED 或实际 RTOS 调度。电脑错误注入与物理故障注入分开记录。实际板上联调已在本机完成，用户确认 OLED、摇杆和按键正常。
