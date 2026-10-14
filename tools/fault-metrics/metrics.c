/* The existing fixtures include the actual UART implementation once.
 * Only HAL, DMA registers, remote peer and RTOS clock are simulated. */
#define main regression_test_main
#include "tests.c"
#undef main

static void item(const char *name, int first) {
    printf("%s{\"case\":\"%s\",\"passed\":true,\"mock_elapsed_ms\":%lu,"
           "\"requests\":%lu,\"confirms\":%lu,\"joystick_tx\":%lu,"
           "\"received_bytes\":%lu,\"consumed_bytes\":%lu,"
           "\"fifo_overflows\":%lu,\"uart_errors\":%lu,\"dma_errors\":%lu,"
           "\"restarts\":%lu,\"parser_rejected\":%lu,"
           "\"sensor_received\":%lu,\"sensor_duplicates\":%lu,\"sensor_acks\":%lu}",
           first ? "" : ",", name, (unsigned long)mock_tick,
           (unsigned long)requests, (unsigned long)confirms, (unsigned long)joystick_tx,
           (unsigned long)g_spLinkRxStats.received_bytes, (unsigned long)g_spLinkRxStats.consumed_bytes,
           (unsigned long)g_spLinkRxStats.fifo_overflows, (unsigned long)g_spLinkRxStats.uart_errors,
           (unsigned long)g_spLinkRxStats.dma_errors, (unsigned long)g_spLinkRxStats.restarts,
           (unsigned long)parser.rejected, (unsigned long)sensor_cache.received_count,
           (unsigned long)sensor_cache.duplicate_count, (unsigned long)sensor_ack_count);
}
int main(void) {
    const char *cases[] = {"handshake_retry", "confirmation_retry", "ack_noise_and_stale",
      "ack_timeout_retry", "failure_rehandshake", "sensor_duplicate", "sensor_sequence_wrap",
      "sensor_stale", "dma_fifo_overflow_recovery", "dma_uart_error_recovery",
      "dma_restart_failure", "dma_random_chunks"};
    printf("{\"environment\":\"production UART source with HAL/RTOS simulation\","
           "\"timing_kind\":\"mock RTOS clock, not physical device timing\",\"cases\":[");
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        fresh();
        if (!strncmp(cases[i], "dma_", 4)) bytes_case(cases[i]);
        else link_case(cases[i]);
        item(cases[i], i == 0);
    }
    puts("]}");
    return 0;
}
