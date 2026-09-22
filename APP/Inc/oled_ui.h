#ifndef __OLED_UI_H
#define __OLED_UI_H

#include "stdint.h"

// ==========================================
// 1. 枚举定义
// ==========================================

// 菜单项ID枚举（用于多语言资源表索引）
typedef enum
{
    // 根菜单
    MENU_ROOT_BASIC = 0,
    MENU_ROOT_DISPLAY,
    MENU_ROOT_DIAG,
    MENU_ROOT_SERVICE,

    // 基本设置
    MENU_BASIC_LOW_ADJ,
    MENU_BASIC_HIGH_ADJ,
    MENU_BASIC_MAT,
    MENU_BASIC_DAMP_TIME,
    MENU_BASIC_OUT_MAP,
    MENU_BASIC_SCALE_UNIT,
    MENU_BASIC_SCALE_VAL,
    MENU_BASIC_RANGE_SETTING,
    MENU_BASIC_BLIND_ZONE,
    MENU_BASIC_SENSOR_TAG,

    // 物料性质
    MENU_MAT_LIQUID,
    MENU_MAT_SOLID,              //15
    MENU_MAT_MICRO_DK,

    // 液体参数
    MENU_MAT_LIQUID_FAST_CHANGE,
    MENU_MAT_LIQUID_FIRST_WAVE,
    MENU_MAT_LIQUID_SURF_ANGLE,
    MENU_MAT_LIQUID_FOAM,
    MENU_MAT_LIQUID_SMALL_DK,
    MENU_MAT_LIQUID_PIPE,

    // 固体参数
    MENU_MAT_SOLID_FAST_CHANGE,
    MENU_MAT_SOLID_FIRST_WAVE,
    MENU_MAT_SOLID_SURF_ANGLE,
    MENU_MAT_SOLID_DUST,
    MENU_MAT_SOLID_SMALL_DK,
    MENU_MAT_SOLID_PIPE,

    // 微DK参数
    MENU_MAT_MICRO_FAST_CHANGE,
    MENU_MAT_MICRO_FIRST_WAVE,     //30
    MENU_MAT_MICRO_SURF_ANGLE,
    MENU_MAT_MICRO_NONE,
    MENU_MAT_MICRO_SMALL_DK,
    MENU_MAT_MICRO_PIPE,

    // 导波管
    MENU_PIPE_DIAMETER,

    // 显示
    MENU_DISP_CONTENT,
    MENU_DISP_CONTRAST,

    // 诊断
    MENU_DIAG_PEAK,
    MENU_DIAG_STATUS,
    MENU_DIAG_CURVE_SEL,
    MENU_DIAG_CURVE,
    MENU_DIAG_SIM,
    TEXT_PEAK_MIN_EMPTY,
    TEXT_PEAK_MAX_EMPTY,
    TEXT_DIAG_RELIABILITY,
    TEXT_DIAG_STATUS,

    // 曲线
    MENU_CURVE_ZOOM_DIR,
    MENU_CURVE_ZOOM_SCALE,

    // 服务
    MENU_SERV_FALSE_ECHO,
    MENU_SERV_CURRENT,
    MENU_SERV_RESET,
    MENU_SERV_UNIT,
    MENU_SERV_LANG,
    MENU_SERV_HART,
    MENU_SERV_COPY_DATA,
    MENU_SERV_PWD,
    MENU_SERV_OFFSET,
    MENU_SERV_THRESH,

    // 电流输出
    MENU_CURRENT_MODE,
    MENU_CURRENT_FAULT,
    MENU_CURRENT_MIN,

    // HART
    MENU_HART_MODE,
    MENU_HART_ADDR,

    // 阈值
    MENU_THRESH_ECHO,
    MENU_THRESH_ENV,

    // 页面标题
    PAGE_TITLE_ROOT,
    PAGE_TITLE_BASIC,
    PAGE_TITLE_MAT,
    PAGE_TITLE_MAT_LIQUID,
    PAGE_TITLE_MAT_SOLID,
    PAGE_TITLE_MAT_MICRO_DK,
    PAGE_TITLE_PIPE,
    PAGE_TITLE_DISPLAY,
    PAGE_TITLE_DIAG,
    PAGE_TITLE_CURVE,
    PAGE_TITLE_SERVICE,
    PAGE_TITLE_CURRENT,
    PAGE_TITLE_HART,
    PAGE_TITLE_HART_ADDR,
    PAGE_TITLE_THRESH,

    // 其他固定文本
    TEXT_HOME_NORMAL,
    TEXT_HOME_CURVE,
    TEXT_PIPE_DIAMETER_LABEL,
    TEXT_PIPE_DIAMETER_UNIT,

    MENU_ID_MAX
} MENU_ID;

// 菜单项类型 (决定了组件的行为和显示方式)
typedef enum
{
    MENU_PAGE = 0, // 子菜单跳转项 (按下K6进入下一级页面)
    MENU_BOOL,     // 布尔参数 (如: 否/是)
    MENU_FLOAT,    // 浮点数参数 (支持位编辑，如量程、阻尼时间)
    MENU_SELECT,   // 选项列表参数 (如: 液体/固体/微DK)
    MENU_ACTION,   // 动作触发项 (如: 复位、复制数据)
    MENU_STRING,   // 字符串编辑 (如: 传感器标签)
    MENU_READONLY  // 只读状态展示 (如: 测量峰值、传感器状态，不可编辑)
} MENU_TYPE;

// 界面全局状态机
typedef enum
{
    UI_HOME = 0, // 正常工作界面 / 曲线图首页
    UI_BROWSE,   // 菜单浏览状态 (上下翻阅菜单项)
    UI_EDIT      // 参数编辑状态 (光标反转，正在修改具体数值)
} UI_STATE;

// 页面排版类型 (决定了 UI_Task 如何在屏幕上画这一页)
typedef enum
{
    PAGE_TYPE_ROOT = 0, // 根菜单 (无标题栏，最多显示4行子项)
    PAGE_TYPE_MENU,     // 子菜单列表 (第一行显示标题，下面显示3行选项，如"物料性质"页)
    PAGE_TYPE_PARAM     // 参数配置详情页 (显示参数名、数值、单位、附带值等)
} PAGE_TYPE;

// 渲染布局模式
typedef enum
{
    RENDER_DEFAULT = 0,         // 标准布局: val居中显示, ex1/ex2 底部行
    RENDER_PIPE    = 1,         // 导波管: 全宽渲染，显示"导波管直径"标题与数值
    RENDER_PEAK    = 2,         // 测量峰值: 左对齐紧凑显示 val 与 ex1
    RENDER_STATUS  = 3,         // 测量状态: 左对齐紧凑显示 val 与 ex1
    RENDER_SERV_CURRENT = 4,    // 服务电流: 左对齐紧凑显示 val 与 ex1/ex2
    RENDER_SERV_HARTADDR = 5,   // 服务HART地址: 左对齐紧凑显示 val 与 ex1
    RENDER_SERV_THRESH = 6,     // 服务阈值: 左对齐紧凑显示 val 与 ex1/ex2
} MENU_RENDER;

// value 指针实际指向的变量类型
typedef enum
{
    VAL_FLOAT  = 0, // float 类型 (默认)
    VAL_UINT8  = 1, // uint8_t 类型
    VAL_STRING = 2, // char[] 字符串类型
} MENU_VALUE_TYPE;

// 编辑子状态
typedef enum
{
    EDIT_MAIN  = 0, // 正在编辑主值
    EDIT_EXTRA = 1, // 正在编辑附带值
} EDIT_SUBMODE;

struct MenuPage;

// ==========================================
// 2. 核心结构体定义
// ==========================================

// 单个菜单项/参数项定义
typedef struct MenuItem
{
    MENU_ID id;                  // 菜单项ID（用于多语言资源表索引）
    MENU_TYPE type;              // 参数类型
    void *value;                 // 绑定底层实际变量的内存地址指针
    float min;                   // 参数最小值限制
    float max;                   // 参数最大值限制
    float step;                  // 步进值
    const char * const *optionStr; // 指向多语言扁平化字典数组的指针
    uint8_t optionNum;           // 该选项包含的单语言条目总数
    const char *formatStr;       // 数值显示的格式化字符串
    const char *unitStr;         // 单位字符串
    struct MenuPage *subPage;    // 关联的子页面跳转指针
    void (*actionCallback)(void); // 动作回调函数
    void (*customFormat)(struct MenuItem *self, char *valBuf, char *extraBuf1, char *extraBuf2);

    MENU_RENDER renderMode;      // 渲染布局模式
    MENU_VALUE_TYPE valueType;   // value 指针指向的类型
    void *exValue;               // 附带可编辑值的地址指针
    const char *exFormat;        // 附带值的格式化串
    uint8_t paramId;             /* 对应主板 DISP_PARAM_ID + 1；0 表示不上报主板（仅本地显示项） */
    uint8_t exParamId;           /* 附带值对应主板 DISP_PARAM_ID + 1；0 表示不上报。
                                  * 例：低位调整的主值=百分比(DPARAM_LOW_ADJ_PCT+1)，
                                  * exValue=距离端点(DPARAM_LOW_ADJ_VAL+1)。
                                  * 两者必须都上报，否则主板 span=0 → 4-20mA 恒满量程。 */
} MenuItem;

// 页面定义
typedef struct MenuPage
{
    MENU_ID titleId;      // 当前页面的顶部标题ID
    PAGE_TYPE type;       // 页面渲染风格
    uint8_t flatten_nav;  // 导航扁平化标志
    MenuItem *items;      // 菜单项数组
    uint8_t itemCount;    // 菜单项总数量
} MenuPage;

// ==========================================
// 3. 业务参数结构体
// ==========================================
typedef struct
{
    // 1. 基本设置
    float lowAdjustPct;
    float lowAdjustVal;
    float highAdjustPct;
    float highAdjustVal;
    uint8_t matType;
    uint8_t matFastChange;
    uint8_t matFirstWave;
    uint8_t matSurfAngle;
    uint8_t matFoamDust;
    uint8_t matSmallDK;
    uint8_t matPipe;
    float pipeDiameter;
    float dampTime;
    uint8_t outMap;
    uint8_t scaleUnit;
    float scaleVal;
    float rangeSetting;
    float blindZone;
    char sensorTag[16];

    // 2. 显示
    uint8_t dispContent;
    float lcdContrast;

    // 3. 诊断
    float peakMinEmpty;
    float peakMaxEmpty;
    uint8_t diagReliability;
    uint8_t diagStatus;
    uint8_t diagCurveSel;
    uint8_t diagCurveZoom;
    uint8_t diagCurveScale;
    uint8_t diagSim;

    // 4. 服务
    uint8_t servFalseEcho;
    uint8_t currMode;
    uint8_t currFault;
    uint8_t currMin;
    uint8_t servReset;
    uint8_t servUnit;
    uint8_t servLang;
    uint8_t servHART;
    uint8_t servHARTAddr;
    uint8_t servCopyData;
    uint8_t servPwdEn;
    float servOffset;
    float threshEcho;
    float threshEnv;

    // 实时测量值
    float realTimeDistance;
    uint8_t measPeakCount;   /* 测量状态：峰值数（来自 MEAS，非配置） */
    uint8_t measMode;        /* 测量状态：TOF/标定模式（来自 MEAS，非配置） */
    float sensorTemperature; /* 传感器温度（来自 DIAG 帧，非配置） */
} RADAR_PARAM;

extern RADAR_PARAM gRadarParam;

// ==========================================
// 5. 多语言资源表
// ==========================================
#define LANG_CN  0  // 中文
#define LANG_EN  1  // English
#define LANG_IT  2  // Italian
#define LANG_FR  3  // French


// 常用列位置宏定义
#define COL_CENTER_X 30      // 文本居中起始列
#define COL_MENU_ITEM 10     // 菜单项起始列  
#define COL_VALUE_X 4        // 值显示起始列

/* 首页左侧预留区宽度（列）—— 留给「物位」显示（标签或竖条），当前不绘制任何内容。
 * 取 16 的三条理由：
 *   ① 128 = 8 x 16，16 列正好是屏宽的 1/8，与其它栅格对齐（12 只有"字模宽"一个含义）；
 *   ② 16 = 汉字字模 12 列 + 左右各 2 列呼吸 —— 竖排「物位」二字（12 列）放进去不贴边；
 *      若日后改画物位竖条，16 列 x 32 行的长宽比也更易于远距离辨识；
 *   ③ 代价为零：2 倍放大后最宽的读数（ft 满量程 "303.150"）只占 62 列，
 *      右侧仍余 112 列 —— "12 列能省出 4 列给数字"这个理由并不成立。
 * 只有一种情况该改回 12：左侧确定只放竖排汉字、且要求零留白贴屏框。
 * 实测宽度账见 .workbuddy/tools/_zone_calc.txt。 */
#define COL_HOME_LEFT_RESERVE 16u

/* 首页左侧预留条的填充字节（2026-09-21 起该预留条不再留空，改为整条填满）。
 * 本工程按负显（DFSTN 黑底白字）使用 ⇒ 0xFF = 亮块，0x00 = 黑块（与"不画"等效）。
 * 若手上这批是正显（FSTN 白底黑字）批次，含义正好相反，把这里改成 0x00u。
 * 依据见 oled.h 的 LCD_FillBlock() 注释与 LX-12864T5B 规格书第 3 节。 */
#define HOME_LEFT_FILL_BYTE 0xFFu
	

/* ★必须与 oled_ui.c 的定义同形（含第二个 const）。
 * 定义侧漏了 const 会把 1360B 指针数组放进 .data 占 RAM；此处 extern 也必须写 const 才不decl冲突。 */
extern const char * const gLangTable[4][MENU_ID_MAX];

static inline const char* UI_GetText(MENU_ID id)
{
    uint8_t lang = gRadarParam.servLang;
    if (lang < 4 && id < MENU_ID_MAX)
        return gLangTable[lang][id];
    return gLangTable[0][id];
}

// ==========================================
// 6. API 接口函数
// ==========================================
void UI_Init(void);
void UI_Task(void);
void UI_KeyK3_Back(void);
void UI_KeyK4_Up(void);
void UI_KeyK5_Loop(void);
void UI_KeyK6_Enter(void);

/* 协议数据更新接口（由 APP/app_disp.c 调用）：写入数据模型并置 uiDirty=1 触发重绘。
 * 字段映射为预留项，用户可据实际显示需求细化。
 * ★这些函数当前运行在 USART1 接收中断上下文，内部只允许 O(1) 赋值/小拷贝
 *   （总耗时必须远小于 2 字节时间 173.6µs）；新增 O(n) 运算务必改为
 *   "中断里置标志 + 主循环执行"，范例见 oled_ui.c 的 UI_RefreshTrendRange()。 */
void UI_UpdateMeas(float distance, uint8_t peak_count, uint8_t mode);
void UI_UpdateDiag(uint8_t reliability, uint8_t status, float peakMinEmpty, float peakMaxEmpty, float temperature);
void UI_UpdateEcho(const uint8_t *echo, uint8_t len);
void UI_UpdateParamDump(const uint8_t *payload, uint8_t len);  /* PARAM_DUMP 全量配置反序列化 */

/* 带曲线类型的数据更新（对应下行 0x06 ECHO_TYPED）：
 *   curveType 取 dispproto.h 的 DISP_CURVE_*（回波/虚假回波）。
 * 按类型写入各自独立的缓冲，避免多条曲线互相覆盖。 */
void UI_UpdateEchoTyped(uint8_t curveType, const uint8_t *data, uint8_t len);

/* 取当前 diagCurveSel 对应的曲线数据指针（供曲线页/首页渲染）。
 * 返回的指针指向 128 点 0~255 标度数据，可直接交给 LCD_DrawEchoCurve。
 * 无数据时返回 NULL，渲染方据此只画坐标轴、不画假波形。 */
const uint8_t *UI_GetSelectedCurve(void);

/* 输出走势曲线已累积的有效点数（0~128）。
 * 归档模式下小于 128 表示"还没攒够"，界面可显示进度；
 * 也用于区分"曲线是平的"和"压根没有数据"这两种不同状态。 */
uint8_t UI_GetTrendPointCount(void);

#endif