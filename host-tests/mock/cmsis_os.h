#pragma once
#include <stdint.h>
typedef void *osThreadId_t;
void osDelay(uint32_t ticks);
osThreadId_t osThreadGetId(void);
uint32_t osThreadGetStackSpace(osThreadId_t task);
