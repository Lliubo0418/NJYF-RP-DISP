#ifndef __OLED_H
#define __OLED_H

#include "main.h"

#define LCD_W 128
#define LCD_H 64

#define LCD_SEG_TOTAL 132

#define LCD_SCAN_ROT180 1

#define OLED_CS_Clr()  HAL_GPIO_WritePin(SPI_CS_GPIO_Port,SPI_CS_Pin,GPIO_PIN_RESET)
#define OLED_CS_Set()  HAL_GPIO_WritePin(SPI_CS_GPIO_Port,SPI_CS_Pin,GPIO_PIN_SET)

#define OLED_RST_Clr() HAL_GPIO_WritePin(OLED_RST_GPIO_Port,OLED_RST_Pin,GPIO_PIN_RESET)
#define OLED_RST_Set() HAL_GPIO_WritePin(OLED_RST_GPIO_Port,OLED_RST_Pin,GPIO_PIN_SET)

#define OLED_A0_Clr() HAL_GPIO_WritePin(A0_GPIO_Port,A0_Pin,GPIO_PIN_RESET)
#define OLED_A0_Set() HAL_GPIO_WritePin(A0_GPIO_Port,A0_Pin,GPIO_PIN_SET)

#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE           	 0x001F  
#define RED           	 0xF800
#define GREEN         	 0x07E0
#define YELLOW        	 0xFFE0

extern  uint16_t BACK_COLOR, POINT_COLOR;

void LCD_WR_DATA8(char da);
void LCD_WR_DATA(int da);
void LCD_WR_REG(char da);


void LCD_FullFill( uint8_t FillData );

void LCD_ClearLine(uint8_t page);

void LCD_FillBlock(uint8_t col, uint8_t page, uint8_t width, uint8_t pages, uint8_t data);

void LCD_init(void);


void display(uint8_t dat1,uint8_t dat2);

void displaychar(uint8_t const *p);

/* 显示 1 个 ASCII 字符 */
void LCD_ShowChar(uint8_t col,uint8_t page,uint8_t Order);
/* 显示纯 ASCII 串 */
void LCD_ShowStr(uint8_t col,uint8_t page,uint8_t *puts);

void LCD_ShowStrEx(uint8_t col, uint8_t page, uint8_t *puts);

void LCD_ShowStrExCompact(uint8_t col, uint8_t page, uint8_t *puts);

uint16_t LCD_GetStrExWidth(const char *str);

void LCD_ShowStrExBig(uint8_t col, uint8_t page, const char *str);

uint16_t LCD_GetStrBigWidth(const char *str);

uint8_t LCD_ShowCharCompact(uint8_t col, uint8_t page, uint8_t ch);

void LCD_ShowBmp( uint8_t const *puts );


void OLED_Set_Pos(uint8_t x, uint8_t y) ;

void LCD_ShowStr_Small(uint8_t col, uint8_t page, const char *str);

void LCD_ShowStr_Small_Compact(uint8_t col, uint8_t page, const char *str);

void LCD_SetCurveXEndLabel(const char *label);

/* 画一个 ">" 箭头 */
void LCD_ShowArrowEx(uint8_t page, uint8_t mode, uint8_t col, const char *str);

/* 绘制诊断回波曲线*/
void LCD_DrawEchoCurve(uint8_t *echo_data, uint8_t x_scale, uint8_t y_scale);
/* 反白显示 1 个 ASCII 字符 */
void LCD_ShowCharReverse(uint8_t col, uint8_t page, uint8_t Order);

#endif  
