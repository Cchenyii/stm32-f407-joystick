cmake_minimum_required(VERSION 3.20)
project(stm32_f407_firmware LANGUAGES C ASM)
set(ROOT "${CMAKE_CURRENT_LIST_DIR}/..")
file(GLOB APP_SOURCES CONFIGURE_DEPENDS "${ROOT}/Core/Src/*.c")
file(GLOB RTOS_SOURCES CONFIGURE_DEPENDS "${ROOT}/Middlewares/Third_Party/FreeRTOS/Source/*.c")
set(HAL_MODULES hal hal_adc hal_adc_ex hal_cortex hal_dma hal_dma_ex hal_exti
  hal_flash hal_flash_ex hal_flash_ramfunc hal_gpio hal_i2c hal_i2c_ex hal_pwr
  hal_pwr_ex hal_rcc hal_rcc_ex hal_tim hal_tim_ex hal_uart)
set(HAL_SOURCES)
foreach(module IN LISTS HAL_MODULES)
  list(APPEND HAL_SOURCES "${ROOT}/Drivers/STM32F4xx_HAL_Driver/Src/stm32f4xx_${module}.c")
endforeach()
add_executable(CJY.elf ${APP_SOURCES} ${RTOS_SOURCES} ${HAL_SOURCES}
  "${ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2/cmsis_os2.c"
  "${ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F/port.c"
  "${ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/MemMang/heap_4.c"
  "${ROOT}/STM32CubeIDE/Application/User/Startup/startup_stm32f407vetx.s"
  "${ROOT}/STM32CubeIDE/Application/User/Startup/Core/syscalls.c"
  "${ROOT}/STM32CubeIDE/Application/User/Startup/Core/sysmem.c")
target_include_directories(CJY.elf PRIVATE "${ROOT}/Core/Inc"
  "${ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc" "${ROOT}/Drivers/STM32F4xx_HAL_Driver/Inc/Legacy"
  "${ROOT}/Drivers/CMSIS/Include" "${ROOT}/Drivers/CMSIS/Device/ST/STM32F4xx/Include"
  "${ROOT}/Middlewares/Third_Party/FreeRTOS/Source/include"
  "${ROOT}/Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2"
  "${ROOT}/Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F")
target_compile_definitions(CJY.elf PRIVATE USE_HAL_DRIVER STM32F407xx DEBUG)
target_compile_features(CJY.elf PRIVATE c_std_11)
set(CPU_FLAGS -mcpu=cortex-m4 -mthumb -mfpu=fpv4-sp-d16 -mfloat-abi=hard)
target_compile_options(CJY.elf PRIVATE ${CPU_FLAGS} -g3 -O0 -ffunction-sections -fdata-sections -Wall)
target_link_options(CJY.elf PRIVATE ${CPU_FLAGS} --specs=nosys.specs --specs=nano.specs
  "-T${ROOT}/STM32CubeIDE/STM32F407VETX_FLASH.ld" -Wl,--gc-sections "-Wl,-Map=${CMAKE_CURRENT_BINARY_DIR}/CJY.map")
target_link_libraries(CJY.elf PRIVATE m)
add_custom_command(TARGET CJY.elf POST_BUILD
  COMMAND "${CMAKE_SIZE}" "$<TARGET_FILE:CJY.elf>")
