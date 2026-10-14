#include "uart_session.h"
#include "session_wire.h"
#include "cmsis_os.h"
#include <string.h>

#define DMA_CAPACITY 256u
#define RX_CAPACITY 512u
static UART_HandleTypeDef *link_uart;
/* .bss lives in DMA-accessible SRAM, not the F407 CCM region. */
static volatile uint8_t rx_dma[DMA_CAPACITY] __attribute__((aligned(4)));
static uint8_t rx_ring[RX_CAPACITY];
static uint16_t dma_position;
static volatile uint16_t rx_head, rx_tail;
static volatile uint8_t rx_overflow, rx_restart;
volatile SpLink_RxStats g_spLinkRxStats;
static sw_parser_t parser;
static uint64_t session;
static uint32_t sequence, last_sequence;
static unsigned failures;
static SpLink_SensorSnapshot sensor_cache;
static uint32_t last_relay_sequence;
static uint8_t has_relay_sequence;

static void set_session(uint64_t token) {
    session = token; has_relay_sequence = 0;
    uint32_t saved = __get_PRIMASK(); __disable_irq();
    memset(&sensor_cache, 0, sizeof(sensor_cache));
    sensor_cache.has_session = token != 0;
    sensor_cache.last_link_ms = HAL_GetTick();
    __DMB(); __set_PRIMASK(saved);
}
static void mark_link_alive(void) {
    uint32_t saved = __get_PRIMASK(); __disable_irq();
    sensor_cache.last_link_ms = HAL_GetTick();
    __DMB(); __set_PRIMASK(saved);
}
void SpLink_GetSensorSnapshot(SpLink_SensorSnapshot *output) {
    if (!output) return;
    uint32_t saved = __get_PRIMASK(); __disable_irq();
    *output = sensor_cache;
    __DMB(); __set_PRIMASK(saved);
    uint32_t now = HAL_GetTick();
    output->link_alive = output->has_session && (uint32_t)(now - output->last_link_ms) < 3000u;
    output->valid = output->valid && output->link_alive &&
                    (uint32_t)(now - output->received_ms) < output->valid_for_ms;
}

void SpLink_GetRxStats(SpLink_RxStats *output) {
    if (!output) return;
    uint32_t saved = __get_PRIMASK(); __disable_irq();
    *output = g_spLinkRxStats;
    __DMB(); __set_PRIMASK(saved);
}
static void copy_pending(uint16_t position) {
    __DMB();
    const uint16_t pending = (uint16_t)((position - dma_position) & (DMA_CAPACITY - 1u));
    if (pending > g_spLinkRxStats.max_chunk_bytes) g_spLinkRxStats.max_chunk_bytes = pending;
    g_spLinkRxStats.received_bytes += pending;
    for (uint16_t count = 0; count < pending; ++count) {
        const uint16_t next = (uint16_t)((rx_head + 1u) & (RX_CAPACITY - 1u));
        if (!rx_overflow && next == rx_tail) {
            rx_overflow = 1; ++g_spLinkRxStats.fifo_overflows;
        }
        if (!rx_overflow && !rx_restart) {
            rx_ring[rx_head] = rx_dma[dma_position]; __DMB(); rx_head = next;
            uint16_t used = (uint16_t)((rx_head - rx_tail) & (RX_CAPACITY - 1u));
            if (used > g_spLinkRxStats.fifo_high_water) g_spLinkRxStats.fifo_high_water = used;
        }
        dma_position = (uint16_t)((dma_position + 1u) & (DMA_CAPACITY - 1u));
    }
}
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *uart, uint16_t size) {
    (void)size;
    if (uart != link_uart || !uart->hdmarx) return;
    const HAL_UART_RxEventTypeTypeDef event = HAL_UARTEx_GetRxEventType(uart);
    if (event == HAL_UART_RXEVENT_IDLE) ++g_spLinkRxStats.idle_events;
    else if (event == HAL_UART_RXEVENT_HT) ++g_spLinkRxStats.half_events;
    else if (event == HAL_UART_RXEVENT_TC) ++g_spLinkRxStats.complete_events;
    /* Read current NDTR rather than an older callback Size: an IDLE IRQ can
       run before a pending HT IRQ. Equal IRQ priorities serialize producers. */
    uint16_t position = (uint16_t)(DMA_CAPACITY - __HAL_DMA_GET_COUNTER(uart->hdmarx));
    if (position == DMA_CAPACITY) position = 0;
    copy_pending(position);
}
void HAL_UART_ErrorCallback(UART_HandleTypeDef *uart) {
    if (uart != link_uart) return;
    ++g_spLinkRxStats.uart_errors;
    if (uart->ErrorCode & HAL_UART_ERROR_DMA) ++g_spLinkRxStats.dma_errors;
    /* HAL stops DMA on blocking errors. Abort/restart runs in TaskB, never ISR. */
    rx_restart = 1;
}
static int pop_byte(uint8_t *byte) {
    if (rx_overflow || rx_restart) {
        if (HAL_UART_AbortReceive(link_uart) != HAL_OK) return 0;
        uint32_t saved = __get_PRIMASK(); __disable_irq();
        rx_head = rx_tail = dma_position = 0; rx_overflow = rx_restart = 0;
        memset(&parser, 0, sizeof(parser));
        __DMB();
        __set_PRIMASK(saved);
        __HAL_UART_CLEAR_OREFLAG(link_uart);
        if (HAL_UARTEx_ReceiveToIdle_DMA(link_uart, (uint8_t *)rx_dma, DMA_CAPACITY) != HAL_OK) {
            rx_restart = 1; return 0;
        }
        ++g_spLinkRxStats.restarts;
    }
    if (rx_tail == rx_head) return 0;
    __DMB(); *byte = rx_ring[rx_tail];
    __DMB(); rx_tail = (uint16_t)((rx_tail + 1u) & (RX_CAPACITY - 1u));
    ++g_spLinkRxStats.consumed_bytes;
    return 1;
}
/* Runs before DMA starts. Exercise the actual cursor/FIFO functions, using
   a known byte pattern; hardware IRQ ordering remains a separate acceptance. */
static int check_bytes(unsigned start, unsigned count) {
    uint8_t byte;
    for (unsigned i = 0; i < count; ++i)
        if (!pop_byte(&byte) || byte != (uint8_t)(start + i)) return 0;
    return rx_head == rx_tail;
}
static int receive_self_test(void) {
    for (unsigned i = 0; i < DMA_CAPACITY; ++i) rx_dma[i] = (uint8_t)i;
    rx_head = rx_tail = dma_position = 0; rx_overflow = rx_restart = 0;
    copy_pending(32); if (!check_bytes(0, 32)) return 0;
    copy_pending(32); if (rx_head != rx_tail) return 0;
    copy_pending(128); if (!check_bytes(32, 96)) return 0;
    copy_pending(0); if (!check_bytes(128, 128)) return 0;
    copy_pending(17); if (!check_bytes(0, 17)) return 0;
    dma_position = 240; copy_pending(16); if (!check_bytes(240, 32)) return 0;
    rx_head = rx_tail = 500; dma_position = 0;
    copy_pending(20); if (!check_bytes(0, 20)) return 0;
    rx_head = rx_tail = dma_position = 0;
    copy_pending(128); copy_pending(0); copy_pending(128); copy_pending(0);
    if (!rx_overflow || g_spLinkRxStats.fifo_overflows != 1 ||
        ((rx_head - rx_tail) & (RX_CAPACITY - 1u)) != RX_CAPACITY - 1u) return 0;
    rx_head = rx_tail = dma_position = 0; rx_overflow = rx_restart = 0;
    memset((void *)&g_spLinkRxStats, 0, sizeof(g_spLinkRxStats));
    return 1;
}
static int transmit(uint8_t type, uint32_t seq, const uint8_t *payload, uint16_t length);
static void receive_sensor(const sw_frame_t *frame) {
    if (frame->length < 8) return;
    uint64_t token = sw_read_token(frame->payload);
    uint8_t status = SW_OK;
    sw_sensor_t sensor;
    if (!session || token != session) status = SW_NEED_SESSION;
    else if (!sw_decode_sensor(frame, &sensor)) status = SW_BAD_LENGTH;
    else {
        uint32_t delta = frame->sequence - last_relay_sequence;
        int duplicate = has_relay_sequence && (delta == 0 || delta >= 0x80000000u);
        uint32_t saved = __get_PRIMASK(); __disable_irq();
        sensor_cache.last_link_ms = HAL_GetTick();
        if (duplicate) ++sensor_cache.duplicate_count;
        else {
            last_relay_sequence = frame->sequence; has_relay_sequence = 1;
            sensor_cache.valid = (sensor.flags & SW_SENSOR_VALID) != 0;
            sensor_cache.wifi_connected = (sensor.flags & SW_WIFI_READY) != 0;
            sensor_cache.tcp_connected = (sensor.flags & SW_TCP_CONNECTED) != 0;
            sensor_cache.has_data = (sensor.flags & SW_SENSOR_SEEN) != 0;
            sensor_cache.temperature_centi_c = sensor.temperature_centi_c;
            sensor_cache.humidity_centi_percent = sensor.humidity_centi_percent;
            sensor_cache.distance_cm = sensor.distance_cm;
            sensor_cache.source_connection = sensor.source_connection;
            sensor_cache.source_sequence = sensor.source_sequence;
            sensor_cache.relay_sequence = frame->sequence;
            sensor_cache.valid_for_ms = sensor.valid_for_ms;
            sensor_cache.received_ms = HAL_GetTick();
            ++sensor_cache.received_count;
        }
        __DMB(); __set_PRIMASK(saved);
    }
    uint8_t payload[9]; sw_write_token(payload, token); payload[8] = status;
    transmit(SW_SENSOR_ACK, frame->sequence, payload, sizeof(payload));
}
void SpLink_Poll(void) {
    if (!link_uart) return;
    uint8_t byte; sw_frame_t frame;
    for (unsigned count = 0; count < RX_CAPACITY && pop_byte(&byte); ++count) {
        if (sw_parser_feed(&parser, byte, &frame) && frame.type == SW_SENSOR)
            receive_sensor(&frame);
    }
}
static int wait_frame(uint8_t type, uint32_t expected, uint64_t token,
                      uint32_t timeout, sw_frame_t *frame) {
    uint32_t start = HAL_GetTick();
    while ((uint32_t)(HAL_GetTick() - start) < timeout) {
        uint8_t byte;
        while (pop_byte(&byte)) {
            if (!sw_parser_feed(&parser, byte, frame)) continue;
            if (frame->type == SW_SENSOR) { receive_sensor(frame); continue; }
            if (frame->type != type || frame->sequence != expected) continue;
            if (type == SW_OFFER && frame->length == 8 && sw_read_token(frame->payload)) return 1;
            if (type == SW_ACK && frame->length == 9 && sw_read_token(frame->payload) == token) {
                mark_link_alive(); return 1;
            }
        }
        osDelay(1);
    }
    return 0;
}
static int transmit(uint8_t type, uint32_t seq, const uint8_t *payload, uint16_t length) {
    uint8_t frame[SW_MAX_FRAME];
    size_t size = sw_encode(type, seq, payload, length, frame, sizeof(frame));
    return size && HAL_UART_Transmit(link_uart, frame, (uint16_t)size, 50) == HAL_OK;
}
static int establish(void) {
    sw_frame_t frame;
    uint64_t candidate = 0;
    for (unsigned attempt = 0; attempt < 3 && !candidate; ++attempt) {
        if (transmit(SW_REQUEST, 0, NULL, 0) && wait_frame(SW_OFFER, 0, 0, 150, &frame))
            candidate = sw_read_token(frame.payload);
    }
    if (!candidate) return 0;
    uint8_t payload[8]; sw_write_token(payload, candidate);
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        if (!transmit(SW_CONFIRM, 0, payload, 8) ||
            !wait_frame(SW_ACK, 0, candidate, 150, &frame)) continue;
        if (frame.payload[8] != SW_OK) return 0;
        set_session(candidate); sequence = 1; failures = 0;
        return 1;
    }
    return 0;
}
int SpLink_Init(UART_HandleTypeDef *uart) {
    if (!uart || !sw_wire_self_test()) return 0;
    link_uart = uart; rx_head = rx_tail = 0; rx_overflow = 0;
    dma_position = 0; rx_restart = 0;
    memset((void *)&g_spLinkRxStats, 0, sizeof(g_spLinkRxStats));
    set_session(0); sequence = 1; last_sequence = 0; failures = 0;
    memset(&parser, 0, sizeof(parser));
    if (!receive_self_test()) return 0;
    g_spLinkRxStats.self_test_passed = 1;
    __HAL_UART_CLEAR_OREFLAG(uart);
    if (!uart->hdmarx || uart->hdmarx->Init.Mode != DMA_CIRCULAR) return 0;
    return HAL_UARTEx_ReceiveToIdle_DMA(uart, (uint8_t *)rx_dma, DMA_CAPACITY) == HAL_OK;
}
int SpLink_SendJoystick(uint16_t x, uint16_t y, uint8_t direction, uint8_t sw) {
    SpLink_Poll();
    if (!link_uart || (!session && !establish())) return 0;
    uint8_t payload[14]; sw_write_token(payload, session);
    payload[8] = (uint8_t)(x >> 8); payload[9] = (uint8_t)x;
    payload[10] = (uint8_t)(y >> 8); payload[11] = (uint8_t)y;
    payload[12] = direction; payload[13] = sw ? 1u : 0u;
    last_sequence = sequence;
    int acknowledged = 0;
    sw_frame_t frame;
    for (unsigned attempt = 0; attempt < 3; ++attempt) {
        const uint32_t started = HAL_GetTick();
        if (!transmit(SW_JOYSTICK, sequence, payload, 14) ||
            !wait_frame(SW_ACK, sequence, session, 120, &frame)) continue;
        if (frame.payload[8] == SW_NEED_SESSION) { set_session(0); return 0; }
        if (frame.payload[8] == SW_OK) {
            acknowledged = 1;
            uint32_t elapsed = HAL_GetTick() - started;
            if (elapsed > g_spLinkRxStats.max_ack_wait_ms) g_spLinkRxStats.max_ack_wait_ms = elapsed;
        }
        break;
    }
    ++sequence; if (!sequence) ++sequence;
    if (acknowledged) failures = 0;
    else if (++failures >= 3) set_session(0);
    g_spLinkRxStats.task_stack_free_bytes = osThreadGetStackSpace(osThreadGetId());
    return acknowledged;
}
uint64_t SpLink_GetSession(void) { return session; }
uint32_t SpLink_GetLastSequence(void) { return last_sequence; }
