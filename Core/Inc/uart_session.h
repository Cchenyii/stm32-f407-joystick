#pragma once
#include "stm32f4xx_hal.h"
#include <stdint.h>
/* One owner: TaskB. Equal-priority DMA/USART IRQs only produce receive bytes. */
int SpLink_Init(UART_HandleTypeDef *uart);
int SpLink_SendJoystick(uint16_t x, uint16_t y, uint8_t direction, uint8_t sw);
uint64_t SpLink_GetSession(void);
uint32_t SpLink_GetLastSequence(void);
typedef struct {
    uint8_t valid, link_alive, wifi_connected, tcp_connected, has_data, has_session;
    int16_t temperature_centi_c;
    uint16_t humidity_centi_percent, distance_cm;
    uint32_t source_connection, source_sequence, relay_sequence;
    uint32_t valid_for_ms, received_ms, last_link_ms;
    uint32_t received_count, duplicate_count;
} SpLink_SensorSnapshot;
/* TaskB services the UART; TaskA obtains a short protected snapshot. */
void SpLink_Poll(void);
void SpLink_GetSensorSnapshot(SpLink_SensorSnapshot *output);

typedef struct {
    uint32_t received_bytes, consumed_bytes;
    uint32_t idle_events, half_events, complete_events;
    uint32_t fifo_overflows, uart_errors, dma_errors, restarts;
    uint32_t fifo_high_water, max_chunk_bytes;
    uint32_t task_stack_free_bytes, max_ack_wait_ms;
    uint32_t task_a_stack_free_bytes, free_heap_bytes, minimum_free_heap_bytes;
    uint32_t self_test_passed;
} SpLink_RxStats;
/* Public debugger-visible counters; obtain a coherent software snapshot via API. */
extern volatile SpLink_RxStats g_spLinkRxStats;
void SpLink_GetRxStats(SpLink_RxStats *output);
