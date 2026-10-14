#pragma once
#include <stdint.h>
#include <stdatomic.h>
typedef enum {HAL_OK=0,HAL_ERROR=1,HAL_BUSY=2,HAL_TIMEOUT=3} HAL_StatusTypeDef;
typedef enum {HAL_UART_RXEVENT_TC=0,HAL_UART_RXEVENT_HT=1,HAL_UART_RXEVENT_IDLE=2} HAL_UART_RxEventTypeTypeDef;
typedef struct {uint32_t NDTR;} DMA_Stream_TypeDef;
typedef struct {DMA_Stream_TypeDef *Instance; struct {uint32_t Mode;} Init;} DMA_HandleTypeDef;
typedef struct {DMA_HandleTypeDef *hdmarx; uint32_t RxState,ErrorCode; HAL_UART_RxEventTypeTypeDef RxEventType;} UART_HandleTypeDef;
#define DMA_CIRCULAR 2u
#define HAL_UART_STATE_READY 0u
#define HAL_UART_ERROR_DMA 0x10u
#define HAL_UART_ERROR_ORE 0x08u
#define __HAL_DMA_GET_COUNTER(handle) ((handle)->Instance->NDTR)
#define __HAL_UART_CLEAR_OREFLAG(handle) ((void)(handle))
extern uint32_t mock_primask;
static inline uint32_t __get_PRIMASK(void) {return mock_primask;}
static inline void __disable_irq(void) {mock_primask=1;}
static inline void __set_PRIMASK(uint32_t saved) {mock_primask=saved;}
static inline void __DMB(void) {atomic_signal_fence(memory_order_seq_cst);}
uint32_t HAL_GetTick(void);
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *uart);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *uart,uint8_t *buffer,uint16_t size);
HAL_UART_RxEventTypeTypeDef HAL_UARTEx_GetRxEventType(UART_HandleTypeDef *uart);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *uart,uint8_t *buffer,uint16_t size,uint32_t timeout);
