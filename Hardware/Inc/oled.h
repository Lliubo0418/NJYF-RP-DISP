/* OLED 显示驱动（STM32F103 显示板）：屏为 ST7565/AiP31567 系 COG，单色 128x64，页地址模式。
 * 接线：CS=SPI_CS(PA)、RST=OLED_RST(PB)、A0(DC)=A0(PA)、SCL=PA5/SPI1_SCK、SDA=PA7/SPI1_MOSI（单线半双工）
 * 本板无 EEPROM：界面参数由主板经 PARAM_DUMP 帧下发，掉电回初值。 */
#ifndef __OLED_H
#define __OLED_H

#include "main.h"

/* 屏幕分辨率：宽 128 列、高 64 行（= 8 页 × 8 行）。⚠ oled.c 所有换行/回卷判断都依赖 LCD_W、LCD_H/8 */
#define LCD_W 128
#define LCD_H 64

/* 芯片总列数 132（SEG0~SEG131），屏体只用 128 列。⚠ 旋转 180° 的列地址补偿（LCD_SetPageCol）依赖它 */
#define LCD_SEG_TOTAL 132

/* 显示方向总开关：1 = 旋转 180°（屏体倒装），0 = 正装；由 MY（上下）+ MX（左右）同时取反实现。
 * ⚠ 改本宏要同步三处，缺一处画面就错位或甩出可见区：LCD_init() 的 0xC0/0xA0 ↔ 0xC8/0xA1、
 *   LCD_SetPageCol() 的列偏移（o ↔ 4-o）、LCD_ShowBmp()/LCD_DrawEchoCurve() 里写死的列起点。
 * 不改逻辑坐标语义：col 0 恒为可见区最左列、page 0 恒在最上方。 */
#define LCD_SCAN_ROT180 1

/* ---------------- OLED 端口定义 ---------------- */
/* CS：低电平选中 */
#define OLED_CS_Clr()  HAL_GPIO_WritePin(SPI_CS_GPIO_Port,SPI_CS_Pin,GPIO_PIN_RESET)
#define OLED_CS_Set()  HAL_GPIO_WritePin(SPI_CS_GPIO_Port,SPI_CS_Pin,GPIO_PIN_SET)

/* RST：低电平复位 */
#define OLED_RST_Clr() HAL_GPIO_WritePin(OLED_RST_GPIO_Port,OLED_RST_Pin,GPIO_PIN_RESET)
#define OLED_RST_Set() HAL_GPIO_WritePin(OLED_RST_GPIO_Port,OLED_RST_Pin,GPIO_PIN_SET)

/* A0（数据手册里的 DC，命令/数据选择）：高 = 显示数据，低 = 命令 */
#define OLED_A0_Clr() HAL_GPIO_WritePin(A0_GPIO_Port,A0_Pin,GPIO_PIN_RESET)
#define OLED_A0_Set() HAL_GPIO_WritePin(A0_GPIO_Port,A0_Pin,GPIO_PIN_SET)

/* 颜色宏（RGB565 取值习惯）；本屏单色，只区分 0 与非 0 */
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE           	 0x001F  
#define RED           	 0xF800
#define GREEN         	 0x07E0
#define YELLOW        	 0xFFE0

extern  uint16_t BACK_COLOR, POINT_COLOR;

/* 底层写：1 字节显示数据（A0=1）/ 16 位数据（高字节先出，占相邻 2 列）/ 1 字节命令（A0=0） */
void LCD_WR_DATA8(char da);
void LCD_WR_DATA(int da);
void LCD_WR_REG(char da);

/* 用同一字节填满整屏：0x00 清屏、0xFF 全亮（约 1188 次 SPI 字节写） */
void LCD_FullFill( uint8_t FillData );
/* 清空一个逻辑页（page 0~7，0 为屏最上方）；page>7 直接返回 */
void LCD_ClearLine(uint8_t page);
/* 用同一字节填满矩形（逻辑页 × 逻辑列），越界自动收窄。
 * ⚠ 列地址走 LCD_SetPageCol()，跟随 LCD_SCAN_ROT180；负显屏 0xFF=亮块、0x00=黑块 */
void LCD_FillBlock(uint8_t col, uint8_t page, uint8_t width, uint8_t pages, uint8_t data);
/* 初始化：硬件复位 + 上电配置命令 + 清屏 + 开显示，开机调用一次 */
void LCD_init(void);

/* 产测图案：按页写 dat1/dat2 交替铺满屏（页地址不做逻辑页翻转） */
void display(uint8_t dat1,uint8_t dat2);
/* 产测图案：把 ≥1024 字节数据整体铺屏（页地址不做逻辑页翻转） */
void displaychar(uint8_t const *p);

/* 显示 1 个 ASCII 字符（8x16 字模，占 6 列 × 2 页，列地址不补偿）；Order 为 0x20~0x7E，与旧内容叠加 */
void LCD_ShowChar(uint8_t col,uint8_t page,uint8_t Order);
/* 显示纯 ASCII 串（固定 8 列推进，首字符从 col+3 起画）：列不足或 page>6 按 2 页换行、超屏回卷 (0,0)；传汉字会乱码 */
void LCD_ShowStr(uint8_t col,uint8_t page,uint8_t *puts);
/* 中英文混排（标准间距）：汉字 12 列、ASCII 固定 8 列；超宽按 page+=2 换行、超屏回卷 (0,0) */
void LCD_ShowStrEx(uint8_t col, uint8_t page, uint8_t *puts);
/* 中英文混排（紧凑间距）：ASCII 只画有效宽度并推进 width+1 列 */
void LCD_ShowStrExCompact(uint8_t col, uint8_t page, uint8_t *puts);
/* 量出 LCD_ShowStrExCompact() 实际占用的列数（同绘制推进口径，可用于右对齐） */
uint16_t LCD_GetStrExWidth(const char *str);
/* 2 倍放大显示纯 ASCII 串（16 列 × 32 行 = 4 页，最近邻放大，Flash 零增长）。
 * ⚠ 汉字跳过不画；page 只能取 0~4，page > 4 直接返回 */
void LCD_ShowStrExBig(uint8_t col, uint8_t page, const char *str);
/* 量出 LCD_ShowStrExBig() 实际占用的列数，用于右对齐 */
uint16_t LCD_GetStrBigWidth(const char *str);
/* 紧凑显示 1 个 ASCII 字符，返回实际占用列数（有效宽度+1，空格返回 3）；调用方需用它累加 col */
uint8_t LCD_ShowCharCompact(uint8_t col, uint8_t page, uint8_t ch);
/* 显示 128x64 单色位图（连续 1024 字节，整屏覆盖）；puts 为 NULL 直接返回 */
void LCD_ShowBmp( uint8_t const *puts );

/* 设置后续写入的起始页地址 + 列地址（只发命令不写数据）；x 为 0~127 列（内部 +1 补偿），y 为逻辑页 0~7 */
void OLED_Set_Pos(uint8_t x, uint8_t y) ;
/* 6x8 小字模串（单页高，不换行，超 128 列折回左侧）。
 * ⚠ F6x8 只有数字、常用符号和 d/m/n 有数据，字母为空表项（显示空白，非 bug） */
void LCD_ShowStr_Small(uint8_t col, uint8_t page, const char *str);
/* 6x8 小字紧凑排版：只画有效宽度、右侧留 1 列间距 */
void LCD_ShowStr_Small_Compact(uint8_t col, uint8_t page, const char *str);
/* 设置回波曲线 X 轴右端刻度文字（≤7 字符，传 NULL 忽略）；只做字符串拷贝，绘制在 LCD_DrawEchoCurve 末尾 */
void LCD_SetCurveXEndLabel(const char *label);

/* 画一个 ">" 箭头（复用 LCD_ShowStr，纵向占 2 页）。落点：mode 0→col=118（右对齐）、1→col=0、
 * 2→传入 col、3→col+该串紧凑排版宽度；其它取值按 0 处理。str 仅 mode=3 用于测宽，其它可传 NULL */
void LCD_ShowArrowEx(uint8_t page, uint8_t mode, uint8_t col, const char *str);

/* 绘制诊断回波曲线：坐标轴 + 刻度 + 折线 + 刻度文字，并整屏刷入 GDDRAM。
 * echo_data 为 NULL 只画坐标轴；x_scale 为列抽样步长、y_scale 为纵向放大系数（0 会被钳到 1）。
 * ⚠ 会覆盖整屏像素，调用顺序必须排在同屏其它绘制之前 */
void LCD_DrawEchoCurve(uint8_t *echo_data, uint8_t x_scale, uint8_t y_scale);
/* 反白显示 1 个 ASCII 字符（整字节取反、不铺背景）；同位置调两次会恢复原样 */
void LCD_ShowCharReverse(uint8_t col, uint8_t page, uint8_t Order);

#endif  
