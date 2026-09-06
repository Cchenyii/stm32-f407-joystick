#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64

HAL_StatusTypeDef SSD1306_Init(I2C_HandleTypeDef *hi2c);
void SSD1306_Clear(void);
void SSD1306_SetCursor(uint8_t x, uint8_t page);
void SSD1306_WriteString(const char *s);
void SSD1306_Update(void);

#endif
