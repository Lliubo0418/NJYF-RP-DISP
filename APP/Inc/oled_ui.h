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
} RADAR_PARAM;

extern volatile RADAR_PARAM gRadarParam;

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
	

extern const char *gLangTable[4][MENU_ID_MAX];

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
 * 字段映射为预留项，用户可据实际显示需求细化。 */
void UI_UpdateMeas(float distance, uint8_t peak_count, uint8_t mode);
void UI_UpdateDiag(uint8_t reliability, uint8_t status, float peakMinEmpty, float peakMaxEmpty);
void UI_UpdateEcho(const uint8_t *echo, uint8_t len);

#endif