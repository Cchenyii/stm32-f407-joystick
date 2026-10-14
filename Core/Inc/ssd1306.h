#ifndef SSD1306_H
#define SSD1306_H

#include "stm32f4xx_hal.h"
#include <stdint.h>

#define SSD1306_WIDTH  128
#define SSD1306_HEIGHT 64
#define SSD1306_BLACK 0U
#define SSD1306_WHITE 1U

HAL_StatusTypeDef SSD1306_Init(I2C_HandleTypeDef *hi2c);
void SSD1306_Clear(void);
void SSD1306_Fill(uint8_t lit);
void SSD1306_DrawPixel(int16_t x, int16_t y, uint8_t lit);
void SSD1306_FillRect(int16_t x, int16_t y, int16_t width, int16_t height, uint8_t lit);
uint16_t SSD1306_TextWidth(const char *s, uint8_t scale);
void SSD1306_DrawText(int16_t x, int16_t y, const char *s, uint8_t scale, uint8_t lit);
uint16_t SSD1306_NumberWidth(const char *s);
void SSD1306_DrawNumber(int16_t x, int16_t y, const char *s, uint8_t lit);
void SSD1306_SetCursor(uint8_t x, uint8_t page);
void SSD1306_WriteString(const char *s);
void SSD1306_Update(void);

#endif
