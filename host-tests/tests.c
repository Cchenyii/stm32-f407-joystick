#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* Compile the actual firmware translation unit once. This also makes its
   private cursor helpers testable without adding test hooks to firmware. */
#include "../Core/Src/uart_session.c"

#define CHECK(value) do {if(!(value)) {fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#value); exit(1);}} while(0)
uint32_t mock_primask;
static uint32_t mock_tick, starts, aborts, requests, confirms, joystick_tx;
static uint32_t sensor_ack_count, sensor_ack_status;
static uint64_t remote_seed, remote_offer, remote_active;
static uint8_t *dma_buffer;
static uint16_t dma_size, dma_cursor;
static int start_fails, no_reply, wrong_ack, mixed_ack, interleave_sensor;
static int drop_first_offer, drop_first_confirm;
static DMA_Stream_TypeDef stream;
static DMA_HandleTypeDef dma;
static UART_HandleTypeDef uart;

uint32_t HAL_GetTick(void) {return mock_tick;}
void osDelay(uint32_t ticks) {mock_tick+=ticks;}
osThreadId_t osThreadGetId(void) {return (void *)&uart;}
uint32_t osThreadGetStackSpace(osThreadId_t task) {(void)task; return 1536;}
HAL_UART_RxEventTypeTypeDef HAL_UARTEx_GetRxEventType(UART_HandleTypeDef *handle) {return handle->RxEventType;}
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *handle) {++aborts; handle->RxState=HAL_UART_STATE_READY; return HAL_OK;}
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *handle,uint8_t *buffer,uint16_t size) {
    if(start_fails) return HAL_ERROR;
    ++starts; dma_buffer=buffer; dma_size=size; dma_cursor=0;
    handle->hdmarx->Instance->NDTR=size; handle->RxState=1; return HAL_OK;
}
static void feed(const uint8_t *bytes,size_t length,int idle) {
    CHECK(dma_buffer && dma_size==DMA_CAPACITY);
    for(size_t i=0;i<length;++i) {
        dma_buffer[dma_cursor++]=bytes[i];
        if(dma_cursor==dma_size) dma_cursor=0;
        stream.NDTR=dma_size-dma_cursor;
        if(dma_cursor==dma_size/2) {uart.RxEventType=HAL_UART_RXEVENT_HT; HAL_UARTEx_RxEventCallback(&uart,dma_size/2);}
        if(!dma_cursor) {uart.RxEventType=HAL_UART_RXEVENT_TC; HAL_UARTEx_RxEventCallback(&uart,dma_size);}
    }
    if(idle) {uart.RxEventType=HAL_UART_RXEVENT_IDLE; HAL_UARTEx_RxEventCallback(&uart,dma_cursor?dma_cursor:dma_size);}
}
static void frame_in(uint8_t type,uint32_t seq,const uint8_t *payload,uint16_t length) {
    uint8_t frame[SW_MAX_FRAME]; size_t size=sw_encode(type,seq,payload,length,frame,sizeof(frame));
    CHECK(size); feed(frame,size,1);
}
static void ack_in(uint64_t token,uint32_t seq,uint8_t status) {
    uint8_t p[9]; sw_write_token(p,token); p[8]=status; frame_in(SW_ACK,seq,p,sizeof(p));
}
static void sensor_in(uint64_t token,uint32_t seq,uint8_t flags) {
    sw_sensor_t sensor={0}; sensor.token=token; sensor.source_connection=7;
    sensor.source_sequence=seq; sensor.valid_for_ms=4000; sensor.temperature_centi_c=-123;
    sensor.humidity_centi_percent=5500; sensor.distance_cm=19; sensor.flags=flags;
    uint8_t bytes[SW_MAX_FRAME]; size_t size=sw_encode_sensor(seq,&sensor,bytes,sizeof(bytes));
    CHECK(size); feed(bytes,size,1);
}
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *handle,uint8_t *bytes,uint16_t length,uint32_t timeout) {
    (void)handle; (void)timeout;
    sw_parser_t p={0}; sw_frame_t frame={0}; unsigned ready=0;
    for(unsigned i=0;i<length;++i) ready+=(unsigned)sw_parser_feed(&p,bytes[i],&frame);
    CHECK(ready==1);
    if(frame.type==SW_SENSOR_ACK) {++sensor_ack_count; sensor_ack_status=frame.payload[8]; return HAL_OK;}
    if(frame.type==SW_REQUEST) {
        ++requests; if(!remote_offer) remote_offer=++remote_seed;
        if(no_reply || (drop_first_offer && requests==1)) return HAL_OK;
        uint8_t payload[8]; sw_write_token(payload,remote_offer); frame_in(SW_OFFER,0,payload,8);
    } else if(frame.type==SW_CONFIRM) {
        ++confirms; uint64_t token=sw_read_token(frame.payload);
        if(token==remote_offer) {remote_active=token; remote_offer=0;}
        if(no_reply || (drop_first_confirm && confirms==1)) return HAL_OK;
        ack_in(token,0,token==remote_active?SW_OK:SW_NEED_SESSION);
    } else if(frame.type==SW_JOYSTICK) {
        ++joystick_tx; uint64_t token=sw_read_token(frame.payload);
        if(no_reply) return HAL_OK;
        if(interleave_sensor) sensor_in(remote_active,100,15);
        if(mixed_ack) {
            const uint8_t noise[]={0x42,0xaa,0x00,0xff}; feed(noise,sizeof(noise),1);
            ack_in(token,frame.sequence+1,SW_OK); ack_in(token+1,frame.sequence,SW_OK);
            uint8_t bad[21],payload[9]; sw_write_token(payload,token); payload[8]=0;
            CHECK(sw_encode(SW_ACK,frame.sequence,payload,9,bad,sizeof(bad))==sizeof(bad));
            bad[20]^=1; feed(bad,sizeof(bad),1);
        }
        if(wrong_ack) {ack_in(token,frame.sequence+1,SW_OK); return HAL_OK;}
        ack_in(token,frame.sequence,token==remote_active?SW_OK:SW_NEED_SESSION);
    }
    return HAL_OK;
}
static void fresh(void) {
    mock_tick=mock_primask=starts=aborts=requests=confirms=joystick_tx=0;
    sensor_ack_count=sensor_ack_status=0; remote_seed=100; remote_offer=remote_active=0;
    start_fails=no_reply=wrong_ack=mixed_ack=interleave_sensor=drop_first_offer=drop_first_confirm=0;
    memset(&stream,0,sizeof(stream)); memset(&dma,0,sizeof(dma)); memset(&uart,0,sizeof(uart));
    dma.Instance=&stream; dma.Init.Mode=DMA_CIRCULAR; uart.hdmarx=&dma;
    CHECK(SpLink_Init(&uart));
}
static void bytes_case(const char *name) {
    uint8_t pattern[512]; for(unsigned i=0;i<sizeof(pattern);++i) pattern[i]=(uint8_t)i;
    if(!strcmp(name,"dma_existing_selftest")) {CHECK(g_spLinkRxStats.self_test_passed==1); return;}
    if(!strcmp(name,"dma_partial")) {feed(pattern,17,1); CHECK(check_bytes(0,17));}
    else if(!strcmp(name,"dma_duplicate_event")) {
        feed(pattern,17,1); uart.RxEventType=HAL_UART_RXEVENT_IDLE;
        HAL_UARTEx_RxEventCallback(&uart,17); CHECK(check_bytes(0,17)); CHECK(g_spLinkRxStats.received_bytes==17);
        /* A stale HT Size must not turn into a fabricated wrap. */
        uart.RxEventType=HAL_UART_RXEVENT_HT; HAL_UARTEx_RxEventCallback(&uart,128);
        CHECK(rx_head==rx_tail && g_spLinkRxStats.received_bytes==17);
    } else if(!strcmp(name,"dma_wrap")) {
        feed(pattern,240,1); CHECK(check_bytes(0,240));
        feed(pattern+240,40,1); CHECK(check_bytes(240,40)); CHECK(g_spLinkRxStats.complete_events==1);
    } else if(!strcmp(name,"dma_half_and_complete")) {
        feed(pattern,256,0); CHECK(check_bytes(0,256));
        CHECK(g_spLinkRxStats.half_events==1 && g_spLinkRxStats.complete_events==1);
    } else if(!strcmp(name,"dma_fifo_wrap")) {
        rx_head=rx_tail=500; feed(pattern,32,1); CHECK(check_bytes(0,32));
    } else if(!strcmp(name,"dma_fifo_overflow_recovery")) {
        feed(pattern,sizeof(pattern),1); CHECK(rx_overflow && g_spLinkRxStats.fifo_overflows==1);
        uint8_t byte; CHECK(!pop_byte(&byte)); CHECK(starts==2 && aborts==1 && g_spLinkRxStats.restarts==1);
        CHECK(!rx_overflow && !rx_restart); feed(pattern,17,1); CHECK(check_bytes(0,17));
    } else if(!strcmp(name,"dma_uart_error_recovery")) {
        uart.ErrorCode=HAL_UART_ERROR_ORE|HAL_UART_ERROR_DMA; HAL_UART_ErrorCallback(&uart);
        CHECK(rx_restart && !aborts); uint8_t byte; CHECK(!pop_byte(&byte));
        CHECK(g_spLinkRxStats.uart_errors==1 && g_spLinkRxStats.dma_errors==1 && g_spLinkRxStats.restarts==1);
        feed(pattern,7,1); CHECK(check_bytes(0,7));
    } else if(!strcmp(name,"dma_restart_failure")) {
        uart.ErrorCode=HAL_UART_ERROR_ORE; HAL_UART_ErrorCallback(&uart); start_fails=1;
        uint8_t byte; CHECK(!pop_byte(&byte) && rx_restart && !g_spLinkRxStats.restarts);
        start_fails=0; CHECK(!pop_byte(&byte)); CHECK(g_spLinkRxStats.restarts==1 && !rx_restart);
    } else if(!strcmp(name,"dma_random_chunks")) {
        uint32_t rng=0x20261014; uint32_t total=0;
        for(unsigned round=0;round<2000;++round) {
            rng=rng*1664525u+1013904223u; unsigned size=1+rng%200;
            for(unsigned i=0;i<size;++i) pattern[i]=(uint8_t)(total+i);
            feed(pattern,size,(int)(rng&1));
            uart.RxEventType=HAL_UART_RXEVENT_IDLE; HAL_UARTEx_RxEventCallback(&uart,dma_cursor);
            CHECK(check_bytes(total,size)); total+=size;
        }
        CHECK(g_spLinkRxStats.received_bytes==total && g_spLinkRxStats.consumed_bytes==total && !rx_overflow);
    } else CHECK(0);
}
static void link_case(const char *name) {
    if(!strcmp(name,"wire_existing_selftest")) {CHECK(sw_wire_self_test()); return;}
    if(!strcmp(name,"handshake_retry")) {
        drop_first_offer=1; CHECK(SpLink_SendJoystick(2000,2001,0,0)); CHECK(requests==2); return;
    }
    if(!strcmp(name,"confirmation_retry")) {
        drop_first_confirm=1; CHECK(SpLink_SendJoystick(2000,2001,0,0)); CHECK(confirms==2); return;
    }
    CHECK(SpLink_SendJoystick(2000,2001,0,0)); uint64_t token=SpLink_GetSession();
    if(!strcmp(name,"ack_noise_and_stale")) {
        mixed_ack=1; CHECK(SpLink_SendJoystick(2000,2001,0,0)); CHECK(parser.rejected>0);
    } else if(!strcmp(name,"ack_timeout_retry")) {
        uint32_t before=joystick_tx; wrong_ack=1;
        CHECK(!SpLink_SendJoystick(2000,2001,0,0)); CHECK(joystick_tx-before==3 && mock_tick>=360);
    } else if(!strcmp(name,"failure_rehandshake")) {
        no_reply=1; for(unsigned i=0;i<3;++i) CHECK(!SpLink_SendJoystick(2000,2001,0,0));
        CHECK(!SpLink_GetSession()); no_reply=0; CHECK(SpLink_SendJoystick(2000,2001,0,0));
        CHECK(SpLink_GetSession()!=token && SpLink_GetLastSequence()==1);
    } else if(!strcmp(name,"b_reboot")) {
        remote_active=remote_offer=0; remote_seed+=100;
        CHECK(!SpLink_SendJoystick(2000,2001,0,0) && !SpLink_GetSession());
        CHECK(SpLink_SendJoystick(2000,2001,0,0) && SpLink_GetSession()!=token);
    } else if(!strcmp(name,"sensor_interleaved")) {
        interleave_sensor=1; CHECK(SpLink_SendJoystick(2000,2001,0,0));
        SpLink_SensorSnapshot snap; SpLink_GetSensorSnapshot(&snap);
        CHECK(snap.valid && snap.received_count==1 && snap.temperature_centi_c==-123 && sensor_ack_status==SW_OK);
    } else if(!strcmp(name,"sensor_duplicate")) {
        sensor_in(token,100,15); SpLink_Poll(); sensor_in(token,100,15); SpLink_Poll(); sensor_in(token,99,15); SpLink_Poll();
        SpLink_SensorSnapshot snap; SpLink_GetSensorSnapshot(&snap);
        CHECK(snap.received_count==1 && snap.duplicate_count==2 && sensor_ack_count==3 && snap.source_sequence==100);
    } else if(!strcmp(name,"sensor_sequence_wrap")) {
        sensor_in(token,0xfffffffeu,15); SpLink_Poll(); sensor_in(token,1,15); SpLink_Poll(); sensor_in(token,0xffffffffu,15); SpLink_Poll();
        SpLink_SensorSnapshot snap; SpLink_GetSensorSnapshot(&snap);
        CHECK(snap.received_count==2 && snap.duplicate_count==1 && snap.source_sequence==1);
    } else if(!strcmp(name,"sensor_stale")) {
        sensor_in(token,100,15); SpLink_Poll(); osDelay(4001); CHECK(SpLink_SendJoystick(2000,2001,0,0));
        SpLink_SensorSnapshot snap; SpLink_GetSensorSnapshot(&snap); CHECK(snap.link_alive && !snap.valid);
    } else if(!strcmp(name,"sensor_validation")) {
        sensor_in(token,100,0x80); SpLink_Poll(); CHECK(sensor_ack_status==SW_BAD_LENGTH && !sensor_cache.received_count);
        sensor_in(token+1,101,15); SpLink_Poll(); CHECK(sensor_ack_status==SW_NEED_SESSION && !sensor_cache.received_count);
        sensor_in(token,102,15); SpLink_Poll(); CHECK(sensor_ack_status==SW_OK && sensor_cache.received_count==1);
    } else CHECK(0);
}
int main(int argc,char **argv) {
    if(argc!=2) return 2;
    fresh();
    const char *name=argv[1];
    if(!strcmp(name,"init_guard")) {
        CHECK(!SpLink_Init(NULL)); dma.Init.Mode=0; CHECK(!SpLink_Init(&uart));
    } else if(!strncmp(name,"dma_",4)) bytes_case(name);
    else link_case(name);
    printf("PASS %s\n",name); return 0;
}
