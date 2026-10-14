cmake_minimum_required(VERSION 3.20)
project(stm32_dma_regression LANGUAGES C)
include(CTest)
option(ENABLE_SANITIZERS "Address/undefined behavior checks on supported hosts" OFF)
add_executable(stm32_regression tests.c ../Core/Src/session_wire.c)
target_include_directories(stm32_regression PRIVATE mock ../Core/Inc)
target_compile_features(stm32_regression PRIVATE c_std_11)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(stm32_regression PRIVATE -Wall -Wextra -Werror)
endif()
if(ENABLE_SANITIZERS AND NOT WIN32 AND CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(stm32_regression PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(stm32_regression PRIVATE -fsanitize=address,undefined)
endif()
set(CASES init_guard dma_existing_selftest dma_partial dma_duplicate_event dma_wrap
  dma_half_and_complete dma_fifo_wrap dma_fifo_overflow_recovery dma_uart_error_recovery
  dma_restart_failure dma_random_chunks wire_existing_selftest handshake_retry
  confirmation_retry ack_noise_and_stale ack_timeout_retry failure_rehandshake b_reboot
  sensor_interleaved sensor_duplicate sensor_sequence_wrap sensor_stale sensor_validation)
foreach(case IN LISTS CASES)
  add_test(NAME ${case} COMMAND stm32_regression ${case})
  set_tests_properties(${case} PROPERTIES TIMEOUT 20)
endforeach()
