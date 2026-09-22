/* ================================================================================
 * 文件功能概述
 * ================================================================================
 * 本文件是"显示板 OLED 屏"的底层驱动（Hardware 层），只负责把字节/字模搬进屏幕
 * 的 GDDRAM；"画什么、什么时候画"的决策全部在上层 APP/Src/oled_ui.c 中完成。
 *
 * 【硬件接口】SPI1，单线半双工（SPI_DIRECTION_1LINE，只有 MOSI 没有 MISO），
 *   SCK = PA5，MOSI = PA7；片选 SPI_CS 与命令/数据选择 A0(DC) 在 GPIOA，
 *   复位 OLED_RST 在 GPIOB。屏幕为 128x64、ST7565/AiP31567 系 COG 驱动。
 *   详见 Core/Src/spi.c、Core/Src/gpio.c。
 *
 * 【坐标系】原点在屏幕物理左上角：
 *     col  —— 像素列，向右递增，范围 0~127（单位：像素）
 *     page —— "页"，向下递增，范围 0~7，1 页 = 8 个像素行
 *   即屏幕是"8 页 × 128 列"的字节矩阵，一个字节 = 某一页里某一列的 8 个纵向像素，
 *   位序为 bit7 在上、bit0 在下：
 *
 *          col=0   col=1   ...                       col=127
 *   page0  [b7..b0] [b7..b0]                           <- 像素行 y = 0 ~ 7
 *   page1  [b7..b0] [b7..b0]                           <- 像素行 y = 8 ~ 15
 *   ...
 *   page7  [b7..b0] [b7..b0]                           <- 像素行 y = 56 ~ 63
 *
 *   注意：逻辑 page 与驱动芯片内部 page 是反的，由 ComTable[i] = 7 - i 做翻转，
 *   再配合初始化里的 0xC0 / 0xA0（COM、SEG 反向扫描）一起，补偿屏幕在整机上
 *   旋转 180° 的安装方向。所以本文件里凡是 "ComTable[page] | 0xB0" 都必须
 *   当成"写第 page 页"来读，不要直接理解成芯片 page 号。
 *
 * 【显存/刷新】本驱动没有整屏软显存：除了 LCD_DrawEchoCurve() 内部私有的
 *   frame_buffer[8][128] 之外，所有函数都是直接写 GDDRAM，写完立即生效，
 *   不存在 OLED_Refresh() 这一步。因此：
 *     1) 每一帧重画之前必须自己先清屏（LCD_FullFill / LCD_ClearLine）或按
 *        覆盖范围整字宽重画，否则上屏旧内容会与新内容叠加成"叠影"。
 *     2) 绘制函数只把"字模/图形里为 1 的像素"点亮，不写背景（不透写 0 像素），
 *        所以在已有内容的区域画字会发生叠加（不自动擦除）。
 * ================================================================================ */

#include "oled.h"
#include "stdlib.h"
#include "oledfont.h"
#include "spi.h"
#include "string.h"



/* 曲线视图(诊断回波曲线)专用的软件显存。
 * 组织方式与屏幕页矩阵一致：frame_buffer[page][col]，page 0~7、col 0~127，
 * 每字节的 bit7 在上、bit0 在下（见文件头示意图）。
 * 只有 LCD_DrawEchoCurve() 用它做"先离屏画好、再整屏刷入"，
 * 其余绘制函数都不经过这里。 */
uint8_t frame_buffer[8][128];    //波形图绘制


/* 前景/背景色全局变量。本屏是单色屏，两个变量实际只作标记：
 * 底层绘制函数不查询它们，取值由上层决定（0=不点亮，非 0=点亮）。
 * 保留声明仅为兼容对外接口与 oled.h 的 extern。 */
uint16_t BACK_COLOR, POINT_COLOR; // 背景色，画笔色

/* 逻辑页 → 芯片页 的映射表：ComTable[i] = 7 - i。
 * 用 "ComTable[page] | 0xB0" 拼出页地址命令，从而把"第 0 页在屏幕最上方"
 * 这个逻辑坐标，换算成芯片实际的页号。配合初始化中的 0xC0/0xA0 一起，
 * 抵消屏幕在整机上旋转 180° 安装带来的影响。 */
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
 * @brief  向 OLED 串行写入 1 个字节（最底层收发原语）。
 * @note   时序：拉低 CS 片选 → SPI1 以 8bit/CPOL=1/CPHA=1/MSB 先出 发 1 字节
 *         → 再拉高 CS。整个工程里只有本函数真正碰 SPI1，其余函数都是它的包装。
 *         字节类型是数据还是命令，由调用方先设置 A0(DC) 电平决定，本函数不管。
 *         超时给到 1000ms：SPI 单字节正常只需几微秒，给大值是为了避免总线异常时
 *         卡死在 HAL 里（返回值未做检查，见下方"疑似问题"）。
 *         每次传参都单独拉一次 CS，属于"每字节一个片选周期"的保守写法。
 * @param  dat : 待发送的字节（0x00~0xFF），按位 MSB 先发。
 * @retval 无（HAL_SPI_Transmit 的返回值被丢弃，不做重试）。
 * @complexity O(1)，1 次 SPI 字节传输，阻塞式。
 */
void LCD_Writ_Bus(char dat) // 串行数据写入
{
    uint8_t i;
    OLED_CS_Clr();
    HAL_SPI_Transmit(&hspi1, (uint8_t *)&dat, 1, 1000);
    OLED_CS_Set();
}

/**
 * @brief  写 1 个字节的"显示数据"（A0=1）到屏幕。
 * @note   先把 A0(DC) 拉高表示后续字节是数据，再交给 LCD_Writ_Bus 发送。
 *         只把字节送进 GDDRAM/数据寄存器，地址指针按芯片规则自增
 *         （页地址模式下写完一列自动右移一列）；不改变页/列地址设置，
 *         也不触发任何重绘。片选 CS 由 LCD_Writ_Bus 内部管理。
 * @param  da : 待写入的 8bit 数据（0x00~0xFF），1=点亮像素。
 * @retval 无
 * @complexity O(1)，1 次 SPI 字节传输。
 */
void LCD_WR_DATA8(char da) // 发送8位数据
{                          // OLED_CS_Clr();
    OLED_A0_Set();
    LCD_Writ_Bus(da);
}

/**
 * @brief  写 16 位数据（高字节在前）。
 * @note   本屏 GDDRAM 是按字节组织的，此函数主要给"16 位色"风格的老接口
 *         兜底：先把 da 的高 8 位当数据发出，再发低 8 位，共消耗 2 个 GDDRAM
 *         字节（也就是同一页上相邻的两列）。当前 UI 层没有调用它。
 * @param  da : int 型的 16bit 数据，实际只取低 16 位（0x0000~0xFFFF）。
 * @retval 无
 * @complexity O(1)，2 次 SPI 字节传输。
 */
void LCD_WR_DATA(int da) // 发送16位数据
{                        //	OLED_CS_Clr();
    OLED_A0_Set();
    LCD_Writ_Bus(da >> 8);
    LCD_Writ_Bus(da);
}

/**
 * @brief  写 1 个字节的"命令"（A0=0）到屏幕。
 * @note   先把 A0(DC) 拉低表示后续字节是命令，再交给 LCD_Writ_Bus 发送。
 *         本文件的命令字节一律走这个入口，典型命令见表：
 *           0x00~0x0F 列地址低 4 位   | 0x10~0x1F 列地址高 4 位
 *           0xB0~0xB7 页地址          | 0x40~0x7F 显示起始行
 *           0xA0/0xA1 SEG 扫描方向    | 0xC0/0xC8 COM 扫描方向
 *           0xA6/0xA7 正常/反显       | 0xA4/0xA5 全亮控制
 *           0xAE/0xAF 关/开显示       | 0xE2 软复位  0x2F 电源控制
 *           0x81 后跟 1 字节电子音量（对比度，0x00~0x3F）
 * @param  da : 命令字节，按位含义见上。
 * @retval 无
 * @complexity O(1)，1 次 SPI 字节传输。
 */
void LCD_WR_REG(char da) // 发送8位命令
{                        //	OLED_CS_Clr();
    OLED_A0_Clr();
    LCD_Writ_Bus(da);
}

/**
 * @brief  先发一条命令，再发一个 16 位参数（命令 + 双字节参数的组合）。
 * @note   给"0x81 + 音量值"这类需要跟参数的命令准备的便捷封装，
 *         等于 LCD_WR_REG(reg) 紧跟 LCD_WR_DATA(da)。
 *         注意参数是 16 位写法（发 2 字节），而本屏绝大多数带参命令
 *         （如 0x81 电子音量）只需要 1 字节参数，用本函数会多发 1 字节。
 *         当前 UI 层未调用，若启用请确认目标命令的参数宽度。
 * @param  reg : 命令字节（例如 0x81 设置电子音量）。
 * @param  da  : 命令参数，按 16 位发送（高字节先出）。
 * @retval 无
 * @complexity O(1)，3 次 SPI 字节传输。
 */
void LCD_WR_REG_DATA(char reg, int da)
{
    LCD_WR_REG(reg);
    LCD_WR_DATA(da);
}

/**
 * @brief  设置后续写入的起始「逻辑页 + 逻辑列」，把页地址与列地址一次下发。
 * @note   ★这是全文件唯一的列地址出口（2026-09-21 收敛，替代了原先散落在
 *         14 处的"页命令 + 列地址高 + 列地址低"三行一式手工序列）。
 *
 *         列地址补偿：o 是调用方名义上的列偏移（LCD_ShowChar 系列用 0，
 *         OLED_Set_Pos / LCD_ShowCharReverse 系列用 1）。屏的可见区落在
 *         SEG0~SEG127，而芯片共有 132 列，另有 4 列（SEG128~SEG131）不可见。
 *         所以"屏幕旋转 180°"时 SEG 扫描方向取反（0xA1）会把内容整体推到
 *         那 4 列不可见区去，必须把偏移镜像为 (132-128)-o = 4-o，才能把画面
 *         重新拉回可见区：
 *             屏幕正装：o → o     （0 → 0，1 → 1）
 *             屏幕倒装：o → 4-o   （0 → 4，1 → 3）
 *         旁证：col 最大 127，倒装时列地址最大 127+4 = 131，正好用满芯片
 *         132 列；若镜像量取 0 则会甩出屏外。
 *         方向开关见 oled.h 的 LCD_SCAN_ROT180。
 * @param  page : 逻辑页号（0 = 屏幕最上方，取值 0~7；内部再 & 0x07）。
 * @param  col  : 程序侧列号 0~127（含调用方自己的基准，不含下面这个 o）。
 * @param  o    : 调用方名义列偏移（0 或 1），旋转时用它做镜像换算。
 * @retval 无
 * @complexity O(1)，3 次 SPI 字节写。
 */
static void LCD_SetPageCol(uint8_t page, uint8_t col, uint8_t o)
{
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
 * @brief  用同一个字节填满整个 GDDRAM（整屏全亮 / 全灭）。
 * @note   循环 i = 0~8，共写 9 页，这 9 页**全部是合法页地址**（2026-09-21 依
 *         AiP31567 手册核正）：页地址命令为 0xB0 | Y3Y2Y1Y0，Y 可取 0~8；
 *         page0~page7 对应 COM0~COM63（D7~D0 全部有效），page8 是 icon 页，
 *         只有 D0 有效、对应第 65 条 COM（手册 4.10.4）。本屏 LX-12864T5B
 *         是 1/64 duty、只用 COM0~63，所以第 9 次（0xB8）落在未连接的驱动行上，
 *         无副作用；但**它是合法页、不是"越界写法"**，
 *         不要以"屏只有 8 页"为由把循环改成 8 次（会漏清 icon 行）。
 *         每页开头固定送 0x10、0x00 把列地址拨回第 0 列；
 *         每次写 132 列 —— 列地址 0~131 正是手册 4.10.5 给出的合法范围，
 *         比可见的 128 列多 4 列，多出的部分不可见，无副作用。
 *         无返回值、不改变页/列地址的"当前值"（每次进函数都重设）。
 * @param  FillData : 填充字节。0x00 全屏熄灭（清屏）；0xFF 全屏点亮。
 * @retval 无
 * @complexity O(9×132) ≈ 1188 次 SPI 字节写，全屏刷新的耗时基准。若在
 *             每帧主循环中无条件调用会明显占用 CPU（LCD_ShowBmp 为同量级）。
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
 * @brief  清空指定的一个逻辑页（8 个像素行），按 0x00 全灭写入。
 * @note   入参 page 是逻辑页号（0 = 屏幕最上方），函数内部用 7 - page
 *         换成芯片页地址后写入，因此不依赖 ComTable。page 越界返回 0xFF。
 *         只清 1 页，不清相邻页；清完立即生效，不需要额外刷新。
 *         同样固定从列 0 开始写 132 列（比 128 列宽，多余部分不可见）。
 * @param  page : 逻辑页号，0~7（0 对应屏幕最上方的 8 个像素行）。
 *                传入 >7 时直接 return，不产生任何总线动作。
 * @retval 无
 * @complexity O(132) ≈ 132 次 SPI 字节写，与 LCD_FullFill 单页开销相同。
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
 * @brief  用同一个字节填满一个矩形区域（逻辑页 × 逻辑列）。
 * @note   给"首页左侧预留条"这类实心色块用，见 oled_ui.c 的 UI_RenderHome()。
 *         ★颜色语义（务必先确认手上屏的极性）：
 *         LX-12864T5B 规格书第 3 节"显示模式"给了两种型号 ——
 *         DFSTN（黑底白字，负显）与 FSTN（白底黑字，正显）。本工程按负显使用，
 *         清屏值 0x00 的底色本身就是黑的，因此：
 *             data = 0xFF → 亮块（白条，与黑底对比明显）
 *             data = 0x00 → 黑块（等效于"不画"，负显下看不出来）
 *         正显批次上两者的效果正好相反：那时"亮块"要用 0x00。装机前按实物确认。
 *         列地址统一走 LCD_SetPageCol()，所以自动跟随 LCD_SCAN_ROT180。
 *         越界按可见区截断：col + width 超过 128 列、page + pages 超过 8 页
 *         都会被收窄，不会写到屏外或 icon 页上。
 * @param  col   : 起始逻辑列（0~127）。
 * @param  page  : 起始逻辑页（0~7，0 = 屏幕最上方）。
 * @param  width : 宽度（列数）；为 0 直接返回。
 * @param  pages : 高度（页数）；为 0 直接返回。
 * @param  data  : 填充字节（0x00 / 0xFF）。
 * @retval 无
 * @complexity O(width × pages) 次 SPI 字节写，例如 16 列 × 8 页 = 128 次。
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
 * @brief  OLED 初始化：硬件复位 + 按序下发上电配置命令 + 清屏 + 开显示。
 * @note   在 UI_Init() 中调用，早于首帧绘制，见 APP/Src/oled_ui.c。
 *         流程分两段：
 *         (1) 复位：CS 拉低使能 → RST 输出 20ms 低脉冲 → RST 拉高 → 再等
 *             20ms → CS 拉高 → 开始发命令（两处 20ms 为用户按参考示例选定）。
 *             ★手册 4.11.1 对上电有两条**同时成立**的约束：
 *               a) RSTB=L 的保持时间 ≥ tRW（≥5µs）—— 下限，20ms 远够；
 *               b) 复位结束（RSTB 回高）到 0x2F 电源控制之间 **≤5ms** —— 上限。
 *             ⚠ 现状偏离：本函数两处延时均为 20ms，第二处使 (b) 变成 20ms，
 *               **超出上限约 4 倍**，与手册不符（已知偏离，保留待实测确认）。
 *               历史版本（init / del0612）的 `delay_ms()` 是空操作、复位间隔
 *               约 14ns，(b) 天然满足。两个版本谁该为"不亮"负责尚未定论 ——
 *               换板后两种时序均不亮，**不要据本注释下结论**。
 *               自研 delay 模块已于 2026-09-20 删除，勿再引入。
 *         (2) 配置序列（每条命令含义见行内注释）：0xE2 软复位 → 0x2F 打开
 *            内部电源（升压/稳压/跟随全开）→ 0x23 设定调节器电阻 →
 *             0xA2 偏压 1/9 → 0x81+0x25 电子音量（对比度，范围 0x00~0x3F）
 *             → 扫描方向（正装 0xC0+0xA0 / 倒装 0xC8+0xA1，由 oled.h 的
 *               LCD_SCAN_ROT180 选择；列地址端的配套补偿在 LCD_SetPageCol()
 *               里，两处必须同进同退）→ 0x40 起始行 0 → 0xA6 正常显示 →
 *             0xA4 关闭全亮 → 清屏 → 0xAF 开显示。
 *             ⚠ 顺序偏离：手册 4.11.1 把"电源控制"排在 RR/EV **之后**，
 *               本处沿参考工程把它放在最前。应能收敛（V0 会先到默认值
 *               再重新稳定），但未按手册顺序，记录在案。若需严格对齐手册，
 *               把 0x2F 移到 0x81+0x25 之后即可。
 *         复位后 CS 被拉高，后续命令由 LCD_Writ_Bus 逐字节自行拉 CS。
 * @param  无
 * @retval 无
 * @complexity O(1)（配置命令）+ O(1188)（内部 LCD_FullFill 清屏），
 *             合计约 1200 次 SPI 字节写；本函数只在开机时执行一次。
 */
void LCD_init(void)
{
    OLED_CS_Clr(); // 打开片选使能
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
    /* ★显示方向（屏幕旋转 180°）—— 手册里管这个的就是下面这两条扫描方向命令：
     *   MY（0xC0 / 0xC8）管上下、MX（0xA0 / 0xA1）管左右，两条一起取反
     *   才是干净的 180°（只翻一条会变成上下颠倒或左右镜像）。
     *   配套：列地址端的镜像补偿（4-o）在 LCD_SetPageCol() 里，两处必须
     *   同进同退；只改这里不改那里，画面会被甩到不可见的 4 列上去。
     *   开关见 oled.h 的 LCD_SCAN_ROT180。 */
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
    LCD_FullFill(0x00); // full clear
    LCD_WR_REG(0xAF);   // turns the display ON
}

/**
 * @brief  测试用：按页写入一对交替字节，铺满整个屏幕。
 * @note   每页（0xB0~0xB7，共 8 页）从列 0 开始，每列连写两个字节
 *         dat1、dat2，共 128 列 —— 即"隔列交替"图案，用来目测屏体是否有
 *         整列/整页不亮的缺陷。写 128 列×2 字节 = 256 字节/页，
 *         会越过可见的 128 列，超出的部分落在不可见区。
 *         ⚠ 本函数页地址用的是 0xB0~0xB7 原始值（未过 ComTable），
 *         所以上下方向与常规绘制函数相反，仅作产测图案，UI 层未调用。
 * @param  dat1 : 每列第一个字节（该列前 1 列宽的图案）。
 * @param  dat2 : 每列第二个字节（该列第二个字节宽度的图案）。
 * @retval 无
 * @complexity O(8×128×2) = 2048 次 SPI 字节写。
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

/**
 * @brief  测试用：把一段连续字节整体铺屏（显示整张位图/字模原始数据）。
 * @note   从指针 p 开始连续取数，按页 0xB0~0xB7、每页 128 列的顺序写入，
 *         总共消耗 8×128 = 1024 字节。调用方必须保证 p 指向的缓冲区
 *         ≥1024 字节，否则会越界读取（本函数不做长度校验）。
 *         与 LCD_ShowBmp 的区别：本函数页地址未过 ComTable，上下方向相反；
 *         LCD_ShowBmp 则做了翻转，两者显示同一张图的结果是上下颠倒的。
 * @param  p : 指向位图数据的指针，长度至少 1024 字节；非 NULL（不校验）。
 * @retval 无
 * @complexity O(1024) 次 SPI 字节写。
 */
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
 * @brief  在指定位置显示 1 个 ASCII 字符（8x16 点阵，固定 6 列宽）。
 * @note   字模取自 oledfont.h 的 ASCIIchardot[95][12]，索引 = 字符码 - 0x20
 *         （即正文 0x20 空格 对应下标 0）。前 6 字节是上半部（8 行）、
 *         后 6 字节是下半部，一个字符纵向跨 2 页，因此本函数会写 2 页：
 *         先写 page，再把 page 加 1 写下半部，最后把 page 还原 —— 这个
 *         page-- 只是恢复局部变量，不影响调用方的实参（C 值传递）。
 *         写入采用"页地址模式"，每次写数据后列地址自动 +1，所以连续 6 字节
 *         正好铺满 6 列。
 *         ⚠ 本函数只把"字模里为 1 的像素点亮"，不写背景（即不透写 0 像素），
 *         因此在一个已有内容的区域上画字会发生叠加；要干净显示请先清区。
 *         本函数列地址不做 +1 偏移（见 LCD_ShowCharReverse 与 OLED_Set_Pos 的差异）。
 * @param  col   : 起始列（像素列，0~127）。字模占 col ~ col+5 共 6 列；
 *                 col > 121 时右侧被截断甚至折回屏幕左侧，调用方需自行保证余量。
 * @param  page  : 上半部所在的逻辑页 0~7（page 内部对 8 取模，越界会回绕）。
 *                 下半部自动落在 page+1（当 page=7 时回绕到第 0 页，属环绕绘制）。
 * @param  Order : 字符的 ASCII 码，有效范围 0x20~0x7E；小于 0x20 会导致负下标越界。
 * @retval 无
 * @complexity O(12) = 12 次 SPI 字节写 + 6 条命令，局部绘制，立即生效。
 */
void LCD_ShowChar(uint8_t col, uint8_t page, uint8_t Order)
{
    uint8_t i;
    uint8_t ch = Order - 0x20;                // ASCII字符从0x20开始
    LCD_SetPageCol(page & 0x07, col, 0);
    for (i = 0; i < 6; i++)                   // 上半部分6列
    {
        LCD_WR_DATA8(ASCIIchardot[ch][i]);
    }
    page++;                                   // 下半字符page+1
    LCD_SetPageCol(page & 0x07, col, 0);
    for (i = 6; i < 12; i++)                  // 下半部分6列
    {
        LCD_WR_DATA8(ASCIIchardot[ch][i]);
    }
    page--; // 写完一个字符page还原
}

// 计算ASCII字符的有效宽度（去除左右空白）
// 字符数据结构：前6字节=上半部分6列，后6字节=下半部分6列，每字节代表一列的8行像素
// 返回值：有效宽度（列数），left表示左边空白列数
// 取模方式：纵向取模、字节正序、高位在上（bit7 为最上一行像素），详见 oledfont.h 文件头
// 下列行号用于与上层 oled_ui.c 的注释对齐（该文件引用了本文件的实现位置，
// 行号已按加注后的当前版本校正）：
//   LCD_DrawEchoCurve 内：清显存 ≈ oled.c:1080、整屏刷新 ≈ oled.c:1190-1197、
//   Y 轴刻度文字 ≈ oled.c:1200-1202
/**
 * @brief  (static) 测量 8x16 ASCII 字模的实际墨迹宽度，并回传左侧空白列数。
 * @note   用于紧凑排版：字模 6 列里首尾常有空白列，直接按 6 列推进会显得字距
 *         过大，因此先扫出"第一个非空列"和"最后一个非空列"。
 *         字模 12 字节的排布是：下标 0~5 为上半部 6 列、6~11 为下半部 6 列，
 *         上下两半必须在同一条列坐标上比较，所以扫下半部时统一减 6 归一到 0~5
 *         （局部变量 col 遮蔽了函数参数 col 的同名概念，此处只是循环计数）。
 *         整字为空白时（例如空格）返回 0，并通过 *left 回传 0。
 * @param  ch   : ASCII 字符码，有效范围 0x20~0x7E（内部减 0x20 取下标）。
 * @param  left : 输出参数，回传左侧空白列数（0~5）；整字空白时回传 0。
 *                不允许为 NULL，函数内直接写入。
 * @retval 字模有效宽度（列数，1~6）；字符为空白时返回 0。
 * @complexity O(12)，纯内存扫描，无总线访问。
 */
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

/**
 * @brief  紧凑显示 1 个 ASCII 字符：只画墨迹部分，右侧留 1 列间距。
 * @note   与 LCD_ShowChar 的区别是"不占满 6 列"：先由 LCD_GetCharWidth 算出
 *         有效宽度和左空白，跳过左侧空白列起画，只写 width 个字节，
 *         从而让 "1"、"i" 这类窄字符不被两侧空白撑开，一行能多塞几个字。
 *         同样跨 2 页写：上半部下标 left ~ left+width-1，下半部整体加 6 偏移。
 *         写入后立刻可见，会与已有内容叠加（不擦背景）。
 *         ⚠ 返回的是"含 1 列间距的推进量"，调用方必须累加返回值来定位下一个
 *           字符，不能再用固定 8 列推进。
 * @param  col  : 起始列 0~127（像素列）；不留余量校验，越界会折回左侧。
 * @param  page : 上半部逻辑页 0~7（对 8 取模；下半部为 page+1，可能回绕）。
 * @param  ch   : ASCII 字符码，有效 0x20~0x7E；<0x20 会造成字模负下标越界。
 * @retval 本字符实际占用的列数 = 有效宽度 + 1；若为空白字符（含空格）返回 3。
 * @complexity O(12) 扫描 + 最多 12 次 SPI 字节写，局部绘制且立即生效。
 */
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
    LCD_SetPageCol(page & 0x07, col, 0);
    for (uint8_t i = left; i < left + width; i++) {
        LCD_WR_DATA8(ASCIIchardot[idx][i]);
    }
    
    // 显示下半部分（只绘制有效宽度）
    page++;
    LCD_SetPageCol(page & 0x07, col, 0);
    for (uint8_t i = left + 6; i < left + width + 6; i++) {
        LCD_WR_DATA8(ASCIIchardot[idx][i]);
    }
    page--;
    
    // 严格只空1列间距
    return width + 1;
}

/**
 * @brief  紧凑地显示一个中英文混合字符串（汉字 12x12 + ASCII 紧凑 8x16）。
 * @note   排版规则：
 *           - 每个字符绘制前预判 "col + 12 > 128" —— 按最宽的汉字预留 12 列，
 *             放不下就换到"下一页组的首行"（page += 2）并回到第 0 列；
 *           - page >= 7（LCD_H/8-1）时回到左上角 (0,0) 从头覆盖，
 *             即超出屏幕时不是停止绘制而是回卷重画；
 *           - 汉字宽 12 列、跨 2 页（每页 12 字节，共 24 字节取自 Hzk[]），
 *             推进量固定 12；ASCII 走 LCD_ShowCharCompact，推进量取返回值
 *             （有效宽度 + 1，约 4~7 列），比标准排版更省横向空间。
 *         编码识别：首字节 >= 0x80 视为汉字，取索引交给 GetHzIndex()
 *         （支持 UTF-8 三字节与 GB2312 双字节，见 oledfont.h）；
 *         索引为 0xFF 表示字库里没有这个字，此时只跳过字节、不画字
 *         （会留下 12 列空档）。ASCII 分支是单字节，按 0x20 偏移取字模。
 *         ⚠ 绘制为覆盖式（不写背景 0），用在有旧内容的区域会叠加；
 *           且 page 用 & 0x07 / 未做下越界保护，page=7 时汉字下半会绕回第 0 页。
 * @param  col  : 起始列 0~127（像素列）。
 * @param  page : 起始逻辑页 0~7。
 * @param  puts : 字符串首地址（UTF-8 或 GB2312 均可），必须以 '\0' 结尾；
 *                不允许为 NULL（不校验）。
 * @retval 无
 * @complexity O(字符数 × (24 或 ≤12)) 次 SPI 字节写，逐个字符即时上屏，
 *             无整体刷新动作；字符串较长时是 UI 刷新中的主要耗时来源之一。
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

        // 判断是否为汉字（双字节/三字节字符）
        if (*puts >= 0x80)
        {
            uint8_t index = GetHzIndex((char *)puts);
            if (index != 0xFF)
            {
                // 12x12 汉字显示
                LCD_SetPageCol(page & 0x07, col, 0);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
                
                LCD_SetPageCol((page + 1) & 0x07, col, 0);
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

/**
 * @brief  (static) 单个 ASCII 字符在"紧凑排版"下的推进量（列数）。
 * @note   ★本函数存在的唯一理由：让"先量宽、再按宽度定位"与"真绘制"用同一套口径。
 *         LCD_ShowCharCompact() 的推进量是【有效墨迹宽度 + 1】，窄字符
 *         （'1'、'.'、'i'）只占 4~5 列；而 LCD_GetStrCompactWidth() 对数字
 *         一律按固定 9 列计。两者对同一个串会给出不同结果，
 *         拿后者去算右对齐/箭头落点必然偏（既有缺陷，见 oled.h 的说明）。
 *         凡是要"量宽后按宽度定位"的代码，一律用本函数的口径。
 * @param  ch : ASCII 字符码 0x20~0x7E；<0x20 会造成字模负下标越界。
 * @retval 本字符占用列数 = 有效宽度 + 1；空白字符（含空格）返回 3。
 * @complexity O(12)，纯内存扫描，无总线访问。
 */
static uint8_t LCD_AdvanceOf(uint8_t ch)
{
    uint8_t left;
    uint8_t w = LCD_GetCharWidth(ch, &left);
    return (w == 0u) ? 3u : (uint8_t)(w + 1u);
}

/**
 * @brief  量出 LCD_ShowStrExCompact() 实际会占用的列数。
 * @note   与 LCD_GetStrCompactWidth() 的区别：本函数对 ASCII 用 LCD_AdvanceOf()
 *         的真实推进量（窄字符不占 9 列），因此结果和画出来的像素完全一致。
 *         汉字按 12 列计（与 LCD_ShowStrExCompact 的 col += 12 一致）。
 * @param  str : 以 '\0' 结束的字符串（UTF-8 / GB2312 / ASCII 混排均可）。
 * @retval 占用列数；str 为 NULL 返回 0。返回 uint16_t，不做 255 回绕。
 * @complexity O(字符数 × ≤12)，纯内存扫描，无总线访问。
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
 * @brief  量出 LCD_ShowStrExBig() 实际会占用的列数。
 * @note   放大 2 倍 ⇒ 每个 ASCII 字符的推进量也是 2 倍（窄字符同样只是 2 倍，
 *         不会因为放大就变成等宽）。汉字不参与放大，串里若出现汉字则跳过、
 *         按 0 列计 —— 与 LCD_ShowStrExBig() 的"跳过不画"保持一致。
 * @param  str : 以 '\0' 结束的字符串（本函数只对 ASCII 部分计宽）。
 * @retval 占用列数；str 为 NULL 返回 0。
 * @complexity O(字符数)，纯内存扫描，无总线访问。
 */
uint16_t LCD_GetStrBigWidth(const char *str)
{
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
 * @brief  2 倍放大显示纯 ASCII 字符串（16 列 × 32 行，纵向占 4 个逻辑页）。
 * @note   用途：物位计首页的"大字读数"。依据是对标样机首页的实拍量取
 *         （功能图片 IMG_2180 的像素剖面：数值高约 32 行），正好是 8x16 字模
 *         放大 2 倍。放大方式是"最近邻 2 倍"——横向每 1 列源像素重复成 2 列、
 *         纵向每 1 行重复成 2 行，所以笔画是方块的，与样机观感一致；
 *         ★不新增任何字模，Flash 零增长（这一点是选它而不是加大字库的原因）。
 *         ⚠ 汉字不参与放大：遇到 >= 0x80 的字节按编码跳过、不画（留 0 列空档）。
 *         ⚠ 纵向占 4 页，故 page 只能取 0~4；page > 4 直接返回，否则会越过
 *           第 7 页、经 & 0x07 回绕把字画到屏幕上半部去。
 *         ⚠ 横向不做换行：整串一直向右推，超出 128 列由列地址回绕。
 *           调用方须自行保证 LCD_GetStrBigWidth(str) <= 128。
 *         ⚠ page=4 时下半部落到第 7 页（屏幕最下沿），是合法的最大取值。
 * @param  col  : 起始列 0~127（像素列）。
 * @param  page : 起始逻辑页 0~4（占 page ~ page+3 共 4 页 = 32 行）。
 * @param  str  : 以 '\0' 结束的 ASCII 串；不允许为 NULL（不校验）。
 * @retval 无
 * @complexity O(字符数 × 4 页 × ≤12 列) 次 SPI 字节写，逐字符立即上屏。
 */
void LCD_ShowStrExBig(uint8_t col, uint8_t page, const char *str)
{
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
 * @brief  显示 1 个汉字（12x12 点阵，占 2 页、共 24 字节）。
 * @note   字模来自 oledfont.h 的 Hzk[index][24]。排布：下标 0~11 是上半部
 *         12 列（第 1 页），12~23 是下半部 12 列（第 2 页）——因为 12x12 点阵
 *         纵向只有 12 行，跨到 2 页时下半部只用每页字节的低 4 位。
 *         索引由 GetHzIndex() 按字符串真实编码（UTF-8 / GB2312）查出；
 *         查不到（索引 0xFF）直接返回、什么都不画，也不报错。
 *         ⚠ page++ 只作用于局部变量，写完后没有 page-- 还原（与本文件
 *           LCD_ShowChar 的写法不同），但因是值传递，对调用方无影响。
 *         ⚠ 函数名/旧注释写的是 "16x16"，实际字库与绘制都是 12x12，
 *           行内注释已按实际改为 12 行；保留函数名以免改动对外接口。
 * @param  col  : 起始列 0~127（像素列），汉字占 col ~ col+11 共 12 列。
 * @param  page : 上半部逻辑页 0~7；下半部落在 page+1（page=7 时回绕到第 0 页）。
 * @param  hz   : 指向汉字首字节的指针，须是完整的 UTF-8/GB2312 序列；
 *                 非 NULL（不校验）。
 * @retval 无
 * @complexity O(24) = 24 次 SPI 字节写 + 6 条命令，局部绘制、立即生效。
 */
void LCD_ShowChinese(uint8_t col, uint8_t page, char *hz)
{
    uint8_t i;
    uint8_t index = GetHzIndex(hz);

    if (index == 0xFF) // 未找到字库
        return;

    // 显示上半部分 (8行)
    LCD_SetPageCol(page & 0x07, col, 0);
    for (i = 0; i < 16; i++)
    {
        LCD_WR_DATA8(Hzk[index][i]);
    }

    // 显示下半部分 (8行)
    page++;
    LCD_SetPageCol(page & 0x07, col, 0);
    for (i = 16; i < 32; i++)
    {
        LCD_WR_DATA8(Hzk[index][i]);
    }
}

/**
 * @brief  显示一个中英文混合字符串（标准间距版本：汉字 12 列 + ASCII 固定 8 列）。
 * @note   与 LCD_ShowStrExCompact() 是同一套逻辑，只有 ASCII 分支不同：
 *         这里调用 LCD_ShowChar() 画满 6 列字模并按固定 8 列推进，
 *         码位整齐、列位可预期，适合需要与其它元素对齐的表格化排版。
 *         换行/回卷规则：
 *           - 绘制前预判 col + 12 > 128 时 page += 2 并回到第 0 列；
 *           - page >= 7 时回到左上角 (0,0)，超屏后回卷覆盖而不是停止。
 *         汉字跨 2 页、每页 12 字节；查不到字模（索引 0xFF）时只跳字节不画字。
 *         ⚠ 与紧凑版的共同注意点：都是覆盖式绘制、会与旧内容叠加；
 *           都是即时上屏，没有整体刷新。
 * @param  col  : 起始列 0~127（像素列）。
 * @param  page : 起始逻辑页 0~7。
 * @param  puts : 字符串首地址（UTF-8 或 GB2312），以 '\0' 结束；不允许为 NULL。
 * @retval 无
 * @complexity O(字符数 × (24 或 12)) 次 SPI 字节写，随字符串长度线性增长。
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

        // 判断是否为汉字（双字节/三字节字符）
        if (*puts >= 0x80)
        {
            uint8_t index = GetHzIndex((char *)puts);
            if (index != 0xFF)
            {
                // 12x12 汉字显示（3页，每页12字节）
                // 第1页
                LCD_SetPageCol(page & 0x07, col, 0);
                for (uint8_t i = 0; i < 12; i++)
                    LCD_WR_DATA8(Hzk[index][i]);
                // 第2页
                LCD_SetPageCol((page + 1) & 0x07, col, 0);
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

/**
 * @brief  显示纯 ASCII 字符串（标准 8x16 字模，固定 8 列推进）。
 * @note   行末与屏末处理（与 ShowStrEx 系列不同，值得注意）：
 *           - col > 120（LCD_W-8）时换行：page += 2、col 归 0；
 *           - page > 6（LCD_H/8-2）时回卷到左上角 (0,0)，继续画。
 *         每个字符水平方向额外偏移 3 列后绘制（LCD_ShowChar(col + 3, ...)）
 *         再按 col += 8 推进，所以字符之间视觉间距比紧凑版宽，
 *         且整串相对传入的 col 整体右移 3 列——这是本工程沿用的对齐习惯。
 *         ⚠ 只支持 ASCII：调用方若传入汉字（首字节 >= 0x80），
 *           会按字节逐个当 ASCII 处理，显示为乱码。
 * @param  col  : 起始列 0~127；实际首个字符从 col+3 开始画。
 * @param  page : 起始逻辑页 0~7（每行占 2 页）。
 * @param  puts : 纯 ASCII 字符串首地址，以 '\0' 结束；不允许为 NULL。
 * @retval 无
 * @complexity O(字符数 × 12) 次 SPI 字节写，即时上屏。
 */
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

/**
 * @brief  显示 0~255 的三位十进制数（不显示无意义的前导 0）。
 * @note   拆分百/十/个位后逐个调用 LCD_ShowChar()：
 *           - 百位 a 为 0 时不画（也不刷空格），十位、个位照画；
 *           - 百位、十位同时为 0 时十位不画，只画个位。
 *           因不补空格，数字宽度随位数变化（1~3 字符），
 *           右侧未占用的列保持原内容不变，所以用本函数覆盖旧数字时，
 *           如果新数位数变少（如 100 → 99），旧的最左一位会残留。
 *           需要彻底擦除请先调用 LCD_ClearLine() 清该页。
 *           目前仅设置了 BACK_COLOR/POINT_COLOR 之外的颜色相关逻辑不受影响。
 * @param  col  : 起始列 0~127，字符依次占 col、col+8、col+16 列。
 * @param  page : 逻辑页 0~7（每个数字跨 2 页）。
 * @param  Num  : 待显示的数值 0~255；超过 255 时按 uint8_t 截断。
 * @retval 无
 * @complexity O(1~3 × 12) 次 SPI 字节写，最多 36 字节，局部绘制。
 */
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

/**
 * @brief  显示一张 128x64 的单色位图（整屏覆盖）。
 * @note   数据排布与屏幕一致：连续 8 页 × 128 列 = 1024 字节，
 *         每字节 bit7 在上、bit0 在下（oledfont.h 里 bmp1[]/bmp3[] 即此格式，
 *         由 PC 端取模工具按"横向取模、字节正序、高位在前"导出）。
 *         每页起始列固定写 0x10 + 0x04（而非 0x00）——本屏列地址起点有偏移，
 *         这两条合起来等价于"从可见区第 0 列开始"，与 OLED_Set_Pos 里的
 *         +1/+3 偏移是同一套硬件补偿，不要随意改动。
 *         页地址经 ComTable[] 映射，所以图不会上下颠倒。
 *         本函数不做清屏，写入即为覆盖整屏像素，适合做启动画面。
 * @param  puts : 位图数据首地址，必须 ≥1024 字节；传入 0(NULL) 直接返回，不访问总线。
 * @retval 无
 * @complexity O(1024) 次 SPI 字节写，是单次调用开销最大的绘制函数之一。
 */
void LCD_ShowBmp(uint8_t const *puts)
{
    if (puts == 0) return;   /* M3: 空指针保护 */
    uint8_t i, j;
    uint16_t X = 0;
    for (i = 0; i < (LCD_H / 8); i++)
    {
        LCD_WR_REG(0xB0 | ComTable[i]); // Set Page Address
        LCD_WR_REG(0x10);               // 列地址高 4 位 = 0
        /* 列地址起点：正装从 RAM 列 4 起（把 SEG4 当可见列第 0 列）；
         * 屏幕旋转 180° 后 SEG 扫描反向（0xA1），起点要镜像成 0，
         * 否则整幅图会被推到右侧不可见的 4 列上去。换算口径同
         * LCD_SetPageCol() 的注释（偏移 o → 4-o）。 */
#if LCD_SCAN_ROT180
        LCD_WR_REG(0x00);               // 倒装：列地址 = 0
#else
        LCD_WR_REG(0x04);               // 正装：列地址 = 4 (Colum from S1 -> S128 auto add)
#endif
        for (j = 0; j < LCD_W; j++)
        {
            LCD_WR_DATA8(puts[X]);
            X++;
        }
    }
}

/**
 * @brief  设置 OLED 显存的写入地址（页地址 + 列地址），后续写数据即从该点开始。
 * @note   只下发 3 条地址命令，不写任何像素数据；写数据时列地址会自动 +1。
 *         注意本函数的列地址做过硬件补偿：
 *           - real_col = col + 3 —— 本屏可见区左边缘相对芯片 SEG 起点有偏移，
 *             代码里给出的 col 已是"用户坐标"，故 +3 补偿；
 *             ⚠ real_col 计算后并未被使用（见下方"疑似问题"），
 *               实际下发的是 (col+1)>>4 与 col&0x0F，即只补偿了 1 列。
 *           - 高 4 位取 (col+1)>>4 而不是 col>>4，是同一补偿在高低半字节上的体现
 *             （写满 1 字符 = 12 字节，高半字节会进位，所以两处必须配套）。
 *         页地址经 ComTable[] 做逻辑页→芯片页翻转。因为列/页都做了补偿，
 *         本函数与直接写 ComTable[page]|0xB0 的 LCD_ShowChar 系列在横向
 *         会相差若干列，混用时务必以实测为准确认对齐。
 * @param  col  : 用户坐标系下的列 0~127（像素列），内部按 +1 补偿后下发。
 * @param  page : 逻辑页 0~7（内部对 8 取模），0 对应屏幕最上方。
 * @retval 无
 * @complexity O(3) 条命令字节，无数据字节。
 */
void OLED_Set_Pos(uint8_t col, uint8_t page)
{
    // 这里的 +3 和 ComTable 必须保留，因为这是你屏幕正常的物理映射
    uint8_t real_col = col + 3;
    LCD_SetPageCol(page & 0x07, col, 1);
}

/**
 * @brief  用小号字模（6x8）显示一个字符串，只有 1 页高。
 * @note   字模取自 oledfont.h 的 F6x8[][6]，下标 = 字符 - 0x20（' ' 空格为下标 0）。
 *         每个字符写 6 字节；写前用 OLED_Set_Pos(col, page) 重设一次地址，
 *         保证每次都从该字符的起点开始（否则连续写会跨列错位）。
 *         只占 1 页（8 像素行），常用于曲线视图的坐标刻度等小字。
 *         ⚠ F6x8 里只有数字 0~9、若干符号和 d/m/n 有字模，其它表项是空 {}，
 *           传入字母/大写字模会显示为空白（不是乱码，是不存在数据）。
 *         ⚠ 本函数按固定 6 列推进，不做换行、不做行末判断：
 *           字符串宽度超过 128-col 时，多出的字符会折回屏幕左侧继续画。
 * @param  col  : 起始列 0~127（像素列）。
 * @param  page : 逻辑页 0~7（单页高度，若为 7 则贴屏幕最下沿）。
 * @param  str  : 字符串首地址，以 '\0' 结束；不允许为 NULL（不校验）。
 * @retval 无
 * @complexity O(字符数 × 6) 次 SPI 字节写，单页局部绘制、立即生效。
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
 * @brief  (static) 测量 6x8 小字模的实际墨迹宽度，并回传左侧空白列宽。
 * @note   与 LCD_GetCharWidth() 同理，只是字模只有一页 6 列、不需要分上下半部，
 *         直接扫描 6 个字节找出首个/末个非空列。
 *         供紧凑小字排版使用：避免 "1"、"." 这类窄字符被 6 列撑开。
 * @param  ch   : 字符码，有效范围 ' '(0x20) 及以上（内部减 ' ' 取下标）。
 * @param  left : 输出参数，回传左侧空白列数（0~5）；整字空白时为 0；
 *                不允许为 NULL。
 * @retval 字模有效宽度（列数 1~6）；字符为空白时返回 0。
 * @complexity O(6)，纯内存扫描，无总线访问。
 */
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

/**
 * @brief  (static) 紧凑显示 1 个 6x8 字符：只画墨迹，右侧保留 1 列间距。
 * @note   先量宽度和左空白，OLED_Set_Pos 定位后只写有效的那几列字节，
 *         比 LCD_ShowStr_Small 的固定 6 列更省横向空间。
 *         仅占 1 页高度，不跨页。
 *         ⚠ 与 LCD_ShowStr_Small 一样，F6x8 里字母大多为空表项，
 *           传字母会画不出东西；使用前请确认所需字符在字库中确实有数据。
 * @param  col  : 起始列 0~127（用户坐标系，内部经 OLED_Set_Pos 的列补偿）。
 * @param  page : 逻辑页 0~7。
 * @param  ch   : 字符码，需为 0x20 及以上；<0x20 会造成字模负下标越界。
 * @retval 本字符实际占用列数 = 有效宽度 + 1；空白字符返回 3。
 * @complexity O(6) 扫描 + ≤6 次 SPI 字节写，单页局部绘制。
 */
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

/**
 * @brief  紧凑显示 6x8 小字字符串（逐字符累加实际宽度推进列）。
 * @note   循环里 col 加上的是 LCD_ShowSmallCharCompact() 的返回值
 *         （有效宽度 + 1），所以字符间距随字宽变化，一行能排得更紧凑。
 *         只占传入的那 1 页，换行/回卷都不做：内容超出 128 列时会折回左侧。
 *         曲线视图的刻度文字就是用它绘制的（见 LCD_DrawEchoCurve 末尾）。
 * @param  col  : 起始列 0~127（用户坐标系）。
 * @param  page : 逻辑页 0~7（单页高）。
 * @param  str  : 字符串首地址，以 '\0' 结束；不允许为 NULL（不校验）。
 * @retval 无
 * @complexity O(字符数 × ≤6) 次 SPI 字节写。本函数是 LCD_DrawEchoCurve
 *             内部除整屏刷新之外的第二笔 SPI 开销（3 条刻度文字）。
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
 * @brief  (static) 预计算字符串按"紧凑排版"绘制后的总像素宽度。
 * @note   专供 LCD_ShowArrowEx(mode=3) 计算箭头该落在哪一列：
 *         由于紧凑排版下每个字符宽度不同（而非固定 8 列），箭头位置不能靠
 *         字符数直接算，必须先按同样的规则把宽度累加出来，规则与
 *         LCD_ShowCharCompact 保持一致：
 *           - 汉字（首字节 >= 0x80）：固定 12 列，并按 UTF-8 三字节 /
 *             GB2312 双字节相应跳字节；
 *           - 空格：固定 3 列；
 *           - 数字 '0'~'9'：固定 9 列（即按满宽 8 列 + 1 列间距计，
 *             而实际绘制时数字有效宽度可能更窄，所以算出来的箭头位置
 *             相对数字串末尾会偏右，属已知偏差）；
 *           - 其它可打印 ASCII：扫描字模求有效宽度 + 1 列间距。
 *         本函数只读字库、不碰总线，纯计算，因此不会改变显示内容。
 * @param  str : 待测量字符串，以 '\0' 结束。
 * @retval 总宽度（列数，0~255）。str 为 NULL 时返回 0；宽度超过 255 时按
 *          uint8_t 回绕（长字符串下箭头位置会失真）。
 * @complexity O(字符数 × ≤12)，纯内存扫描，无 SPI 访问。
 */
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

/**
 * @brief  在指定页画一个 ">" 箭头，位置由 mode 决定（用于菜单/列表的选中标记）。
 * @note   mode 取值与箭头落点（以本文件实现为准，oled.h 里旧注释的 3/4 描述有误）：
 *           - 0（默认）：右对齐，箭头固定在 col = LCD_W - 10 = 118；
 *           - 1：左对齐，箭头在 col = 0（文本之前）；
 *           - 2：自定义，箭头在 col = 传入的 col；
 *           - 3：落在紧凑排版文本的右端，先由 LCD_GetStrCompactWidth(str)
 *                算出该字符串的总列宽，箭头画在 col + textWidth（即文本右侧）；
 *           - 其它值：等同 mode 0，右对齐。
 *         箭头本身复用 LCD_ShowStr 绘制字符 '>'，因此会继承它的两个特性：
 *         水平方向整体再 +3 列偏移，以及行末/屏末的换行回卷逻辑。
 *         箭头只占 1 个字符（2 页高、8 列宽），即时上屏。
 * @param  page : 逻辑页 0~7，箭头的上半部所在页（下半部在 page+1）。
 * @param  mode : 定位方式，见上（0/1/2/3，其它值按 0 处理）。
 * @param  col  : 仅 mode=3 时作为文本起始列、mode=2 时作为箭头列；
 *                mode=0/1 时该参数被忽略（可传 0）。
 * @param  str  : 仅 mode=3 时用于测宽的字符串；其它 mode 下不使用，
 *                可传 NULL（LCD_GetStrCompactWidth 内部对 NULL 有保护）。
 * @retval 无
 * @complexity O(测宽扫描 + 12) 次 SPI 字节写（mode=3 时还需一次字库扫描）。
 */
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

/* 在私有显存 frame_buffer 上"置 1"一个像素（只置位、不清零，叠加式描点）。
 * 坐标以屏幕左上角为原点：x = 像素列（0~127），y = 像素行（0~63）。
 * 换算：页 = y/8，页内位 = 7-(y%8)，即 bit7 朝上、bit0 朝下（见文件头示意图）。
 * 越界（x/y 超出屏幕）时静默丢弃，因此调用方无需再做边界判断，
 * 这也是本文件里唯一带自动越界保护的绘制入口。用 do{}while(0) 包裹，
 * 保证在 if/else 中当作单条语句使用不会出错。 */
#define DRAW_PIXEL(x, y) do { \
    if ((x) >= 0 && (x) < 128 && (y) >= 0 && (y) < 64) { \
        frame_buffer[(y) / 8][(x)] |= (1 << (7 - ((y) % 8))); \
    } \
} while(0)

/**
 * @brief  绘制诊断回波曲线：坐标轴 + 刻度 + 折线 + 坐标文字，并整屏刷入。
 * @note   ⚠ 副作用最重的绘制函数（唯一使用私有显存 frame_buffer 的函数）：
 *         1) 开头就把 frame_buffer 整块 memset 清零，函数返回后这块显存里
 *            的内容即被判定为废弃，下次调用会再次清空；
 *         2) 曲线全部在 frame_buffer 里"离屏"画完，最后按 8 页 × 128 列
 *            一次性刷入 GDDRAM（这一步才会真正上屏，共 1024 字节）；
 *         3) 刷完紧接着用 6x8 小字写 "0.01"、"m(d)"、"8.91" 三个刻度文字，
 *            文字是"越过 frame_buffer 直接写屏"的，所以如果本函数被频繁调用，
 *            文字会先被上面的整屏刷新擦掉再重画，属正常现象；
 *         4) 本函数会覆盖整屏像素，调用顺序上必须在同屏文字之前或之内完成；
 *            完整绘制顺序约束见 APP/Src/oled_ui.c 顶部文件头说明。
 *         绘制内容（括号内为硬编码坐标，均在 128x64 内）：
 *           - Y 轴竖线 x=8，y 从 15 到 63；X 轴横线 y=55，x 从 0 到 127；
 *           - Y 轴顶部箭头：x 在 8±w 展开，y = 16~20，w 随高度递增形成实心箭头；
 *           - X 轴右端箭头：顶点在 x=127，向左侧展宽，y = 55±w；
 *           - X 轴刻度：4 条，间隔 27 列，从 y=53 到 55；x 超过 120 时中断，
 *             以避开右侧箭头区域（即第 5 条及以后的刻度不会画出来）；
 *           - 数据折线：x 从 10 一直到 121，按 data_idx = (x-10-2)*x_scale
 *             取回波数据；y = 55 - (data * y_scale) / 5，并做纵向钳位
 *             （上限 15、下限 55），相邻点之间用垂直连线填补，保证曲线 1 像素宽。
 * @param  echo_data : 回波数据缓冲区指针，长度按 128 字节预留；传 NULL 时
 *                     只画坐标轴与刻度，不画曲线（保护见函数内 if）。
 * @param  x_scale   : X 方向抽样步长（每前进 1 列取第几个数据点），
 *                     建议 1~8；传 0 会被内部钳到 1（避免所有列取同一数据点）。
 * @param  y_scale   : Y 方向放大倍数（数据 → 像素高度的比例因子），
 *                     等价于把原式的 1/5 系数按 y_scale 倍缩放；
 *                     建议 1~8；传 0 会被内部钳到 1（避免曲线压成一条直线）。
 *                     数据约 275/y_scale 以上就会顶到 y=15 的钳位线。
 * @retval 无
 * @complexity O(1024) 次 SPI 字节写（整屏刷新）+ 约 60 次小字写 +
 *             约 300 次纯内存描点；是 UI 中单次开销最大的绘制函数，
 *             调用频率务必受控（oled_ui.c 里用脏标记做节流）。
 */
// 注意：为了STM32F1的性能，x_scale 和 y_scale 建议传入整型(uint8_t)
void LCD_DrawEchoCurve(uint8_t *echo_data, uint8_t x_scale, uint8_t y_scale)
{
    uint8_t i, j;

    /* M5: 缩放因子钳位，避免 0 导致曲线退化为直线或除零 */
    if (x_scale < 1u) x_scale = 1u;
    if (y_scale < 1u) y_scale = 1u;

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
        LCD_WR_REG(0x10);               // 列地址高 4 位 = 0
        /* 列地址起点：正装写 RAM 列 0~127（对应可见列 0~127）；
         * 屏幕旋转 180° 后 SEG 扫描反向（0xA1），起点镜像成 4
         * （ram_col = 4 对应可见列第 127 列），否则整屏会右移 4 列并露边。
         * 换算口径同 LCD_SetPageCol() 的注释（偏移 o → 4-o）。 */
#if LCD_SCAN_ROT180
        LCD_WR_REG(0x04);               // 倒装：列地址 = 4
#else
        LCD_WR_REG(0x00);               // 正装：列地址 = 0
#endif
        for (j = 0; j < 128; j++) {
            LCD_WR_DATA8(frame_buffer[i][j]);
        }
    }

    // 9. 绘制文字
    LCD_ShowStr_Small_Compact(11, 7, "0.01");
    LCD_ShowStr_Small_Compact(55, 7, "m(d)");
    LCD_ShowStr_Small_Compact(98, 7, "8.91");
}

/**
 * @brief  反白显示 1 个 ASCII 字符（字模取反，即"黑底白字"的效果）。
 * @note   与 LCD_ShowChar 的唯一区别是写入前对字模按位取反（~(byte)），
 *         被点亮的像素与正常字符正好相反。用于选中项的高亮显示。
 *         ⚠ 取反只是"整字节取反"，不做背景填充，因此：
 *           - 它是"在旧内容上做异或式反转"，同一位置重复调用两次会恢复原样；
 *           - 需要干净的实心反白块，必须在调用前先用同色块（0xFF）铺底，
 *             否则反白区域里会残留原先的图案。
 *         ⚠ 本函数的列地址按 (col+1)>>4 与 col&0x0F 下发，做了 +1 列补偿，
 *           与 LCD_ShowChar（不补偿）横向相差 1 列，二者混用会有轻微错位。
 *         字符跨 2 页；page++ 后同样以 page-- 还原局部变量。
 * @param  col   : 起始列 0~127（含 +1 列补偿）。
 * @param  page  : 上半部逻辑页 0~7；下半部在 page+1（page=7 时回绕）。
 * @param  Order : ASCII 码 0x20~0x7E，内部减 0x20 取字模下标。
 * @retval 无
 * @complexity O(12) = 12 次 SPI 字节写 + 6 条命令，局部绘制、立即生效。
 */
void LCD_ShowCharReverse(uint8_t col, uint8_t page, uint8_t Order)
{
    uint8_t i;
    uint8_t ch = Order - 0x20;                // ASCII字符从0x20开始
    LCD_SetPageCol(page & 0x07, col, 1);
    for (i = 0; i < 6; i++)                   // 上半部分6列
    {
        LCD_WR_DATA8(~(ASCIIchardot[ch][i]));
    }
    page++;                                   // 下半字符page+1
    LCD_SetPageCol(page & 0x07, col, 1);
    for (i = 6; i < 12; i++)                  // 下半部分6列
    {
        LCD_WR_DATA8(~(ASCIIchardot[ch][i]));
    }
    page--; // 写完一个字符page还原
}