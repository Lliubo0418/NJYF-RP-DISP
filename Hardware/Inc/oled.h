//  版 本 号   : v1.0
//  作    者   : Joyee
//  生成日期   : 2019-08-17
//  最近修改   : 
//  功能描述   : OLED演示例程(STM32系列)
//              说明: 
//              ----------------------------------------------------------------
//  ★注意：下面这组引脚是另一块 STM32F103 老板子的接法，与本工程实物不符，
//    仅为保留历史信息；本工程实际接线请看 Core/Src/gpio.c 与 Core/Src/spi.c：
//              ------------------------------------------------ 本工程实际接法
//							CS(片选)  接 SPI_CS_Pin    (GPIOA)
//              RST(复位) 接 OLED_RST_Pin  (GPIOB)
//              A0(DC)    接 A0_Pin        (GPIOA)
//              SCL(时钟) 接 PA5 / SPI1_SCK
//              SDA(数据) 接 PA7 / SPI1_MOSI（单线半双工，无 MISO）
//              VCC       3.3v电源
//              GND       电源地
//              ------------------------------------------------ 以下为旧板接法(作废)
//							CS     接PD6
//              RST    接PD8
//              A0     接PD10
//              SCL    接PD12
//              SDA    接PD14
//              VCC    3.3v电源
//              GND    电源地
//              ----------------------------------------------------------------
//  驱动芯片   : ST7565 / AiP31567 系 COG 驱动（不是 SSD1306）
//              屏为单色 128x64，页地址模式，1 页 = 8 像素行，列地址自动 +1
//  显示板说明 : 本板无 EEPROM。界面参数由主控板经 PARAM_DUMP 帧下发，
//              本地 gRadarParam 里的诊断曲线选择等设置掉电即回初值。
//******************************************************************************/
#ifndef __OLED_H
#define __OLED_H

//#include <stm32f10x.h>
#include "main.h"

/* 屏幕分辨率：宽 128 像素列、高 64 像素行（= 8 页 × 8 行）。
 * 这两个宏是 oled.c 中所有换行/回卷判断（LCD_W、LCD_H/8）的依据。 */
#define LCD_W 128
#define LCD_H 64

/* 芯片总列数：AiP31567 有 SEG0~SEG131 共 132 列，屏体只用其中 128 列，
 * 另有 4 列不可见。旋转 180° 时的列地址补偿要用到它，见 LCD_SetPageCol()。 */
#define LCD_SEG_TOTAL 132

/* ★显示方向总开关（2026-09-21 加入）：
 *   1 = 相对出厂方向旋转 180°（屏体倒装时置 1，画面仍正立可读）
 *   0 = 不旋转（屏体正装）
 * 由手册里那两条扫描方向命令实现：MY（0xC0/0xC8）管上下、MX（0xA0/0xA1）
 * 管左右，两条一起取反才是干净的 180°。
 * ⚠ 改动是【三处配套】，缺一处画面就会错位或甩出可见区：
 *     ① oled.c 的 LCD_init()          —— 0xC0+0xA0 ↔ 0xC8+0xA1
 *     ② oled.c 的 LCD_SetPageCol()    —— 列偏移 o ↔ 4-o（4 = 132-128）
 *     ③ oled.c 的 LCD_ShowBmp() / LCD_DrawEchoCurve() 里写死的列起点
 *   （①②③ 全部由本宏自动切换，无需手工逐个改。）
 * 只影响"画面朝哪边"，不改逻辑坐标语义：col 0 永远是可见区的最左列、
 * page 0 永远在最上方，UI 层不需要跟着改。 */
#define LCD_SCAN_ROT180 1


			  
//-----------------OLED端口定义----------------  					   
/* 片选 CS：低电平选中 */
#define OLED_CS_Clr()  HAL_GPIO_WritePin(SPI_CS_GPIO_Port,SPI_CS_Pin,GPIO_PIN_RESET)//CS
#define OLED_CS_Set()  HAL_GPIO_WritePin(SPI_CS_GPIO_Port,SPI_CS_Pin,GPIO_PIN_SET)

/* 硬件复位 RST：低电平复位。 */
#define OLED_RST_Clr() HAL_GPIO_WritePin(OLED_RST_GPIO_Port,OLED_RST_Pin,GPIO_PIN_RESET)//RES
#define OLED_RST_Set() HAL_GPIO_WritePin(OLED_RST_GPIO_Port,OLED_RST_Pin,GPIO_PIN_SET)

/* A0（即数据手册里的 DC，命令/数据选择，在 GPIOA）：
 * Set() = 高电平 = 后续字节是"显示数据"；Clr() = 低电平 = 后续字节是"命令"。 */
#define OLED_A0_Clr() HAL_GPIO_WritePin(A0_GPIO_Port,A0_Pin,GPIO_PIN_RESET)//DC                         A0
#define OLED_A0_Set() HAL_GPIO_WritePin(A0_GPIO_Port,A0_Pin,GPIO_PIN_SET)
 		     
/* 颜色宏（RGB565 风格的取值习惯）。本屏是单色屏，实际只区分 0 与非 0。 */
#define WHITE         	 0xFFFF
#define BLACK         	 0x0000	  
#define BLUE           	 0x001F  
#define RED           	 0xF800
#define GREEN         	 0x07E0
#define YELLOW        	 0xFFE0
					  		 
extern  uint16_t BACK_COLOR, POINT_COLOR;   //背景色，画笔色

/* 写 1 字节显示数据（A0=1）。da 为 8bit 图形数据，1 = 点亮像素。 */
void LCD_WR_DATA8(char da); //发送数据-8位参数
/* 写 16 位数据（高字节先出，占用 GDDRAM 相邻 2 列），当前 UI 未使用。 */
void LCD_WR_DATA(int da);
/* 写 1 字节命令（A0=0）；命令字含义见 oled.c 中 LCD_WR_REG 的注释表。 */
void LCD_WR_REG(char da);
 
/* 用同一个字节填满整屏：0x00 清屏、0xFF 全亮。约 1188 次 SPI 字节写。 */
void LCD_FullFill( uint8_t FillData );
/* 清空一个逻辑页（page 0~7，0 为屏幕最上方）；page>7 直接返回。约 132 次字节写。 */
void LCD_ClearLine(uint8_t page);
/* 用同一个字节填满一块矩形区域（逻辑页 × 逻辑列）。
 * col/page 为左上角（page 0 = 屏幕最上方），width/pages 为列数与页数。
 * 负显屏（黑底白字）上 0xFF = 亮块、0x00 = 黑块；正显屏反之。
 * 越界按可见区自动收窄；列地址走 LCD_SetPageCol()，跟随 LCD_SCAN_ROT180。 */
void LCD_FillBlock(uint8_t col, uint8_t page, uint8_t width, uint8_t pages, uint8_t data);
/* 初始化：硬件复位 + 上电配置命令 + 清屏 + 开显示，开机调用一次。 */
void LCD_init(void);

/* 产测图案：按页写 dat1/dat2 交替字节铺满屏幕（页地址未做逻辑页翻转）。 */
void display(uint8_t dat1,uint8_t dat2);
/* 产测图案：把 ≥1024 字节的数据整体铺屏（页地址未做逻辑页翻转）。 */
void displaychar(uint8_t const *p);

/* 显示 1 个 ASCII 字符（8x16 字模，占 6 列 × 2 页，列地址不做补偿）。
 * Order 为 ASCII 码 0x20~0x7E；不透写背景，会与旧内容叠加。 */
void LCD_ShowChar(uint8_t col,uint8_t page,uint8_t Order);
/* 显示纯 ASCII 字符串（固定 8 列推进，首字符从 col+3 起画）。
 * 列不足或 page>6 时按 2 页换行，超出屏末回卷到 (0,0)；传汉字会乱码。 */
void LCD_ShowStr(uint8_t col,uint8_t page,uint8_t *puts);
/* 显示 1 个汉字（12x12 字模，占 12 列 × 2 页）。字库无此字时静默不画。
 * hz 须指向完整的 UTF-8 / GB2312 序列（编码识别见 oledfont.h）。 */
void LCD_ShowChinese(uint8_t col, uint8_t page, char *hz);
/* 中英文混排（标准间距）：汉字 12 列、ASCII 固定 8 列；
 * 超宽/超屏时分别按 page+=2 换行、回卷到 (0,0)。 */
void LCD_ShowStrEx(uint8_t col, uint8_t page, uint8_t *puts);
/* 中英文混排（紧凑间距）：ASCII 只画有效宽度并推进 width+1 列，更省横向空间。 */
void LCD_ShowStrExCompact(uint8_t col, uint8_t page, uint8_t *puts);
/* 量出 LCD_ShowStrExCompact() 实际占用的列数（与绘制推进量同口径，可用于右对齐）。
 * 与旧的 LCD_GetStrCompactWidth() 不同：窄字符按真实墨迹宽度算，不按固定 9 列。 */
uint16_t LCD_GetStrExWidth(const char *str);
/* 2 倍放大显示纯 ASCII 串（16 列 × 32 行，纵向占 page ~ page+3 共 4 页 = 32 行）。
 * 放大方式为最近邻 2 倍（每列/每行重复一遍），不新增字模、Flash 零增长。
 * 汉字不参与放大（跳过不画）；page 只能取 0~4，page > 4 直接返回。 */
void LCD_ShowStrExBig(uint8_t col, uint8_t page, const char *str);
/* 量出 LCD_ShowStrExBig() 实际占用的列数（= 2 × 每字符推进量），用于右对齐。 */
uint16_t LCD_GetStrBigWidth(const char *str);
/* 紧凑显示 1 个 ASCII 字符，返回本字符实际占用的列数（有效宽度+1；空格返回 3）。
 * 调用方需用返回值累加 col 来定位下一个字符。 */
uint8_t LCD_ShowCharCompact(uint8_t col, uint8_t page, uint8_t ch);
/* 显示 0~255 的十进制数（不画前导 0；位数变少时旧的高位会残留，需先清屏）。 */
void LCD_ShowNum(uint8_t col,uint8_t page,uint8_t Num);
/* 显示 128x64 单色位图（连续 1024 字节，按 8 页 × 128 列整屏覆盖）。
 * puts 为 NULL 时直接返回，不访问总线。 */
void LCD_ShowBmp( uint8_t const *puts );

// 设置OLED显存地址 
/* 设置后续写入的起始 页地址 + 列地址；只发命令、不写数据。
 * x 为 0~127 的用户列坐标（内部按 +1 列补偿），y 为逻辑页 0~7。 */
void OLED_Set_Pos(uint8_t x, uint8_t y) ;
/* 6x8 小字模字符串（单页高）。注意 F6x8 只有数字、常用符号和 d/m/n 有数据，
 * 字母/大写多为空表项、会显示为空白；不做换行，超出 128 列会折回左侧。 */
void LCD_ShowStr_Small(uint8_t col, uint8_t page, const char *str);
/* 6x8 小字紧凑排版：只画有效宽度、右侧留 1 列间距。 */
void LCD_ShowStr_Small_Compact(uint8_t col, uint8_t page, const char *str);



// mode: 0=右对齐(默认), 1=左对齐(文本前), 2=指定位置, 3=自动计算字符串宽度后显示
// col: 当mode=2时为箭头所在列; 当mode=3时为文本起始列
/* 画一个 ">" 箭头（复用 LCD_ShowStr，纵向占 2 页）。
 * 箭头落点：mode 0 → col=118（右对齐）；1 → col=0；2 → 传入的 col；
 *           3 → col + 该字符串的紧凑排版宽度；其它取值按 0 处理。
 * str 仅在 mode=3 时用于测宽，其它模式可传 NULL。 */
void LCD_ShowArrowEx(uint8_t page, uint8_t mode, uint8_t col, const char *str);

/* 绘制诊断回波曲线：坐标轴 + 刻度 + 折线 + 刻度文字，并整屏刷入 GDDRAM。
 * echo_data 为回波数据（传 NULL 则只画坐标轴）；x_scale 为列抽样步长、
 * y_scale 为纵向放大系数，二者建议 1~8，传 0 会被内部钳到 1。
 * 副作用最重：会覆盖整屏像素，调用顺序上必须排在同屏其它绘制之前。 */
void LCD_DrawEchoCurve(uint8_t *echo_data, uint8_t x_scale, uint8_t y_scale);
/* 反白显示 1 个 ASCII 字符（字模按位取反）。列地址按 +1 补偿后下发。
 * 只做整字节取反、不铺背景，同一位置调用两次会恢复原样。 */
void LCD_ShowCharReverse(uint8_t col, uint8_t page, uint8_t Order);



#endif  
	 
	 



