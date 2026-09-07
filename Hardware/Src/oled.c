
#include "oled.h"
#include "stdlib.h"
#include "oledfont.h"
#include "delay.h"
#include "spi.h"
#include "string.h"



uint8_t frame_buffer[8][128];    //波形图绘制


uint16_t BACK_COLOR, POINT_COLOR; // 背景色，画笔色
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

void LCD_Writ_Bus(char dat) // 串行数据写入
{
    uint8_t i;
    OLED_CS_Clr();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&dat, 1, 1000);
    OLED_CS_Set();
}

void LCD_WR_DATA8(char da) // 发送8位数据
{                          // OLED_CS_Clr();
    OLED_A0_Set();
    LCD_Writ_Bus(da);
}
void LCD_WR_DATA(int da) // 发送16位数据
{                        //	OLED_CS_Clr();
    OLED_A0_Set();
    LCD_Writ_Bus(da >> 8);
    LCD_Writ_Bus(da);
}
void LCD_WR_REG(char da) // 发送8位命令
{                        //	OLED_CS_Clr();
    OLED_A0_Clr();
    LCD_Writ_Bus(da);
}
void LCD_WR_REG_DATA(char reg, int da)
{
    LCD_WR_REG(reg);
    LCD_WR_DATA(da);
}

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

void LCD_init(void)
{
    OLED_CS_Clr(); // 打开片选使能
    OLED_RST_Clr();
    delay_ms(20);
    OLED_RST_Set();
    delay_ms(20);
    OLED_CS_Set();

    LCD_WR_REG(0xE2);   // initialize interal function
    LCD_WR_REG(0x2F);   // power control(VB,VR,VF=1,1,1)
    LCD_WR_REG(0x23);   // Regulator resistor select(RR2,RR1,VRR0=0,1,1)
    LCD_WR_REG(0xA2);   // set LCD bias=1/9(BS=0)
    LCD_WR_REG(0x81);   // set reference voltage
    LCD_WR_REG(0x25);   // Set electronic volume (EV) level
    // LCD_WR_REG(0xC8);   // set SHL COM1 to COM64
    // LCD_WR_REG(0xA1);   // ADC select SEG1 to SEG132
    LCD_WR_REG(0xC0);	// set SHL COM64 to COM1
    LCD_WR_REG(0xA0);	// ADC select SEG132 to SEG1
    LCD_WR_REG(0x40);   // Initial Display Line
    LCD_WR_REG(0xA6);   // set reverse display OFF
    LCD_WR_REG(0xA4);   // set all pixels OFF
    LCD_FullFill(0x00); // full clear
    LCD_WR_REG(0xAF);   // turns the display ON
}

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

// 显示字符
void LCD_ShowChar(uint8_t col, uint8_t page, uint8_t Order)
{
    uint8_t i;
    uint8_t ch = Order - 0x20;                // ASCII字符从0x20开始
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0); // Set Page Address
    LCD_WR_REG(((col) >> 4) | 0x10);          // Set Column Address High Byte
    LCD_WR_REG(col & 0x0F);                   // Low Byte Column Address
    for (i = 0; i < 6; i++)                   // 上半部分6列
    {
        LCD_WR_DATA8(ASCIIchardot[ch][i]);
    }
    page++;                                   // 下半字符page+1
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0); // Set Page Address
    LCD_WR_REG(((col) >> 4) | 0x10);          // Set Column Address High Byte
    LCD_WR_REG(col & 0x0F);                   // Low Byte Column Address
    for (i = 6; i < 12; i++)                  // 下半部分6列
    {
        LCD_WR_DATA8(ASCIIchardot[ch][i]);
    }
    page--; // 写完一个字符page还原
}

// 计算ASCII字符的有效宽度（去除左右空白）
// 字符数据结构：前6字节=上半部分6列，后6字节=下半部分6列，每字节代表一列的8行像素
// 返回值：有效宽度（列数），left表示左边空白列数
static uint8_t LCD_GetCharWidth(uint8_t ch, uint8_t *left)
{
    uint8_t idx = ch - 0x20; // ASCII字符从0x20开始
    uint8_t first_col = 6, last_col = 0;
    
    // 扫描上半部分6列（每列1字节）
    for (uint8_t col = 0; col < 6; col++) {
        if (ASCIIchardot[idx][col] != 0) {
            if (col < first_col) first_col = col;
            if (col > last_col) last_col = col;
        }
    }
    
    // 扫描下半部分6列（每列1字节）
    for (uint8_t col = 6; col < 12; col++) {
        if (ASCIIchardot[idx][col] != 0) {
            if ((col - 6) < first_col) first_col = col - 6;
            if ((col - 6) > last_col) last_col = col - 6;
        }
    }
    
    if (last_col < first_col) {
        *left = 0;
        return 0; // 空白字符
    }
    
    *left = first_col;
    return last_col - first_col + 1;
}

// 紧凑显示单个ASCII字符（只绘制有效宽度，字母间空一列）
uint8_t LCD_ShowCharCompact(uint8_t col, uint8_t page, uint8_t ch)
{
    uint8_t left, width;
    width = LCD_GetCharWidth(ch, &left);
    
    if (width == 0) {
        // 空格字符（如字符串内部的空格），保留3列宽度
        return 3;
    }
    
    uint8_t idx = ch - 0x20; // ASCII字符从0x20开始
    
    // 显示上半部分（只绘制有效宽度）
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0);
    LCD_WR_REG(((col) >> 4) | 0x10);
    LCD_WR_REG(col & 0x0F);
    for (uint8_t i = left; i < left + width; i++) {
        LCD_WR_DATA8(ASCIIchardot[idx][i]);
    }
    
    // 显示下半部分（只绘制有效宽度）
    page++;
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0);
    LCD_WR_REG(((col) >> 4) | 0x10);
    LCD_WR_REG(col & 0x0F);
    for (uint8_t i = left + 6; i < left + width + 6; i++) {
        LCD_WR_DATA8(ASCIIchardot[idx][i]);
    }
    page--;
    
    // 严格只空1列间距
    return width + 1;
}

// 紧凑显示字符串（支持中英文混合）
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

        // 判断是否为汉字（双字节/三字节字符）
        if (*puts >= 0x80)
        {
            uint8_t index = GetHzIndex((char *)puts);
            if (index != 0xFF)
            {
                // 12x12 汉字显示
                LCD_WR_REG(ComTable[page & 0x07] | 0xB0);
                LCD_WR_REG(((col) >> 4) | 0x10);
                LCD_WR_REG(col & 0x0F);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
                
                LCD_WR_REG(ComTable[(page + 1) & 0x07] | 0xB0);
                LCD_WR_REG(((col) >> 4) | 0x10);
                LCD_WR_REG(col & 0x0F);
                for (uint8_t i = 12; i < 24; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
            }
            if ((*puts & 0xF0) == 0xE0)
                puts += 3; // UTF-8 三字节
            else
                puts += 2; // GB2312 双字节
            col += 12;     // 12x12 汉字宽度 + 1列间距
        }
        else // ASCII 字符 - 使用紧凑显示
        {
            col += LCD_ShowCharCompact(col, page, *puts);
            puts++;
        }
    }
}

// 显示单个汉字 (16x16)
void LCD_ShowChinese(uint8_t col, uint8_t page, char *hz)
{
    uint8_t i;
    uint8_t index = GetHzIndex(hz);

    if (index == 0xFF) // 未找到字库
        return;

    // 显示上半部分 (8行)
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0);
    LCD_WR_REG(((col) >> 4) | 0x10);
    LCD_WR_REG(col & 0x0F);
    for (i = 0; i < 16; i++)
    {
        LCD_WR_DATA8(Hzk[index][i]);
    }

    // 显示下半部分 (8行)
    page++;
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0);
    LCD_WR_REG(((col) >> 4) | 0x10);
    LCD_WR_REG(col & 0x0F);
    for (i = 16; i < 32; i++)
    {
        LCD_WR_DATA8(Hzk[index][i]);
    }
}

// 显示字符串（支持中英文混合，兼容UTF-8和GB2312）
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

        // 判断是否为汉字（双字节/三字节字符）
        if (*puts >= 0x80)
        {
            uint8_t index = GetHzIndex((char *)puts);
            if (index != 0xFF)
            {
                // 12x12 汉字显示（3页，每页12字节）
                // 第1页
                LCD_WR_REG(ComTable[page & 0x07] | 0xB0);
                LCD_WR_REG(((col) >> 4) | 0x10);
                LCD_WR_REG(col & 0x0F);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
                // 第2页
                LCD_WR_REG(ComTable[(page + 1) & 0x07] | 0xB0);
                LCD_WR_REG(((col) >> 4) | 0x10);
                LCD_WR_REG(col & 0x0F);
                for (uint8_t i = 12; i < 24; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
            }
            // 根据编码类型跳过字节
            if ((*puts & 0xF0) == 0xE0)
                puts += 3; // UTF-8 三字节
            else
                puts += 2; // GB2312 双字节
            col += 12;     // 12x12 汉字宽度
        }
        else // ASCII 字符
        {
            LCD_ShowChar(col, page, *puts);
            puts++;
            col += 8;
        }
    }
}

// 显示字符串(ASCII only)
void LCD_ShowStr(uint8_t col, uint8_t page, uint8_t *puts)
{
    while (*puts != '\0') // 判断字符串是否显示完毕
    {
        if (col > (LCD_W - 8)) // 判断行末是否够放一个字符，否则换行
        {
            page = page + 2;
            col = 0;
        }
        if (page > (LCD_H / 8 - 2)) // 判断屏末是否够放一个字符，否则返回
        {
            page = 0;
            col = 0;
        }
        LCD_ShowChar(col + 3, page, *puts);
        puts++;
        col = col + 8; // 下一个字符
    }
}

// 显示三位数(0-255)
void LCD_ShowNum(uint8_t col, uint8_t page, uint8_t Num)
{
    uint8_t a, b, c;
    a = Num / 100;
    b = (Num % 100) / 10;
    c = Num % 10;
    if (a == 0)
        ; // 不写空格,直接跳过//PutChar(col,page,0x20);
    else
        LCD_ShowChar(col, page, a + 0x30);
    if (a == 0 && b == 0)
        ; // 不写空格,直接跳过//LCD_ShowChar(col,page,0x20);
    else
        LCD_ShowChar(col + 8, page, b + 0x30);
    LCD_ShowChar(col + 16, page, c + 0x30);
}

// 显示图片
void LCD_ShowBmp(uint8_t const *puts)
{
    uint8_t i, j;
    uint16_t X = 0;
    for (i = 0; i < (LCD_H / 8); i++)
    {
        LCD_WR_REG(0xB0 | ComTable[i]); // Set Page Address
        LCD_WR_REG(0x10);               // Set Column Address = 0
        LCD_WR_REG(0x04);               // Colum from S1 -> S128 auto add             //
        for (j = 0; j < LCD_W; j++)
        {
            LCD_WR_DATA8(puts[X]);
            X++;
        }
    }
}

// 设置OLED显存地址
// oled.c
void OLED_Set_Pos(uint8_t col, uint8_t page)
{
    // 这里的 +3 和 ComTable 必须保留，因为这是你屏幕正常的物理映射
    uint8_t real_col = col + 3;
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0); // Set Page Address
    LCD_WR_REG(((col + 1) >> 4) | 0x10);      // Set Column Address High Byte
    LCD_WR_REG(col & 0x0F);                   // Low Byte Column Address
}

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

// 计算6x8字符的有效宽度（去除左右空白）
// 返回值：有效宽度（列数），left表示左边空白列数
static uint8_t LCD_GetSmallCharWidth(uint8_t ch, uint8_t *left)
{
    uint8_t idx = ch - ' '; // ASCII字符从' '开始
    uint8_t first_col = 6, last_col = 0;
    
    // 扫描6列（每列1字节，8行）
    for (uint8_t col = 0; col < 6; col++) {
        if (F6x8[idx][col] != 0) {
            if (col < first_col) first_col = col;
            if (col > last_col) last_col = col;
        }
    }
    
    if (last_col < first_col) {
        *left = 0;
        return 0; // 空白字符
    }
    
    *left = first_col;
    return last_col - first_col + 1;
}

// 紧凑显示单个6x8字符（只绘制有效宽度，字母间空一列）
static uint8_t LCD_ShowSmallCharCompact(uint8_t col, uint8_t page, uint8_t ch)
{
    uint8_t left, width;
    width = LCD_GetSmallCharWidth(ch, &left);
    
    if (width == 0) {
        return 3; // 空格字符，保留3列宽度
    }
    
    uint8_t idx = ch - ' '; // ASCII字符从' '开始
    
    OLED_Set_Pos(col, page);
    for (uint8_t i = left; i < left + width; i++) {
        LCD_WR_DATA8(F6x8[idx][i]);
    }
    
    return width + 1; // 返回实际占用宽度（有效宽度 + 1列间距）
}

// 紧凑显示小字体字符串（6x8）
void LCD_ShowStr_Small_Compact(uint8_t col, uint8_t page, const char *str)
{
    while (*str != '\0')
    {
        col += LCD_ShowSmallCharCompact(col, page, *str);
        str++;
    }
}

// 计算紧凑显示后的字符串宽度（用于箭头位置计算）
static uint8_t LCD_GetStrCompactWidth(const char *str)
{
    uint8_t width = 0;
    if (str == NULL) return 0;
    
    while (*str != '\0') {
        if (*str >= 0x80) {
            // 中文字符
            width += 12;
            if ((*str & 0xF0) == 0xE0)
                str += 3; // UTF-8 三字节
            else
                str += 2; // GB2312 双字节
        } else {
            // ASCII 字符 - 计算紧凑宽度
            if (*str == ' ') {
                width += 3; // 空格保留3列
            } else if (*str >= '0' && *str <= '9') {
                width += 9; // 数字固定8列 + 1列间距
            } else {
                // 其他ASCII字符计算有效宽度
                uint8_t idx = *str - 0x20;
                if (idx < 95) { // 有效ASCII范围
                    uint8_t first_col = 6, last_col = 0;
                    // 扫描上半部分6列
                    for (uint8_t col = 0; col < 6; col++) {
                        if (ASCIIchardot[idx][col] != 0) {
                            if (col < first_col) first_col = col;
                            if (col > last_col) last_col = col;
                        }
                    }
                    // 扫描下半部分6列
                    for (uint8_t col = 6; col < 12; col++) {
                        if (ASCIIchardot[idx][col] != 0) {
                            if ((col - 6) < first_col) first_col = col - 6;
                            if ((col - 6) > last_col) last_col = col - 6;
                        }
                    }
                    if (last_col >= first_col) {
                        width += (last_col - first_col + 1) + 1; // 有效宽度 + 1列间距
                    } else {
                        width += 3; // 空白字符
                    }
                } else {
                    width += 3; // 非法字符
                }
            }
            str++;
        }
    }
    return width;
}

// 智能箭头显示函数
// mode: 0=右对齐(默认), 1=左对齐(文本前), 2=指定位置, 3==自动计算字符串宽度后显示
// col: 当mode=2/3时为文本起始列
// str: 当mode=3时为要计算宽度的字符串
void LCD_ShowArrowEx(uint8_t page, uint8_t mode, uint8_t col, const char *str)
{
    uint8_t arrowCol;
    
    switch (mode) {
        case 1: // 左对齐(文本前)
            arrowCol = 0;
            break;
        case 2: // 指定位置
            arrowCol = col;
            break;
        case 3: // 自动计算紧凑字符串宽度后显示
            {
                uint8_t textWidth = LCD_GetStrCompactWidth(str);
                arrowCol = col + textWidth;
            }
            break;
        case 0: // 右对齐(默认)
        default:
            arrowCol = LCD_W - 10;
            break;
    }
    
    LCD_ShowStr(arrowCol, page, (uint8_t *)">");
}

// 内部安全绘图宏：负责自动处理跨页映射，杜绝内存越界
#define DRAW_PIXEL(x, y) do { \
    if ((x) >= 0 && (x) < 128 && (y) >= 0 && (y) < 64) { \
        frame_buffer[(y) / 8][(x)] |= (1 << (7 - ((y) % 8))); \
    } \
} while(0)

// 注意：为了STM32F1的性能，x_scale 和 y_scale 建议传入整型(uint8_t)
void LCD_DrawEchoCurve(uint8_t *echo_data, uint8_t x_scale, uint8_t y_scale)
{
    uint8_t i, j;
    
    // --- 坐标系基准参数 ---
    const int AXIS_X = 8;    // Y轴的水平位置 (X坐标)
    const int AXIS_Y = 55;   // X轴的垂直位置 (Y坐标)

    
    // 1. 初始化显存数组 (局部变量，用完释放栈空间)
 
    memset(frame_buffer, 0, sizeof(frame_buffer));
    
    // 2. 绘制Y轴：从原点(58)向上画到顶(8)，杜绝画穿屏幕
    for (int y = 15; y <= LCD_H-1; y++) {
        DRAW_PIXEL(AXIS_X, y);
    }
    
    // 3. 绘制X轴：从原点(8)向右画到屏幕边缘(127)
    for (int x = 0; x <= LCD_W-1; x++) {
        DRAW_PIXEL(x, AXIS_Y);
    }
    
    // 4. 绘制Y轴箭头
    for (int h = 16; h < 21; h++) {
        int y =   h;       
        int w = ((h-16) < 2) ? 0 : ((h-16) / 2);  // 宽度渐变，形成细长实心箭头
        for (int x = AXIS_X - w; x <= AXIS_X + w; x++) {
            DRAW_PIXEL(x, y);
        }
    }
    
    // 5. 绘制X轴箭头（顶点在右侧 x=127 处，向左展开）
    for (int h = 0; h < 5; h++) {
        int x = 127 - h;     
        int w = (h < 2) ? 0 : (h / 2);  
        for (int y = AXIS_Y - w; y <= AXIS_Y + w; y++) {
            DRAW_PIXEL(x, y);
        }
    }
    
    // 6. 绘制X轴刻度线（在X轴下方，向Y值较大的方向凸起）
    for (i = 0; i <= 4; i++) {
        int x = AXIS_X + (i + 1) * 27;
        if (x > 120) break;  // 避开右侧箭头区域
        
        //刻度
        for (int y = AXIS_Y- 2; y <= AXIS_Y ; y++) {
            DRAW_PIXEL(x, y);
        }
    }
    
    // 7. 绘制平滑连续的波形曲线
    if (echo_data != NULL) {
        int prev_y = -1; // 记录前一个点的Y坐标，用于连线
        
        // X轴限制：从 x=10 开始，画到 x=121 结束，严格保证在坐标轴内部
        for (int x = AXIS_X + 2; x < 122; x++) {
            
            // 纯整数计算 X 轴对应的数据索引
            int data_idx = (x - AXIS_X - 2) * x_scale; 
            if (data_idx >= 128) break;  // 防止读取原始数据越界
            if (data_idx < 0) continue;
            
            // 纯整数计算 Y 坐标 (将原本的 * 0.20f 转换为等价的 * 1 / 5，极大节省CPU开销)
            int y = AXIS_Y - ((echo_data[data_idx] * y_scale * 1) / 5);     //最大在230左右的数据正常显示
            
            // --- 核心边界保护 ---
            if (y < 15) y = 15; // 最高不能冲破 Y=15
            if (y > AXIS_Y) y = AXIS_Y;     // 最低不能掉下 Y=58 (X轴)
            
            DRAW_PIXEL(x, y);
            
            // 垂直连线修复：连接当前点与前一个点的垂直断层，保持曲线1像素等宽
            if (prev_y != -1) {
                // start和end收紧1个像素，避免在拐点处重复描点
                int start = (y < prev_y) ? y + 1 : prev_y + 1;
                int end   = (y > prev_y) ? y - 1 : prev_y - 1;
                for (int fill_y = start; fill_y <= end; fill_y++) {
                    DRAW_PIXEL(x, fill_y);
                }
            }
            prev_y = y; // 更新记忆
        }
    }
    // // 7. 绘制平滑连续的波形曲线
    // if (echo_data != NULL) {
    //     int prev_x = -1;
    //     int prev_y = -1;

    //     for (int x = AXIS_X + 2; x < 122; x++) {
    //         int data_idx = (int)((x - AXIS_X - 2) * x_scale);
    //         if (data_idx >= 128) break;
    //         if (data_idx < 0) continue;
    //         // 计算当前点的Y坐标（数据越大，峰值越向上，即Y值越小）
    //         int y = AXIS_Y - (int)(echo_data[data_idx] * y_scale * 1/3);
    //         // 严格边界保护
    //         if (y < 0) y = 0;
    //         if (y > AXIS_Y) y = AXIS_Y; 
    //         DRAW_PIXEL(x, y);        
    //         // 垂直连线修复：连接当前点与前一个点的垂直断层
    //         if (prev_y != -1) {
    //             int start = (y < prev_y) ? y : prev_y;
    //             int end = (y > prev_y) ? y : prev_y;
    //             // 在当前x列填充垂线
    //             for (int fill_y = start; fill_y <= end; fill_y++) {
    //                 DRAW_PIXEL(x, fill_y);
    //             }
    //             // 在x-1列同样填充，确保两列之间无空隙
    //             if (prev_x == x - 1) {
    //                 for (int fill_y = start; fill_y <= end; fill_y++) {
    //                     DRAW_PIXEL(prev_x, fill_y);
    //                 }
    //             }
    //         }
    //         prev_x = x;
    //         prev_y = y;
    //     }
    // }
    
    // 8. 按照 AiP31567 驱动特性刷入硬件显存
    for (i = 0; i < 8; i++) {
        LCD_WR_REG(ComTable[i] | 0xB0); // Page 地址
        LCD_WR_REG(0x10);               // 列地址高位
        LCD_WR_REG(0x00);               // 列地址低位
        for (j = 0; j < 128; j++) {
            LCD_WR_DATA8(frame_buffer[i][j]);
        }
    }

    // 9. 绘制文字
    LCD_ShowStr_Small_Compact(11, 7, "0.01");
    LCD_ShowStr_Small_Compact(55, 7, "m(d)");
    LCD_ShowStr_Small_Compact(98, 7, "8.91");
}

void LCD_ShowCharReverse(uint8_t col, uint8_t page, uint8_t Order)
{
    uint8_t i;
    uint8_t ch = Order - 0x20;                // ASCII字符从0x20开始
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0); // Set Page Address
    LCD_WR_REG(((col + 1) >> 4) | 0x10);      // Set Column Address High Byte
    LCD_WR_REG(col & 0x0F);                   // Low Byte Column Address
    for (i = 0; i < 6; i++)                   // 上半部分6列
    {
        LCD_WR_DATA8(~(ASCIIchardot[ch][i]));
    }
    page++;                                   // 下半字符page+1
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0); // Set Page Address
    LCD_WR_REG(((col + 1) >> 4) | 0x10);      // Set Column Address High Byte
    LCD_WR_REG(col & 0x0F);                   // Low Byte Column Address
    for (i = 6; i < 12; i++)                  // 下半部分6列
    {
        LCD_WR_DATA8(~(ASCIIchardot[ch][i]));
    }
    page--; // 写完一个字符page还原
}