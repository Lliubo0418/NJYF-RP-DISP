//  版 本 号   : v1.0
//  作    者   : Joyee
//  生成日期   : 2019-08-17
//  最近修改   : 
//  功能描述   : OLED演示例程(STM32系列)
//              说明: 
//              ----------------------------------------------------------------
//							CS     接PD6
//              RST    接PD8
//              A0     接PD10
//              SCL    接PD12
//              SDA    接PD14
//              VCC    3.3v电源
//              GND    电源地
//              ----------------------------------------------------------------
//******************************************************************************/
#ifndef __OLED_H
#define __OLED_H

//#include <stm32f10x.h>
#include "main.h"

#define LCD_W 128
#define LCD_H 64


			  
//-----------------OLED端口定义----------------  					   
#define OLED_CS_Clr()  HAL_GPIO_WritePin(SPI_CS_GPIO_Port,SPI_CS_Pin,GPIO_PIN_RESET)//CS
#define OLED_CS_Set()  HAL_GPIO_WritePin(SPI_CS_GPIO_Port,SPI_CS_Pin,GPIO_PIN_SET)

#define OLED_RST_Clr() HAL_GPIO_WritePin(OLED_RST_GPIO_Port,OLED_RST_Pin,GPIO_PIN_RESET)//RES
#define OLED_RST_Set() HAL_GPIO_WritePin(OLED_RST_GPIO_Port,OLED_RST_Pin,GPIO_PIN_SET)

#define OLED_A0_Clr() HAL_GPIO_WritePin(A0_GPIO_Port,A0_Pin,GPIO_PIN_RESET)//DC                         A0
#define OLED_A0_Set() HAL_GPIO_WritePin(A0_GPIO_Port,A0_Pin,GPIO_PIN_SET)
 		     

#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE           	 0x001F  
#define RED           	 0xF800
#define GREEN         	 0x07E0
#define YELLOW        	 0xFFE0
					  		 
extern  uint16_t BACK_COLOR, POINT_COLOR;   //背景色，画笔色

void LCD_WR_DATA8(char da); //发送数据-8位参数
void LCD_WR_DATA(int da);
void LCD_WR_REG(char da);
 
void LCD_FullFill( uint8_t FillData );
void LCD_ClearLine(uint8_t page);
void LCD_init(void);

void display(uint8_t dat1,uint8_t dat2);
void displaychar(uint8_t const *p);

void LCD_ShowChar(uint8_t col,uint8_t page,uint8_t Order);
void LCD_ShowStr(uint8_t col,uint8_t page,uint8_t *puts);
void LCD_ShowChinese(uint8_t col, uint8_t page, char *hz);
void LCD_ShowStrEx(uint8_t col, uint8_t page, uint8_t *puts);
void LCD_ShowStrExCompact(uint8_t col, uint8_t page, uint8_t *puts);
uint8_t LCD_ShowCharCompact(uint8_t col, uint8_t page, uint8_t ch);
void LCD_ShowNum(uint8_t col,uint8_t page,uint8_t Num);
void LCD_ShowBmp( uint8_t const *puts );

// 设置OLED显存地址 
void OLED_Set_Pos(uint8_t x, uint8_t y) ;
void LCD_ShowStr_Small(uint8_t col, uint8_t page, const char *str);
void LCD_ShowStr_Small_Compact(uint8_t col, uint8_t page, const char *str);



// mode: 0=右对齐(默认), 1=左对齐(文本前), 2=指定位置, 3=紧跟文本(间隔2列), 4=自动计算字符串宽度后显示
// col: 当mode=2/4时为文本起始列; 当mode=3时为文本结束位置
void LCD_ShowArrowEx(uint8_t page, uint8_t mode, uint8_t col, const char *str);

void LCD_DrawEchoCurve(uint8_t *echo_data, uint8_t x_scale, uint8_t y_scale);
void LCD_ShowCharReverse(uint8_t col, uint8_t page, uint8_t Order);



#endif  
	 
	 



