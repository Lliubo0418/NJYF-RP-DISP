<<<<<<< HEAD
=======
/* 显示板 OLED 屏底层驱动（Hardware 层）：只把字节/字模写进 GDDRAM，"画什么"由 APP 层 oled_ui.c 决定。
 * 硬件：SPI1 单线半双工（无 MISO），SCK=PA5、MOSI=PA7；屏 128x64、ST7565/AiP31567 系 COG。
 * 【坐标系】原点在物理左上角：col 为像素列 0~127，page 为"页" 0~7（1 页 = 8 行），字节 bit7 在上。
 * ⚠ 逻辑 page 与芯片 page 相反，由 ComTable[i] = 7 - i 翻转：凡 "ComTable[page] | 0xB0"
 *   都必须当成"写第 page 页"来读。屏幕 180° 旋转由 0xC0/0xA0 与列地址镜像（o → 4-o）配套实现，
 *   开关见 oled.h 的 LCD_SCAN_ROT180，改一处必须同步另一处。
 * 【显存/刷新】无整屏软显存（仅 LCD_DrawEchoCurve 私有的 frame_buffer[8][128]），所有函数直写 GDDRAM。
 * ⚠ 每帧重画前必须自行清屏（LCD_FullFill / LCD_ClearLine）或整字宽重画，否则叠加成"叠影"；
 *   绘制函数只点亮字模里为 1 的像素、不写背景。 */
>>>>>>> origin/develop

#include "oled.h"
#include "stdlib.h"
#include "oledfont.h"
<<<<<<< HEAD
#include "delay.h"
#include "spi.h"
#include "string.h"



uint8_t frame_buffer[8][128];    //波形图绘制


uint16_t BACK_COLOR, POINT_COLOR; // 背景色，画笔色
=======
#include "spi.h"
#include "string.h"

/* 曲线视图专用软件显存，组织同屏幕页矩阵 [page][col]，每字节 bit7 在上；仅 LCD_DrawEchoCurve 使用 */
uint8_t frame_buffer[8][128];

/* 前景/背景色：本屏单色，底层绘制不查询，仅为兼容 oled.h 的 extern 接口（0 = 不点亮，非 0 = 点亮） */
uint16_t BACK_COLOR, POINT_COLOR; // 背景色，画笔色

/* 逻辑页 → 芯片页 映射表：ComTable[i] = 7 - i，用 ComTable[page] | 0xB0 拼页地址命令，
 * 抵消屏在整机上旋转 180° 的安装方向（配合 LCD_init 里的 0xC0/0xA0）。 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
void LCD_Writ_Bus(char dat) // 串行数据写入
=======
/**
 * @brief  向 OLED 串行写入 1 个字节（最底层收发原语）
 * @param  dat 待发送字节（0x00~0xFF），MSB 先发
 * @retval 无
 */
void LCD_Writ_Bus(char dat)
>>>>>>> origin/develop
{
    uint8_t i;
    OLED_CS_Clr();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&dat, 1, 1000);
    OLED_CS_Set();
}

<<<<<<< HEAD
void LCD_WR_DATA8(char da) // 发送8位数据
{                          // OLED_CS_Clr();
    OLED_A0_Set();
    LCD_Writ_Bus(da);
}
void LCD_WR_DATA(int da) // 发送16位数据
{                        //	OLED_CS_Clr();
=======
/**
 * @brief  写 1 个字节显示数据（A0=1）
 * @param  da 待写入的 8bit 数据，1 = 点亮像素
 * @retval 无
 */
void LCD_WR_DATA8(char da)
{
    OLED_A0_Set();
    LCD_Writ_Bus(da);
}

/**
 * @brief  写 16 位数据（高字节在前，占同页相邻 2 列）
 * @param  da 16bit 数据，实际只取低 16 位
 * @retval 无
 */
void LCD_WR_DATA(int da)
{
>>>>>>> origin/develop
    OLED_A0_Set();
    LCD_Writ_Bus(da >> 8);
    LCD_Writ_Bus(da);
}
<<<<<<< HEAD
void LCD_WR_REG(char da) // 发送8位命令
{                        //	OLED_CS_Clr();
    OLED_A0_Clr();
    LCD_Writ_Bus(da);
}
void LCD_WR_REG_DATA(char reg, int da)
{
=======

/**
 * @brief  写 1 个字节命令（A0=0），本文件所有命令字节的唯一入口
 * @param  da 命令字节
 * @retval 无
 */
void LCD_WR_REG(char da)
{
    /* 常用命令：0xB0~0xB7 页地址；0x00~0x0F / 0x10~0x1F 列地址低/高 4 位；0x40~0x7F 起始行；
     * 0xA0/0xA1 SEG 方向；0xC0/0xC8 COM 方向；0xAE/0xAF 关/开显示；0xA6/0xA7 正常/反显；0x81 电子音量 */
    OLED_A0_Clr();
    LCD_Writ_Bus(da);
}

/**
 * @brief  先发一条命令，再发一个 16 位参数
 * @param  reg 命令字节（例如 0x81 设置电子音量）
 * @param  da  命令参数，按 16 位发送
 * @retval 无
 */
void LCD_WR_REG_DATA(char reg, int da)
{
    /* ⚠ 参数按 16 位发（2 字节），而本屏带参命令大多只需 1 字节，启用前须确认宽度 */
>>>>>>> origin/develop
    LCD_WR_REG(reg);
    LCD_WR_DATA(da);
}

<<<<<<< HEAD
void LCD_FullFill(uint8_t FillData)
{
=======
/**
 * @brief  设置后续写入的起始「逻辑页 + 逻辑列」，页地址与列地址一次下发
 * @param  page 逻辑页号 0~7（0 = 屏幕最上方）
 * @param  col  程序侧列号 0~127（不含调用方自己的基准 o）
 * @param  o    调用方名义列偏移（0 或 1），旋转时用它做镜像换算
 * @retval 无
 */
static void LCD_SetPageCol(uint8_t page, uint8_t col, uint8_t o)
{
    /* ⚠ 全文件唯一的列地址出口。芯片 132 列而屏可见区 128 列，倒装时偏移须镜像为 4-o、
     *   正装为 o，否则画面会被甩到不可见的 4 列上（开关见 LCD_SCAN_ROT180） */
#if LCD_SCAN_ROT180
    uint8_t c = (uint8_t)(col + ((132u - 128u) - o));   /* 倒装：偏移镜像为 4-o */
#else
    uint8_t c = (uint8_t)(col + o);                     /* 正装：沿用原偏移 */
#endif
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0);           /* 页地址（逻辑页→芯片页） */
    LCD_WR_REG((uint8_t)(((c >> 4) & 0x0F) | 0x10));    /* 列地址高 4 位 */
    LCD_WR_REG((uint8_t)(c & 0x0F));                    /* 列地址低 4 位 */
}

/**
 * @brief  用同一个字节填满整个 GDDRAM（整屏全亮 / 全灭）
 * @param  FillData 填充字节：0x00 全屏熄灭，0xFF 全屏点亮
 * @retval 无
 */
void LCD_FullFill(uint8_t FillData)
{
    /* ⚠ 循环写 9 页（0xB0|Y，Y 取 0~8）：page8 是 icon 页（只有 D0 有效），
     *   不要以"屏只有 8 页"为由改成 8 次，否则漏清 icon 行；每页 132 列（可见 128 + 不可见 4） */
>>>>>>> origin/develop
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
<<<<<<< HEAD
=======

/**
 * @brief  清空指定的一个逻辑页（8 个像素行）
 * @param  page 逻辑页号 0~7；>7 直接返回
 * @retval 无
 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
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
=======
/**
 * @brief  用同一个字节填满一个矩形区域（逻辑页 × 逻辑列）
 * @param  col   起始逻辑列 0~127
 * @param  page  起始逻辑页 0~7
 * @param  width 宽度（列数），0 直接返回
 * @param  pages 高度（页数），0 直接返回
 * @param  data  填充字节（0x00 / 0xFF）
 * @retval 无
 */
void LCD_FillBlock(uint8_t col, uint8_t page, uint8_t width, uint8_t pages, uint8_t data)
{
    /* ⚠ 颜色语义（负显 DFSTN 黑底白字）：0xFF = 亮块、0x00 = 黑块；正显批次（FSTN）相反，装机前按实物确认
     * 越界按可见区截断（col+width > 128、page+pages > 8 均收窄），不会写到屏外或 icon 页上 */
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
 * @brief  OLED 初始化：硬件复位 + 按序下发上电配置命令 + 清屏 + 开显示
 * @retval 无
 */
void LCD_init(void)
{
    /* 复位：CS 拉低 → RST 输出 20ms 低脉冲 → RST 拉高 → 再等 20ms → CS 拉高
     * ⚠ 第二段延时 20ms 超出手册 4.11.1 的 ≤5ms 上限；电源控制命令置于最前（手册要求排在 RR/EV 之后）
     *   —— 两处均为已知偏离 */
    OLED_CS_Clr();
    OLED_RST_Clr();
    HAL_Delay(20); /* 复位保持（手册要求 tRW 至少 5µs） */
    OLED_RST_Set();
    HAL_Delay(20);
    OLED_CS_Set();

    LCD_WR_REG(0xE2); // initialize interal function
    LCD_WR_REG(0x2F); // power control(VB,VR,VF=1,1,1)
    LCD_WR_REG(0x23); // Regulator resistor select(RR2,RR1,VRR0=0,1,1)
    LCD_WR_REG(0xA2); // set LCD bias=1/9(BS=0)
    LCD_WR_REG(0x81); // set reference voltage
    LCD_WR_REG(0x25); // Set electronic volume (EV) level
    /* ⚠ 显示方向（旋转 180°）：MY（0xC0/0xC8）管上下、MX（0xA0/0xA1）管左右，两条一起取反才是
     *   干净的 180°（只翻一条会上下颠倒或左右镜像）；配套的列地址镜像在 LCD_SetPageCol() 里，
     *   两处必须同进同退，开关见 LCD_SCAN_ROT180 */
#if LCD_SCAN_ROT180
    LCD_WR_REG(0xC8);   // MY=1：反向扫描（COM63~COM0）
    LCD_WR_REG(0xA1);   // MX=1：反向显示（SEG131~SEG0）
#else
    LCD_WR_REG(0xC0);   // MY=0：普通扫描（COM0~COM63）
    LCD_WR_REG(0xA0);   // MX=0：普通显示（SEG0~SEG131）
#endif
    LCD_WR_REG(0x40);   // 显示起始行 S[5:0]=0
    LCD_WR_REG(0xA6);   // INV=0：普通显示（非反显）
    LCD_WR_REG(0xA4);   // AP=0：普通显示（取消"屏全亮"）
    LCD_FullFill(0x00);
    LCD_WR_REG(0xAF);   // turns the display ON
}

/**
 * @brief  产测图案：按页写入一对交替字节，铺满整个屏幕
 * @param  dat1 每列第一个字节
 * @param  dat2 每列第二个字节
 * @retval 无
 */
void display(uint8_t dat1, uint8_t dat2)
{
    /* ⚠ 页地址用 0xB0~0xB7 原始值（未过 ComTable），上下方向与常规绘制函数相反 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
void displaychar(uint8_t const *p)
{
=======
/**
 * @brief  产测图案：把一段连续字节整体铺屏（8 页 × 128 列 = 1024 字节）
 * @param  p 指向位图数据，长度至少 1024 字节
 * @retval 无
 */
void displaychar(uint8_t const *p)
{
    /* ⚠ 不做长度校验，调用方须保证 ≥1024 字节；页地址未过 ComTable，
     *   与做了翻转的 LCD_ShowBmp 显示同一张图会上下颠倒 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
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
=======
/**
 * @brief  在指定位置显示 1 个 ASCII 字符（8x16 字模，固定 6 列宽、跨 2 页）
 * @param  col   起始列 0~127；字模占 col ~ col+5，col > 121 时右侧被截断
 * @param  page  上半部逻辑页 0~7（内部对 8 取模）；下半部为 page+1
 * @param  Order ASCII 码 0x20~0x7E；小于 0x20 会导致负下标越界
 * @retval 无
 */
void LCD_ShowChar(uint8_t col, uint8_t page, uint8_t Order)
{
    /* 字模取自 oledfont.h 的 ASCIIchardot[95][12]（索引 = 码 - 0x20），前 6 字节上半部、后 6 字节下半部
     * ⚠ 只点亮字模里为 1 的像素、不写背景，在已有内容上画会叠加；⚠ 列地址不做 +1 补偿，
     *   与 LCD_ShowCharReverse / OLED_Set_Pos 横向相差 1 列 */
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

/**
 * @brief  测量 8x16 ASCII 字模的实际墨迹宽度，并回传左侧空白列数
 * @param  ch   ASCII 字符码 0x20~0x7E
 * @param  left 输出：左侧空白列数（0~5），整字空白时为 0，不允许为 NULL
 * @retval 字模有效宽度（1~6）；空白字符返回 0
 */
static uint8_t LCD_GetCharWidth(uint8_t ch, uint8_t *left)
{
    /* 紧凑排版用：下标 0~5 为上半部、6~11 为下半部，扫下半部时须减 6 归一到 0~5，
     * 上下两半才能在同一条列坐标上比较 */
    uint8_t idx = ch - 0x20;
    uint8_t first_col = 6, last_col = 0;

>>>>>>> origin/develop
    for (uint8_t col = 0; col < 6; col++) {
        if (ASCIIchardot[idx][col] != 0) {
            if (col < first_col) first_col = col;
            if (col > last_col) last_col = col;
        }
    }
<<<<<<< HEAD
    
    // 扫描下半部分6列（每列1字节）
=======

>>>>>>> origin/develop
    for (uint8_t col = 6; col < 12; col++) {
        if (ASCIIchardot[idx][col] != 0) {
            if ((col - 6) < first_col) first_col = col - 6;
            if ((col - 6) > last_col) last_col = col - 6;
        }
    }
    
    if (last_col < first_col) {
        *left = 0;
<<<<<<< HEAD
        return 0; // 空白字符
=======
        return 0;
>>>>>>> origin/develop
    }
    
    *left = first_col;
    return last_col - first_col + 1;
}

<<<<<<< HEAD
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
=======
/**
 * @brief  紧凑显示 1 个 ASCII 字符：只画墨迹部分，右侧留 1 列间距
 * @param  col  起始列 0~127（不校验，越界会折回左侧）
 * @param  page 上半部逻辑页 0~7（下半部为 page+1，可能回绕）
 * @param  ch   ASCII 码 0x20~0x7E；<0x20 会造成字模负下标越界
 * @retval 本字符实际占用列数 = 有效宽度 + 1；空白字符返回 3
 */
uint8_t LCD_ShowCharCompact(uint8_t col, uint8_t page, uint8_t ch)
{
    /* ⚠ 返回值含 1 列间距，调用方必须累加返回值定位下一个字符，不能再用固定 8 列推进 */
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
>>>>>>> origin/develop
    for (uint8_t i = left + 6; i < left + width + 6; i++) {
        LCD_WR_DATA8(ASCIIchardot[idx][i]);
    }
    page--;
<<<<<<< HEAD
    
    // 严格只空1列间距
    return width + 1;
}

// 紧凑显示字符串（支持中英文混合）
void LCD_ShowStrExCompact(uint8_t col, uint8_t page, uint8_t *puts)
{
=======

    return width + 1;
}

/**
 * @brief  紧凑显示中英文混排字符串（汉字 12x12 + ASCII 紧凑 8x16）
 * @param  col  起始列 0~127
 * @param  page 起始逻辑页 0~7
 * @param  puts 字符串首地址（UTF-8 或 GB2312），以 '\0' 结尾，不允许为 NULL
 * @retval 无
 */
void LCD_ShowStrExCompact(uint8_t col, uint8_t page, uint8_t *puts)
{
    /* 换行/回卷：col + 12 > 128 时 page += 2 并回到第 0 列；page >= 7 时回左上角 (0,0) 从头覆盖
     * ⚠ 覆盖式绘制（不写背景），用在有旧内容的区域会叠加
     * ⚠ 未做下越界保护：page=7 时汉字下半会经 & 0x07 绕回第 0 页；字库无此字（索引 0xFF）时只跳字节不画字 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
        // 判断是否为汉字（双字节/三字节字符）
=======
>>>>>>> origin/develop
        if (*puts >= 0x80)
        {
            uint8_t index = GetHzIndex((char *)puts);
            if (index != 0xFF)
            {
<<<<<<< HEAD
                // 12x12 汉字显示
                LCD_WR_REG(ComTable[page & 0x07] | 0xB0);
                LCD_WR_REG(((col) >> 4) | 0x10);
                LCD_WR_REG(col & 0x0F);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
                
                LCD_WR_REG(ComTable[(page + 1) & 0x07] | 0xB0);
                LCD_WR_REG(((col) >> 4) | 0x10);
                LCD_WR_REG(col & 0x0F);
=======
                LCD_SetPageCol(page & 0x07, col, 0);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);

                LCD_SetPageCol((page + 1) & 0x07, col, 0);
>>>>>>> origin/develop
                for (uint8_t i = 12; i < 24; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
            }
            if ((*puts & 0xF0) == 0xE0)
<<<<<<< HEAD
                puts += 3; // UTF-8 三字节
            else
                puts += 2; // GB2312 双字节
            col += 12;     // 12x12 汉字宽度 + 1列间距
        }
        else // ASCII 字符 - 使用紧凑显示
=======
                puts += 3;
            else
                puts += 2;
            col += 12;
        }
        else
>>>>>>> origin/develop
        {
            col += LCD_ShowCharCompact(col, page, *puts);
            puts++;
        }
    }
}

<<<<<<< HEAD
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
=======
/**
 * @brief  单个 ASCII 字符在紧凑排版下的推进量（列数）
 * @param  ch ASCII 字符码 0x20~0x7E；<0x20 会造成字模负下标越界
 * @retval 占用列数 = 有效宽度 + 1；空白字符返回 3
 */
static uint8_t LCD_AdvanceOf(uint8_t ch)
{
    /* ⚠ 与 LCD_ShowCharCompact() 的推进量必须是同一口径（有效墨迹宽度 + 1，空白 3）。
     *   凡"先量宽、再按宽度定位"一律用本函数：LCD_GetStrCompactWidth() 对数字固定按 9 列计，
     *   拿它算右对齐/箭头落点必然偏（既有缺陷） */
    uint8_t left;
    uint8_t w = LCD_GetCharWidth(ch, &left);
    return (w == 0u) ? 3u : (uint8_t)(w + 1u);
}

/**
 * @brief  量出 LCD_ShowStrExCompact() 实际会占用的列数
 * @param  str 以 '\0' 结束的字符串（UTF-8 / GB2312 / ASCII 混排）
 * @retval 占用列数；str 为 NULL 返回 0
 */
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
 * @brief  量出 LCD_ShowStrExBig() 实际会占用的列数
 * @param  str 以 '\0' 结束的字符串（只对 ASCII 部分计宽）
 * @retval 占用列数；str 为 NULL 返回 0
 */
uint16_t LCD_GetStrBigWidth(const char *str)
{
    /* 放大 2 倍 ⇒ 推进量也是 2 倍；汉字不参与放大，跳过并按 0 列计 */
    uint16_t width = 0u;
    if (str == NULL) return 0u;

    while (*str != '\0')
    {
        if ((uint8_t)*str >= 0x80u)
            str += ((*str & 0xF0) == 0xE0) ? 3 : 2;   /* 汉字：跳过、不计宽 */
        else
        {
            width += (uint16_t)(2u * LCD_AdvanceOf((uint8_t)*str));
            str++;
        }
    }
    return width;
}

/**
 * @brief  2 倍放大显示纯 ASCII 字符串（16 列 × 32 行，纵向占 4 个逻辑页）
 * @param  col  起始列 0~127
 * @param  page 起始逻辑页 0~4（占 page ~ page+3）
 * @param  str  以 '\0' 结束的 ASCII 串，不允许为 NULL（不校验）
 * @retval 无
 */
void LCD_ShowStrExBig(uint8_t col, uint8_t page, const char *str)
{
    /* 最近邻放大（横向 1→2 列、纵向 1→2 行），不新增字模、Flash 零增长；用于首页大字读数
     * ⚠ page 只能取 0~4，page>4 直接返回；⚠ 横向不换行，调用方须保证 LCD_GetStrBigWidth(str) <= 128；
     *   ⚠ 汉字不参与放大（遇 >= 0x80 按编码跳过、不画） */
    if (str == NULL || page > 4u) return;

    while (*str != '\0')
    {
        uint8_t ch = (uint8_t)*str;

        if (ch >= 0x80u)                     /* 汉字：跳过不画 */
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

                /* 4 个页各写一遍：第 k 页负责放大后第 k*8 ~ k*8+7 行 */
                for (uint8_t k = 0u; k < 4u; k++)
                {
                    LCD_SetPageCol((page + k) & 0x07, col, 0);

                    for (uint8_t j = 0u; j < (uint8_t)(2u * w); j++)
                    {
                        uint8_t sc  = (uint8_t)(left + (j >> 1));   /* 源列：每列画 2 遍 */
                        uint8_t up  = ASCIIchardot[idx][sc];        /* 上半部 8 行 */
                        uint8_t lo  = ASCIIchardot[idx][sc + 6];    /* 下半部 8 行 */
                        uint8_t out = 0u;

                        for (uint8_t r = 0u; r < 8u; r++)
                        {
                            uint8_t row = (uint8_t)(k * 8u + r);    /* 放大后行号 0~31 */
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
                col = (uint8_t)(col + 2u * w + 2u);   /* 推进量 = 2 ×(有效宽 + 1) */
            }
            else
            {
                col = (uint8_t)(col + 6u);            /* 空格：2 × 3 列 */
            }
        }
        str++;
    }
}

/**
 * @brief  显示中英文混排字符串（标准间距：汉字 12 列 + ASCII 固定 8 列）
 * @param  col  起始列 0~127
 * @param  page 起始逻辑页 0~7
 * @param  puts 字符串首地址（UTF-8 或 GB2312），以 '\0' 结束，不允许为 NULL
 * @retval 无
 */
void LCD_ShowStrEx(uint8_t col, uint8_t page, uint8_t *puts)
{
    /* 与 LCD_ShowStrExCompact() 同一套逻辑，只有 ASCII 分支不同：这里用 LCD_ShowChar 画满 6 列并按
     * 固定 8 列推进，码位整齐、列位可预期，适合需要与其它元素对齐的表格化排版
     * ⚠ 覆盖式绘制会与旧内容叠加；查不到字模（0xFF）时只跳字节不画字 */
>>>>>>> origin/develop
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
<<<<<<< HEAD
                LCD_WR_REG(ComTable[page & 0x07] | 0xB0);
                LCD_WR_REG(((col) >> 4) | 0x10);
                LCD_WR_REG(col & 0x0F);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
                // 第2页
                LCD_WR_REG(ComTable[(page + 1) & 0x07] | 0xB0);
                LCD_WR_REG(((col) >> 4) | 0x10);
                LCD_WR_REG(col & 0x0F);
=======
                LCD_SetPageCol(page & 0x07, col, 0);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
                // 第2页
                LCD_SetPageCol((page + 1) & 0x07, col, 0);
>>>>>>> origin/develop
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

<<<<<<< HEAD
// 显示字符串(ASCII only)
void LCD_ShowStr(uint8_t col, uint8_t page, uint8_t *puts)
{
=======
/**
 * @brief  显示纯 ASCII 字符串（8x16 字模，固定 8 列推进）
 * @param  col  起始列 0~127；实际首字符从 col+3 开始画
 * @param  page 起始逻辑页 0~7（每行占 2 页）
 * @param  puts 纯 ASCII 字符串首地址，以 '\0' 结束，不允许为 NULL
 * @retval 无
 */
void LCD_ShowStr(uint8_t col, uint8_t page, uint8_t *puts)
{
    /* 换行：col > 120（LCD_W-8）时 page += 2、col 归 0；page > 6 时回卷到 (0,0) 继续画
     * 每字符先偏移 3 列后绘制（LCD_ShowChar(col + 3, ...)）再按 col += 8 推进 —— 本工程沿用的对齐习惯
     * ⚠ 只支持 ASCII：传入汉字会按字节当 ASCII 处理，显示为乱码 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
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
=======
/**
 * @brief  显示一张 128x64 的单色位图（整屏覆盖）
 * @param  puts 位图数据首地址，必须 ≥1024 字节；传 0 直接返回
 * @retval 无
 */
void LCD_ShowBmp(uint8_t const *puts)
{
    /* 数据排布：连续 8 页 × 128 列，每字节 bit7 在上（oledfont.h 的 bmp1[]/bmp3[] 即此格式）
     * ⚠ 每页起始列固定写 0x10 + 0x04（而非 0x00）：本屏列地址起点有偏移，与 OLED_Set_Pos 的 +1 补偿
     *   是同一套硬件补偿，不要随意改动；页地址经 ComTable[] 映射，图不会上下颠倒 */
    if (puts == 0) return;   /* 空指针保护 */
>>>>>>> origin/develop
    uint8_t i, j;
    uint16_t X = 0;
    for (i = 0; i < (LCD_H / 8); i++)
    {
        LCD_WR_REG(0xB0 | ComTable[i]); // Set Page Address
<<<<<<< HEAD
        LCD_WR_REG(0x10);               // Set Column Address = 0
        LCD_WR_REG(0x04);               // Colum from S1 -> S128 auto add             //
=======
        LCD_WR_REG(0x10);               // 列地址高 4 位 = 0
        /* 列地址起点：正装从 RAM 列 4 起（把 SEG4 当可见列第 0 列）；旋转 180° 后 SEG 扫描反向
         * （0xA1），起点要镜像成 0，否则整幅图会被推到右侧不可见的 4 列上去（口径同 LCD_SetPageCol） */
#if LCD_SCAN_ROT180
        LCD_WR_REG(0x00);               // 倒装：列地址 = 0
#else
        LCD_WR_REG(0x04);               // 正装：列地址 = 4 (Colum from S1 -> S128 auto add)
#endif
>>>>>>> origin/develop
        for (j = 0; j < LCD_W; j++)
        {
            LCD_WR_DATA8(puts[X]);
            X++;
        }
    }
}

<<<<<<< HEAD
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
=======
/**
 * @brief  设置 OLED 显存的写入地址（页地址 + 列地址），后续写数据从该点开始
 * @param  col  用户坐标系列 0~127（内部按 +1 补偿后下发）
 * @param  page 逻辑页 0~7（内部对 8 取模）
 * @retval 无
 */
void OLED_Set_Pos(uint8_t col, uint8_t page)
{
    // 这里的 +3 和 ComTable 必须保留，因为这是你屏幕正常的物理映射
    /* ⚠ real_col = col + 3 计算后未被使用，实际只补偿 1 列；
     *   与直接写 ComTable[page]|0xB0 的 LCD_ShowChar 系列横向相差若干列，混用时须以实测对齐为准 */
    uint8_t real_col = col + 3;
    LCD_SetPageCol(page & 0x07, col, 1);
}

/**
 * @brief  用小号字模（6x8）显示字符串，只有 1 页高
 * @param  col  起始列 0~127
 * @param  page 逻辑页 0~7（单页高度）
 * @param  str  字符串首地址，以 '\0' 结束，不允许为 NULL
 * @retval 无
 */
void LCD_ShowStr_Small(uint8_t col, uint8_t page, const char *str)
{
    /* 字模取自 oledfont.h 的 F6x8[][6]（下标 = 字符 - 0x20），每字符写 6 字节
     * ⚠ F6x8 只有数字 0~9、若干符号和 d/m/n 有字模，其它表项是空 {}，传字母显示为空白（不是乱码）
     * ⚠ 固定 6 列推进、不做换行判断，超过 128-col 会折回屏幕左侧 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
// 计算6x8字符的有效宽度（去除左右空白）
// 返回值：有效宽度（列数），left表示左边空白列数
static uint8_t LCD_GetSmallCharWidth(uint8_t ch, uint8_t *left)
{
=======
/**
 * @brief  测量 6x8 小字模的实际墨迹宽度，并回传左侧空白列宽
 * @param  ch   字符码（' '(0x20) 及以上）
 * @param  left 输出：左侧空白列数（0~5），整字空白时为 0，不允许为 NULL
 * @retval 字模有效宽度（1~6）；空白字符返回 0
 */
static uint8_t LCD_GetSmallCharWidth(uint8_t ch, uint8_t *left)
{
    /* 与 LCD_GetCharWidth() 同理，只是字模只有一页 6 列、不分上下半部 */
>>>>>>> origin/develop
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
<<<<<<< HEAD
        return 0; // 空白字符
=======
        return 0;
>>>>>>> origin/develop
    }
    
    *left = first_col;
    return last_col - first_col + 1;
}

<<<<<<< HEAD
// 紧凑显示单个6x8字符（只绘制有效宽度，字母间空一列）
static uint8_t LCD_ShowSmallCharCompact(uint8_t col, uint8_t page, uint8_t ch)
{
=======
/**
 * @brief  紧凑显示 1 个 6x8 字符：只画墨迹，右侧保留 1 列间距
 * @param  col  起始列 0~127（内部经 OLED_Set_Pos 的列补偿）
 * @param  page 逻辑页 0~7
 * @param  ch   字符码 0x20 及以上；<0x20 会造成字模负下标越界
 * @retval 占用列数 = 有效宽度 + 1；空白字符返回 3
 */
static uint8_t LCD_ShowSmallCharCompact(uint8_t col, uint8_t page, uint8_t ch)
{
    /* ⚠ 与 LCD_ShowStr_Small 一样，F6x8 里字母大多为空表项，传字母会画不出东西 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
// 紧凑显示小字体字符串（6x8）
void LCD_ShowStr_Small_Compact(uint8_t col, uint8_t page, const char *str)
{
=======
/**
 * @brief  紧凑显示 6x8 小字字符串（逐字符累加实际宽度推进列）
 * @param  col  起始列 0~127
 * @param  page 逻辑页 0~7（单页高）
 * @param  str  字符串首地址，以 '\0' 结束，不允许为 NULL
 * @retval 无
 */
void LCD_ShowStr_Small_Compact(uint8_t col, uint8_t page, const char *str)
{
    /* 只占传入的那 1 页，不做换行/回卷，超出 128 列折回左侧；曲线视图的刻度文字用它绘制 */
>>>>>>> origin/develop
    while (*str != '\0')
    {
        col += LCD_ShowSmallCharCompact(col, page, *str);
        str++;
    }
}

<<<<<<< HEAD
// 计算紧凑显示后的字符串宽度（用于箭头位置计算）
static uint8_t LCD_GetStrCompactWidth(const char *str)
{
=======
/**
 * @brief  预计算字符串按紧凑排版绘制后的总像素宽度
 * @param  str 待测量字符串，以 '\0' 结束
 * @retval 总宽度（列数，0~255）；str 为 NULL 返回 0
 */
static uint8_t LCD_GetStrCompactWidth(const char *str)
{
    /* 专供 LCD_ShowArrowEx(mode=3) 算箭头落点：汉字 12 列、空格 3 列、数字 '0'~'9' 固定 9 列、
     * 其它可打印 ASCII 扫描字模求有效宽度 + 1 列间距
     * ⚠ 数字按满宽 9 列计而实际墨迹可能更窄，算出位置相对数字串末尾会偏右（已知偏差）；
     *   宽度超过 255 时按 uint8_t 回绕 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
// 智能箭头显示函数
// mode: 0=右对齐(默认), 1=左对齐(文本前), 2=指定位置, 3==自动计算字符串宽度后显示
// col: 当mode=2/3时为文本起始列
// str: 当mode=3时为要计算宽度的字符串
void LCD_ShowArrowEx(uint8_t page, uint8_t mode, uint8_t col, const char *str)
{
=======
/**
 * @brief  在指定页画一个 ">" 箭头，位置由 mode 决定
 * @param  page 逻辑页 0~7（箭头上半部所在页，下半部在 page+1）
 * @param  mode 定位方式：0=右对齐 col=118、1=左对齐 col=0、2=用传入 col、3=紧凑文本右端；其它按 0
 * @param  col  mode=3 时为文本起始列、mode=2 时为箭头列；mode=0/1 忽略
 * @param  str  仅 mode=3 用于测宽的字符串，其它 mode 可传 NULL
 * @retval 无
 */
void LCD_ShowArrowEx(uint8_t page, uint8_t mode, uint8_t col, const char *str)
{
    /* 箭头复用 LCD_ShowStr 绘制字符 '>'，故继承其 +3 列水平偏移与行末/屏末换行回卷；
     * ⚠ oled.h 中 mode 3/4 的描述有误，以本文件实现为准 */
>>>>>>> origin/develop
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

<<<<<<< HEAD
// 内部安全绘图宏：负责自动处理跨页映射，杜绝内存越界
=======
/* 在私有显存 frame_buffer 上"置 1"一个像素（只置位、不清零，叠加式描点）。
 * 换算：页 = y/8、页内位 = 7-(y%8)；越界（x 超出 0~127、y 超出 0~63）静默丢弃。
 * 用 do{}while(0) 包裹，保证在 if/else 中当单条语句使用不会出错。 */
>>>>>>> origin/develop
#define DRAW_PIXEL(x, y) do { \
    if ((x) >= 0 && (x) < 128 && (y) >= 0 && (y) < 64) { \
        frame_buffer[(y) / 8][(x)] |= (1 << (7 - ((y) % 8))); \
    } \
} while(0)

<<<<<<< HEAD
// 注意：为了STM32F1的性能，x_scale 和 y_scale 建议传入整型(uint8_t)
void LCD_DrawEchoCurve(uint8_t *echo_data, uint8_t x_scale, uint8_t y_scale)
{
    uint8_t i, j;
    
    // --- 坐标系基准参数 ---
    const int AXIS_X = 8;    // Y轴的水平位置 (X坐标)
    const int AXIS_Y = 55;   // X轴的垂直位置 (Y坐标)
=======
/* X 轴右端刻度文字：由上层 UI 按当前量程写入，默认 "8.91" 兼容旧行为。
 * 只做字符串拷贝、不引入浮点格式化；实际绘制在 LCD_DrawEchoCurve() 末尾随刻度一起写出。
 * ⚠ 下行 ECHO 是主板归一化到 0~255 的【128 点】包络，既不是 ADC 原始 12-bit 幅值、
 *   也不是 4096/5120 个采样点，横轴刻度单位仍是米。 */
static char s_curve_x_end[8] = "8.91";

/**
 * @brief  设置回波曲线 X 轴右端刻度文字
 * @param  label 刻度文字（≤7 字符）；传 0 忽略
 * @retval 无
 */
void LCD_SetCurveXEndLabel(const char *label)
{
    if (label == 0) return;
    strncpy(s_curve_x_end, label, sizeof(s_curve_x_end) - 1u);
    s_curve_x_end[sizeof(s_curve_x_end) - 1u] = '\0';
}

/**
 * @brief  绘制诊断回波曲线：坐标轴 + 刻度 + 折线 + 坐标文字，并整屏刷入
 * @param  echo_data 回波数据（128 字节）；传 NULL 只画坐标轴与刻度
 * @param  x_scale   X 方向抽样步长（每列取第几个点），建议 1~8；0 会钳到 1
 * @param  y_scale   Y 方向放大倍数，建议 1~8；0 会钳到 1
 * @retval 无
 */
// 注意：为了STM32F1的性能，x_scale 和 y_scale 建议传入整型(uint8_t)
void LCD_DrawEchoCurve(uint8_t *echo_data, uint8_t x_scale, uint8_t y_scale)
{
    /* ⚠ 唯一使用私有显存 frame_buffer 的函数：开头整块 memset 清零 → 在 frame_buffer 里离屏画完
     *   → 最后按 8 页 × 128 列一次性刷入 GDDRAM（这一步才真正上屏）→ 再用 6x8 小字写三个刻度
     * ⚠ 会覆盖整屏像素，调用顺序必须排在同屏其它绘制之前；y_scale=1 时 0~255 完整映射到 40px 绘图区 */
    uint8_t i, j;

    /* 缩放因子钳位：避免 0 导致曲线退化或除零 */
    if (x_scale < 1u) x_scale = 1u;
    if (y_scale < 1u) y_scale = 1u;

    // --- 坐标系基准参数 ---
    const int AXIS_X = 8;    // Y轴的水平位置 (X坐标)
    const int AXIS_Y = 55;   // X轴的垂直位置 (Y坐标)
    /* 曲线绘图区：顶部 15 px 留给标题（LCD_ShowStrExCompact(0,0,...) 与右上角页码），
     * 底部到 X 轴 55 ⇒ 净高 40 px。0~255 归一化数据按此高度完整映射，不削顶。 */
    const int CURVE_TOP_Y  = 15;
    const int CURVE_PLOT_H = AXIS_Y - CURVE_TOP_Y;
>>>>>>> origin/develop

    
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
            
<<<<<<< HEAD
            // 纯整数计算 Y 坐标 (将原本的 * 0.20f 转换为等价的 * 1 / 5，极大节省CPU开销)
            int y = AXIS_Y - ((echo_data[data_idx] * y_scale * 1) / 5);     //最大在230左右的数据正常显示
            
            // --- 核心边界保护 ---
            if (y < 15) y = 15; // 最高不能冲破 Y=15
=======
            /* 纵向定标：按 CURVE_PLOT_H/255 映射，255 恰好落在 y=CURVE_TOP_Y（标题下沿）。
             * ⚠ 不可写成 /5 或 /3 的固定除数：那会把 0~255 铺成 0~51/0~85 px，超出 40 px 的部分
             *   全部被钳位 ⇒ 强回波帧（峰值接近 255）几乎每帧削顶 */
            int y = AXIS_Y - ((int)echo_data[data_idx] * (int)y_scale * CURVE_PLOT_H / 255);

            // --- 核心边界保护 ---
            if (y < CURVE_TOP_Y) y = CURVE_TOP_Y;   // 最高不能冲破标题下沿
>>>>>>> origin/develop
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
<<<<<<< HEAD
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
=======
>>>>>>> origin/develop
    
    // 8. 按照 AiP31567 驱动特性刷入硬件显存
    for (i = 0; i < 8; i++) {
        LCD_WR_REG(ComTable[i] | 0xB0); // Page 地址
<<<<<<< HEAD
        LCD_WR_REG(0x10);               // 列地址高位
        LCD_WR_REG(0x00);               // 列地址低位
=======
        LCD_WR_REG(0x10);               // 列地址高 4 位 = 0
        /* 列地址起点：正装写 RAM 列 0~127（对应可见列 0~127）；旋转 180° 后 SEG 扫描反向（0xA1），
         * 起点镜像成 4（ram_col = 4 对应可见列第 127 列），否则整屏会右移 4 列并露边（口径同 LCD_SetPageCol） */
#if LCD_SCAN_ROT180
        LCD_WR_REG(0x04);               // 倒装：列地址 = 4
#else
        LCD_WR_REG(0x00);               // 正装：列地址 = 0
#endif
>>>>>>> origin/develop
        for (j = 0; j < 128; j++) {
            LCD_WR_DATA8(frame_buffer[i][j]);
        }
    }

    // 9. 绘制文字
    LCD_ShowStr_Small_Compact(11, 7, "0.01");
    LCD_ShowStr_Small_Compact(55, 7, "m(d)");
<<<<<<< HEAD
    LCD_ShowStr_Small_Compact(98, 7, "8.91");
}

void LCD_ShowCharReverse(uint8_t col, uint8_t page, uint8_t Order)
{
    uint8_t i;
    uint8_t ch = Order - 0x20;                // ASCII字符从0x20开始
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0); // Set Page Address
    LCD_WR_REG(((col + 1) >> 4) | 0x10);      // Set Column Address High Byte
    LCD_WR_REG(col & 0x0F);                   // Low Byte Column Address
=======
    /* 右端刻度 = 当前量程（由 LCD_SetCurveXEndLabel 设定）。
     * 紧凑小字按有效宽度绘制，长度 >4 时向左补偿，避免顶到屏右边缘。 */
    {
        size_t n = strlen(s_curve_x_end);
        uint8_t col = 98u;
        if (n > 4u) col = (uint8_t)(98u - (n - 4u) * 5u);
        LCD_ShowStr_Small_Compact(col, 7, s_curve_x_end);
    }
}

/**
 * @brief  反白显示 1 个 ASCII 字符（字模取反，黑底白字）
 * @param  col   起始列 0~127（含 +1 列补偿）
 * @param  page  上半部逻辑页 0~7；下半部在 page+1（page=7 时回绕）
 * @param  Order ASCII 码 0x20~0x7E，内部减 0x20 取字模下标
 * @retval 无
 */
void LCD_ShowCharReverse(uint8_t col, uint8_t page, uint8_t Order)
{
    /* 与 LCD_ShowChar 的唯一区别是写入前按位取反
     * ⚠ 只做整字节取反、不做背景填充 ⇒ 同一位置重复调用两次会恢复原样；
     *   要干净的反相白块须先用 0xFF 铺底。⚠ 列地址含 +1 补偿，与 LCD_ShowChar 横向相差 1 列 */
    uint8_t i;
    uint8_t ch = Order - 0x20;                // ASCII字符从0x20开始
    LCD_SetPageCol(page & 0x07, col, 1);
>>>>>>> origin/develop
    for (i = 0; i < 6; i++)                   // 上半部分6列
    {
        LCD_WR_DATA8(~(ASCIIchardot[ch][i]));
    }
    page++;                                   // 下半字符page+1
<<<<<<< HEAD
    LCD_WR_REG(ComTable[page & 0x07] | 0xB0); // Set Page Address
    LCD_WR_REG(((col + 1) >> 4) | 0x10);      // Set Column Address High Byte
    LCD_WR_REG(col & 0x0F);                   // Low Byte Column Address
=======
    LCD_SetPageCol(page & 0x07, col, 1);
>>>>>>> origin/develop
    for (i = 6; i < 12; i++)                  // 下半部分6列
    {
        LCD_WR_DATA8(~(ASCIIchardot[ch][i]));
    }
    page--; // 写完一个字符page还原
<<<<<<< HEAD
}
=======
}
>>>>>>> origin/develop
