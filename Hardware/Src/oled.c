
#include "oled.h"
#include "stdlib.h"
#include "oledfont.h"
#include "spi.h"
#include "string.h"

uint8_t frame_buffer[8][128];

uint16_t BACK_COLOR, POINT_COLOR;

uint8_t const ComTable[] = {
    7,
    6,
    5,
    4,
    3,
    2,
    1,
    0,
};

/**
 * @brief  向 OLED 串行写入 1 个字节
 * @param  dat 待发送字节
 * @retval 无
 */
void LCD_Writ_Bus(char dat)
{
    uint8_t i;
    OLED_CS_Clr();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&dat, 1, 1000);
    OLED_CS_Set();
}

/**
 * @brief  写 1 个字节显示数据
 * @param  da 待写入的 8bit 数据
 * @retval 无
 */
void LCD_WR_DATA8(char da)
{
    OLED_A0_Set();
    LCD_Writ_Bus(da);
}

/**
 * @brief  写 16 位数据
 * @param  da 16bit 数据
 * @retval 无
 */
void LCD_WR_DATA(int da)
{
    OLED_A0_Set();
    LCD_Writ_Bus(da >> 8);
    LCD_Writ_Bus(da);
}

/**
 * @brief  写 1 个字节命令
 * @param  da 命令字节
 * @retval 无
 */
void LCD_WR_REG(char da)
{
    OLED_A0_Clr();
    LCD_Writ_Bus(da);
}

/**
 * @brief  写 1 个字节命令，后跟 16 位参数
 * @param  reg 命令字节
 * @param  da  命令参数，按 16 位发送
 * @retval 无
 */
void LCD_WR_REG_DATA(char reg, int da)
{
    LCD_WR_REG(reg);
    LCD_WR_DATA(da);
}

/**
 * @brief  设置后续写入的起始
 * @param  page 逻辑页号 0~7
 * @param  col  程序侧列号 0~127
 * @param  o    调用方名义列偏移
 * @retval 无
 */
static void LCD_SetPageCol(uint8_t page, uint8_t col, uint8_t o)
{
#if LCD_SCAN_ROT180
    uint8_t c = (uint8_t)(col + ((132u - 128u) - o));
#else
    uint8_t c = (uint8_t)(col + o);
#endif
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0);           /* 页地址 */
    LCD_WR_REG((uint8_t)(((c >> 4) & 0x0F) | 0x10));    /* 列地址高 4 位 */
    LCD_WR_REG((uint8_t)(c & 0x0F));
}

/**
 * @brief  用同一个字节填满整个 GDDRAM
 * @param  FillData 填充字节：0x00 全屏熄灭，0xFF 全屏点亮
 * @retval 无
 */
void LCD_FullFill(uint8_t FillData)
{
    uint8_t i, j;
    for (i = 0; i < 9; i++)
    {
        LCD_WR_REG(i | 0xB0);
        LCD_WR_REG(0x10);
        LCD_WR_REG(0x00);
        for (j = 0; j < 132; j++)
        {
            LCD_WR_DATA8(FillData);
        }
    }
}

/**
 * @brief  清空指定的一个逻辑页
 * @param  page 逻辑页号 0~7
 * @retval 无
 */
void LCD_ClearLine(uint8_t page)
{
    if(page > 7)
        return;
    page = 7 - page;
    uint8_t i;
    LCD_WR_REG(page | 0xB0);
    LCD_WR_REG(0x10);
    LCD_WR_REG(0x00);
    for (i = 0; i < 132; i++)
    {
        LCD_WR_DATA8(0x00);
    }
}

/**
 * @brief  用同一个字节填满一个矩形区域
 * @param  col   起始逻辑列 0~127
 * @param  page  起始逻辑页 0~7
 * @param  width 宽度
 * @param  pages 高度
 * @param  data  填充字节
 * @retval 无
 */
void LCD_FillBlock(uint8_t col, uint8_t page, uint8_t width, uint8_t pages, uint8_t data)
{
    uint8_t p, i;
    if (width == 0u || pages == 0u || col > 127u || page > 7u)
        return;
    if ((uint16_t)col + (uint16_t)width > LCD_W)
        width = (uint8_t)(LCD_W - col);
    if ((uint16_t)page + (uint16_t)pages > 8u)
        pages = (uint8_t)(8u - page);
    for (p = 0; p < pages; p++)
    {
        LCD_SetPageCol((uint8_t)(page + p), col, 0u);
        for (i = 0; i < width; i++)
        {
            LCD_WR_DATA8(data);
        }
    }
}

/**
 * @brief  OLED 初始化
 * @retval 无
 */
void LCD_init(void)
{
    OLED_CS_Clr();
    OLED_RST_Clr();
    HAL_Delay(20); /* 复位保持 */
    OLED_RST_Set();
    HAL_Delay(20);
    OLED_CS_Set();

    LCD_WR_REG(0xE2); // initialize interal function
    LCD_WR_REG(0x2F); // power control(VB,VR,VF=1,1,1)
    LCD_WR_REG(0x23); // Regulator resistor select(RR2,RR1,VRR0=0,1,1)
    LCD_WR_REG(0xA2); // set LCD bias=1/9(BS=0)
    LCD_WR_REG(0x81); // set reference voltage
    LCD_WR_REG(0x25);
#if LCD_SCAN_ROT180
    LCD_WR_REG(0xC8);   // MY=1：反向扫描（COM63~COM0）
    LCD_WR_REG(0xA1);
#else
    LCD_WR_REG(0xC0);   // MY=0：普通扫描（COM0~COM63）
    LCD_WR_REG(0xA0);
#endif
    LCD_WR_REG(0x40);   // 显示起始行 S[5:0]=0
    LCD_WR_REG(0xA6);   // INV=0：普通显示（非反显）
    LCD_WR_REG(0xA4);   // AP=0：普通显示（取消"屏全亮"）
    LCD_FullFill(0x00);
    LCD_WR_REG(0xAF);
}

/**
 * @brief  显示
 * @param  dat1 每列第一个字节
 * @param  dat2 每列第二个字节
 * @retval 无
 */
void display(uint8_t dat1, uint8_t dat2)
{
    uint8_t row, col;

    for (row = 0xb0; row < 0xb8; row++)
    {
        LCD_WR_REG(row);  // set page address
        LCD_WR_REG(0x10); // set column address
        LCD_WR_REG(0x00);
        for (col = 0; col < 128; col++)
        {
            LCD_WR_DATA8(dat1);
            LCD_WR_DATA8(dat2);
        }
    }
}


void displaychar(uint8_t const *p)
{
    uint8_t row, col;

    for (row = 0xb0; row < 0xb8; row++)
    {
        LCD_WR_REG(row);  // set page address
        LCD_WR_REG(0x10); // set column address
        LCD_WR_REG(0x00);
        for (col = 0; col < 128; col++)
            LCD_WR_DATA8(*p++);
    }
}

/**
 * @brief  在指定位置显示 1 个 ASCII 字符
 * @param  col   起始列 0~127
 * @param  page  上半部逻辑页 0~7
 * @param  Order ASCII 字符码 0x20~0x7E
 * @retval 无
 */
void LCD_ShowChar(uint8_t col, uint8_t page, uint8_t Order)
{
    uint8_t i;
    uint8_t ch = Order - 0x20;
    LCD_SetPageCol(page & 0x07, col, 0);
    for (i = 0; i < 6; i++)
    {
        LCD_WR_DATA8(ASCIIchardot[ch][i]);
    }
    page++;
    LCD_SetPageCol(page & 0x07, col, 0);
    for (i = 6; i < 12; i++)
    {
        LCD_WR_DATA8(ASCIIchardot[ch][i]);
    }
    page--;
}


static uint8_t LCD_GetCharWidth(uint8_t ch, uint8_t *left)
{
    uint8_t idx = ch - 0x20;
    uint8_t first_col = 6, last_col = 0;

    for (uint8_t col = 0; col < 6; col++) {
        if (ASCIIchardot[idx][col] != 0) {
            if (col < first_col) first_col = col;
            if (col > last_col) last_col = col;
        }
    }

    for (uint8_t col = 6; col < 12; col++) {
        if (ASCIIchardot[idx][col] != 0) {
            if ((col - 6) < first_col) first_col = col - 6;
            if ((col - 6) > last_col) last_col = col - 6;
        }
    }

    if (last_col < first_col) {
        *left = 0;
        return 0;
    }

    *left = first_col;
    return last_col - first_col + 1;
}


uint8_t LCD_ShowCharCompact(uint8_t col, uint8_t page, uint8_t ch)
{
    uint8_t left, width;
    width = LCD_GetCharWidth(ch, &left);

    if (width == 0) {
        return 3;
    }

    uint8_t idx = ch - 0x20;

    LCD_SetPageCol(page & 0x07, col, 0);
    for (uint8_t i = left; i < left + width; i++) {
        LCD_WR_DATA8(ASCIIchardot[idx][i]);
    }

    page++;
    LCD_SetPageCol(page & 0x07, col, 0);
    for (uint8_t i = left + 6; i < left + width + 6; i++) {
        LCD_WR_DATA8(ASCIIchardot[idx][i]);
    }
    page--;

    return width + 1;
}

/**
 * @brief  紧凑显示中英文混排字符串
 * @param  col  起始列 0~127
 * @param  page 起始逻辑页 0~7
 * @param  puts 字符串首地址
 * @retval 无
 */
void LCD_ShowStrExCompact(uint8_t col, uint8_t page, uint8_t *puts)
{
    while (*puts != '\0')
    {
        if (col + 12 > LCD_W)
        {
            page = page + 2;
            col = 0;
        }
        if (page >= LCD_H / 8 - 1)
        {
            page = 0;
            col = 0;
        }

        if (*puts >= 0x80)
        {
            uint8_t index = GetHzIndex((char *)puts);
            if (index != 0xFF)
            {
                LCD_SetPageCol(page & 0x07, col, 0);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);

                LCD_SetPageCol((page + 1) & 0x07, col, 0);
                for (uint8_t i = 12; i < 24; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
            }
            if ((*puts & 0xF0) == 0xE0)
                puts += 3;
            else
                puts += 2;
            col += 12;
        }
        else
        {
            col += LCD_ShowCharCompact(col, page, *puts);
            puts++;
        }
    }
}

/**
 * @brief  计算单个 ASCII 字符在紧凑排版下的占用列数
 * @param  ch ASCII 字符码 0x20~0x7E
 * @retval 占用列数 有效宽度 + 1；空白字符返回 3
 */
static uint8_t LCD_AdvanceOf(uint8_t ch)
{
    uint8_t left;
    uint8_t w = LCD_GetCharWidth(ch, &left);
    return (w == 0u) ? 3u : (uint8_t)(w + 1u);
}


uint16_t LCD_GetStrExWidth(const char *str)
{
    uint16_t width = 0u;
    if (str == NULL) return 0u;

    while (*str != '\0')
    {
        if ((uint8_t)*str >= 0x80u)
        {
            width += 12u;
            str += ((*str & 0xF0) == 0xE0) ? 3 : 2;
        }
        else
        {
            width += LCD_AdvanceOf((uint8_t)*str);
            str++;
        }
    }
    return width;
}

/**
 * @brief  计算字符串占用列数
 * @param  str 
 * @retval 占用列数
 */
uint16_t LCD_GetStrBigWidth(const char *str)
{
    uint16_t width = 0u;
    if (str == NULL) return 0u;

    while (*str != '\0')
    {
        if ((uint8_t)*str >= 0x80u)
            str += ((*str & 0xF0) == 0xE0) ? 3 : 2;
        else
        {
            width += (uint16_t)(2u * LCD_AdvanceOf((uint8_t)*str));
            str++;
        }
    }
    return width;
}

/**
 * @brief  2 倍放大显示纯 ASCII 字符串
 * @param  col  起始列 0~127
 * @param  page 起始逻辑页 0~4
 * @param  str  
 * @retval 无
 */
void LCD_ShowStrExBig(uint8_t col, uint8_t page, const char *str)
{
    if (str == NULL || page > 4u) return;

    while (*str != '\0')
    {
        uint8_t ch = (uint8_t)*str;

        if (ch >= 0x80u)
        {
            str += ((ch & 0xF0) == 0xE0) ? 3 : 2;
            continue;
        }

        if (ch >= 0x20u)
        {
            uint8_t left;
            uint8_t w = LCD_GetCharWidth(ch, &left);

            if (w > 0u)
            {
                uint8_t idx = (uint8_t)(ch - 0x20u);

                for (uint8_t k = 0u; k < 4u; k++)
                {
                    LCD_SetPageCol((page + k) & 0x07, col, 0);

                    for (uint8_t j = 0u; j < (uint8_t)(2u * w); j++)
                    {
                        uint8_t sc  = (uint8_t)(left + (j >> 1));
                        uint8_t up  = ASCIIchardot[idx][sc];
                        uint8_t lo  = ASCIIchardot[idx][sc + 6];
                        uint8_t out = 0u;

                        for (uint8_t r = 0u; r < 8u; r++)
                        {
                            uint8_t row = (uint8_t)(k * 8u + r);
                            uint8_t bit;
                            if (row < 16u)
                                bit = (uint8_t)((up >> (7u - (row >> 1))) & 1u);
                            else
                                bit = (uint8_t)((lo >> (7u - ((row - 16u) >> 1))) & 1u);
                            if (bit)
                                out |= (uint8_t)(0x80u >> r);
                        }
                        LCD_WR_DATA8((char)out);
                    }
                }
                col = (uint8_t)(col + 2u * w + 2u);
            }
            else
            {
                col = (uint8_t)(col + 6u);
            }
        }
        str++;
    }
}

/**
 * @brief  显示中英文混排字符串
 * @param  col  起始列 0~127
 * @param  page 起始逻辑页 0~7
 * @param  puts 字符串首地址
 * @retval 无
 */
void LCD_ShowStrEx(uint8_t col, uint8_t page, uint8_t *puts)
{
    while (*puts != '\0')
    {
        if (col + 12 > LCD_W)
        {
            page = page + 2;
            col = 0;
        }
        if (page >= LCD_H / 8 - 1)
        {
            page = 0;
            col = 0;
        }

        if (*puts >= 0x80)
        {
            uint8_t index = GetHzIndex((char *)puts);
            if (index != 0xFF)
            {
                // 12x12 汉字显示
                // 第1页
                LCD_SetPageCol(page & 0x07, col, 0);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
                // 第2页
                LCD_SetPageCol((page + 1) & 0x07, col, 0);
                for (uint8_t i = 12; i < 24; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
            }
            if ((*puts & 0xF0) == 0xE0)
                puts += 3;
            else
                puts += 2;
            col += 12;
        }
        else
        {
            LCD_ShowChar(col, page, *puts);
            puts++;
            col += 8;
        }
    }
}

/**
 * @brief  显示纯 ASCII 字符串
 * @param  col  起始列 0~127；实际首字符从 col+3 开始画
 * @param  page 起始逻辑页 0~7
 * @param  puts 字符串首地址
 * @retval 无
 */
void LCD_ShowStr(uint8_t col, uint8_t page, uint8_t *puts)
{
    while (*puts != '\0')
    {
        if (col > (LCD_W - 8))
        {
            page = page + 2;
            col = 0;
        }
        if (page > (LCD_H / 8 - 2))
        {
            page = 0;
            col = 0;
        }
        LCD_ShowChar(col + 3, page, *puts);
        puts++;
        col = col + 8;
    }
}

/**
 * @brief  显示一张 128x64 的单色位图
 * @param  puts 
 * @retval 无
 */
void LCD_ShowBmp(uint8_t const *puts)
{
    if (puts == 0) return;
    uint8_t i, j;
    uint16_t X = 0;
    for (i = 0; i < (LCD_H / 8); i++)
    {
        LCD_WR_REG(0xB0 | ComTable[i]); // Set Page Address
        LCD_WR_REG(0x10);
#if LCD_SCAN_ROT180
        LCD_WR_REG(0x00);
#else
        LCD_WR_REG(0x04);
#endif
        for (j = 0; j < LCD_W; j++)
        {
            LCD_WR_DATA8(puts[X]);
            X++;
        }
    }
}

/**
 * @brief  设置 OLED 显存的写入地址
 * @param  col  
 * @param  page 逻辑页 0~7
 * @retval 无
 */
void OLED_Set_Pos(uint8_t col, uint8_t page)
{
    uint8_t real_col = col + 3;
    LCD_SetPageCol(page & 0x07, col, 1);
}

/**
 * @brief  用小号字模显示字符串
 * @param  col  起始列 0~127
 * @param  page 逻辑页 0~7
 * @param  str  字符串首地址
 * @retval 无
 */
void LCD_ShowStr_Small(uint8_t col, uint8_t page, const char *str)
{
    uint8_t i = 0, j = 0;
    while (*str != '\0')
    {
        OLED_Set_Pos(col, page);

        uint8_t c = *str - ' ';
        for (i = 0; i < 6; i++)
        {
            uint8_t temp = F6x8[c][i]; // 获取原始字模数据

            LCD_WR_DATA8(temp);
        }

        col += 6;
        str++;
    }
}

/**
 * @brief  测量 6x8 小字模的实际墨迹宽度
 * @param  ch   字符码
 * @param  left 输出
 * @retval 字模有效宽度
 */
static uint8_t LCD_GetSmallCharWidth(uint8_t ch, uint8_t *left)
{
    uint8_t idx = ch - ' ';
    uint8_t first_col = 6, last_col = 0;

    for (uint8_t col = 0; col < 6; col++) {
        if (F6x8[idx][col] != 0) {
            if (col < first_col) first_col = col;
            if (col > last_col) last_col = col;
        }
    }

    if (last_col < first_col) {
        *left = 0;
        return 0;
    }

    *left = first_col;
    return last_col - first_col + 1;
}

/**
 * @brief  紧凑显示 1 个 6x8 字符
 * @param  col  起始列
 * @param  page 逻辑页 0~7
 * @param  ch   字符码 
 * @retval 占用列数
 */
static uint8_t LCD_ShowSmallCharCompact(uint8_t col, uint8_t page, uint8_t ch)
{
    uint8_t left, width;
    width = LCD_GetSmallCharWidth(ch, &left);

    if (width == 0) {
        return 3;
    }

    uint8_t idx = ch - ' '; // ASCII字符从' '开始

    OLED_Set_Pos(col, page);
    for (uint8_t i = left; i < left + width; i++) {
        LCD_WR_DATA8(F6x8[idx][i]);
    }

    return width + 1;
}

/**
 * @brief  紧凑显示 6x8 小字字符串
 * @param  col  起始列
 * @param  page 逻辑页 0~7
 * @param  str  字符串首地址
 * @retval 无
 */
void LCD_ShowStr_Small_Compact(uint8_t col, uint8_t page, const char *str)
{
    while (*str != '\0')
    {
        col += LCD_ShowSmallCharCompact(col, page, *str);
        str++;
    }
}

/**
 * @brief  计算字符串按紧凑排版绘制后的总像素宽度
 * @param  str 待测量字符串
 * @retval 总宽度
 */
static uint8_t LCD_GetStrCompactWidth(const char *str)
{
    uint8_t width = 0;
    if (str == NULL) return 0;

    while (*str != '\0') {
        if (*str >= 0x80) {
            width += 12;
            if ((*str & 0xF0) == 0xE0)
                str += 3;
            else
                str += 2;
        } else {
            if (*str == ' ') {
                width += 3;
            } else if (*str >= '0' && *str <= '9') {
                width += 9;
            } else {
                uint8_t idx = *str - 0x20;
                if (idx < 95) {
                    uint8_t first_col = 6, last_col = 0;
                    for (uint8_t col = 0; col < 6; col++) {
                        if (ASCIIchardot[idx][col] != 0) {
                            if (col < first_col) first_col = col;
                            if (col > last_col) last_col = col;
                        }
                    }
                    for (uint8_t col = 6; col < 12; col++) {
                        if (ASCIIchardot[idx][col] != 0) {
                            if ((col - 6) < first_col) first_col = col - 6;
                            if ((col - 6) > last_col) last_col = col - 6;
                        }
                    }
                    if (last_col >= first_col) {
                        width += (last_col - first_col + 1) + 1;
                    } else {
                        width += 3;
                    }
                } else {
                    width += 3;
                }
            }
            str++;
        }
    }
    return width;
}

/**
 * @brief  画一个 ">" 箭头
 * @param  page 逻辑页 
 * @param  mode 定位方式
 * @param  col  mode=3 时为文本起始列、mode=2 时为箭头列；mode=0/1 忽略
 * @param  str  待测量字符串
 * @retval 无
 */
void LCD_ShowArrowEx(uint8_t page, uint8_t mode, uint8_t col, const char *str)
{
    uint8_t arrowCol;

    switch (mode) {
        case 1:
            arrowCol = 0;
            break;
        case 2:
            arrowCol = col;
            break;
        case 3:
            {
                uint8_t textWidth = LCD_GetStrCompactWidth(str);
                arrowCol = col + textWidth;
            }
            break;
        case 0:
        default:
            arrowCol = LCD_W - 10;
            break;
    }

    LCD_ShowStr(arrowCol, page, (uint8_t *)">");
}

#define DRAW_PIXEL(x, y) do { \
    if ((x) >= 0 && (x) < 128 && (y) >= 0 && (y) < 64) { \
        frame_buffer[(y) / 8][(x)] |= (1 << (7 - ((y) % 8))); \
    } \
} while(0)

static char s_curve_x_end[8] = "8.91";

/**
 * @brief  设置回波曲线 X 轴右端刻度文字
 * @param  label 刻度文字
 * @retval 无
 */
void LCD_SetCurveXEndLabel(const char *label)
{
    if (label == 0) return;
    strncpy(s_curve_x_end, label, sizeof(s_curve_x_end) - 1u);
    s_curve_x_end[sizeof(s_curve_x_end) - 1u] = '\0';
}

/**
 * @brief  绘制诊断回波曲线
 * @param  echo_data 回波数据
 * @param  x_scale   X 方向抽样步长
 * @param  y_scale   Y 方向放大倍数
 * @retval 无
 */
void LCD_DrawEchoCurve(uint8_t *echo_data, uint8_t x_scale, uint8_t y_scale)
{
    uint8_t i, j;

    if (x_scale < 1u) x_scale = 1u;
    if (y_scale < 1u) y_scale = 1u;

    const int AXIS_X = 8;
    const int AXIS_Y = 55;
    const int CURVE_TOP_Y  = 15;
    const int CURVE_PLOT_H = AXIS_Y - CURVE_TOP_Y;
    const int CURVE_X1     = 127;   // 绘图区右端列 = 屏幕右缘（128 点铺满全宽）


    // 初始化显存数组 

    memset(frame_buffer, 0, sizeof(frame_buffer));

    for (int y = 15; y <= LCD_H-1; y++) {
        DRAW_PIXEL(AXIS_X, y);
    }

    for (int x = 0; x <= CURVE_X1; x++) {
        DRAW_PIXEL(x, AXIS_Y);
    }

    for (int h = 16; h < 21; h++) {
        int y =   h;       
        int w = ((h-16) < 2) ? 0 : ((h-16) / 2);
        for (int x = AXIS_X - w; x <= AXIS_X + w; x++) {
            DRAW_PIXEL(x, y);
        }
    }

    for (int h = 0; h < 5; h++) {
        int x = CURVE_X1 - h;     
        int w = (h < 2) ? 0 : (h / 2);  
        for (int y = AXIS_Y - w; y <= AXIS_Y + w; y++) {
            DRAW_PIXEL(x, y);
        }
    }

    // 刻度按绘图区跨度精确 5 等分：10~127 共 117 列 ⇒ 33/56/80/103（先乘后除，勿写 (i+1)*(117/5)）
    for (i = 0; i <= 3; i++) {
        int x = AXIS_X + 2 + ((i + 1) * (CURVE_X1 - AXIS_X - 2)) / 5;

        for (int y = AXIS_Y- 2; y <= AXIS_Y ; y++) {
            DRAW_PIXEL(x, y);
        }
    }

    if (echo_data != NULL) {
        int prev_y = -1;

        // 绘图区 10~127（118 列）均匀映射到下标 0~127 ⇒ 128 点铺满全量程。
        // （原先 x<122 且 1 列 1 点，只画到下标 111 = 112/128 点，右端空 6 列，与右端量程刻度矛盾）
        const int PLOT_X0   = AXIS_X + 2;          // 10
        const int PLOT_SPAN = CURVE_X1 - PLOT_X0;  // 117（列数 118）

        for (int x = PLOT_X0; x <= CURVE_X1; x++) {

            int data_idx = ((x - PLOT_X0) * 127 * (int)x_scale) / PLOT_SPAN; 
            if (data_idx >= 128) break;
            if (data_idx < 0) continue;

            int y = AXIS_Y - ((int)echo_data[data_idx] * (int)y_scale * CURVE_PLOT_H / 255);

            if (y < CURVE_TOP_Y) y = CURVE_TOP_Y;
            if (y > AXIS_Y) y = AXIS_Y;     // 最低不能掉下 Y=58 (X轴)

            DRAW_PIXEL(x, y);

            if (prev_y != -1) {
                int start = (y < prev_y) ? y + 1 : prev_y + 1;
                int end   = (y > prev_y) ? y - 1 : prev_y - 1;
                for (int fill_y = start; fill_y <= end; fill_y++) {
                    DRAW_PIXEL(x, fill_y);
                }
            }
            prev_y = y;
        }
    }

    for (i = 0; i < 8; i++) {
        LCD_WR_REG(ComTable[i] | 0xB0); // Page 地址
        LCD_WR_REG(0x10);
#if LCD_SCAN_ROT180
        LCD_WR_REG(0x04);
#else
        LCD_WR_REG(0x00);
#endif
        for (j = 0; j < 128; j++) {
            LCD_WR_DATA8(frame_buffer[i][j]);
        }
    }

    LCD_ShowStr_Small_Compact(11, 7, "0.01");
    LCD_ShowStr_Small_Compact(55, 7, "m(d)");
    {
        size_t n = strlen(s_curve_x_end);
        uint8_t col = 98u;
        if (n > 4u) col = (uint8_t)(98u - (n - 4u) * 5u);
        LCD_ShowStr_Small_Compact(col, 7, s_curve_x_end);
    }
}

/**
 * @brief  反白显示 1 个 ASCII 字符
 * @param  col   起始列
 * @param  page  上半部逻辑页
 * @param  Order ASCII 码
 * @retval 无
 */
void LCD_ShowCharReverse(uint8_t col, uint8_t page, uint8_t Order)
{
    uint8_t i;
    uint8_t ch = Order - 0x20;                // ASCII字符从0x20开始
    LCD_SetPageCol(page & 0x07, col, 1);
    for (i = 0; i < 6; i++)
    {
        LCD_WR_DATA8(~(ASCIIchardot[ch][i]));
    }
    page++;                                   // 下半字符page+1
    LCD_SetPageCol(page & 0x07, col, 1);
    for (i = 6; i < 12; i++)
    {
        LCD_WR_DATA8(~(ASCIIchardot[ch][i]));
    }
    page--;
}
