#include "oled_ui.h"
#include "oled.h"
#include "oledfont.h"
#include "dispproto.h"
#include "app_disp.h"
#include "main.h"     /* HAL_GetTick：写参数 ACK 提示的显示时长 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* 格式化输出缓冲大小（必须与 UI_FormatItemValue / CustomFormat 调用方栈缓冲区一致） */
#define CUSTOM_BUF_SIZE 32u

/* 曲线数据缓冲：按类型分开存放（0x02 ECHO 发实时回波、0x06 ECHO_TYPED 发各诊断曲线）
 * ⚠ 必须分缓冲：共用一块无法分辨类型，会导致"选择曲线"只换标题不换内容 */
uint8_t radar_echo[128] = {
    8,  7,  9,  8, 10,  9, 11, 10,  9,  8,  9, 10, 11, 12, 10, 11,
   12, 13, 14, 15, 13, 12, 11, 10, 12, 13, 14, 15, 16, 18, 20, 22,
   25, 28, 32, 36, 40, 44, 48, 52, 58, 65, 72, 80, 88, 96,104,112,
  120,128,135,142,148,152,155,158,160,162,158,152,145,138,130,122,
  114,106, 98, 90, 82, 74, 68, 62, 58, 54, 50, 46, 44, 42, 40, 38,
   35, 32, 30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10,  9,  8,  7,
    6,  5,  4,  3,  2,  1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
   10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
};
/* 虚假回波曲线（空罐学习基线）。初值全 0：未收到真实数据前画平线，不用演示数据冒充 */
uint8_t radar_echo_false[128] = {0};

/* ---------- 输出走势曲线：距离历史环形缓冲 ----------
 * 由本板自攒：每收到一帧 MEAS 存一点即可，主控与协议零改动（改由主控下发要多两处改动面）。
 * 当前只走【归档】：凑满 128 点即冻结（见 gTrendSwitchArchived）；滚动视图路径保留但默认不启用。 */
#define CURVE_HIST_LEN  128u
static float    s_hist_dist[CURVE_HIST_LEN];    /* 128×4B = 512B */
static uint8_t  s_hist_valid = 0;               /* 已写入的有效点数（0~128） */
static uint16_t s_hist_head  = 0;               /* 下一个写入位置 */
static float    s_hist_min   = 0.0f;            /* 用于 Y 轴自适应 */
static float    s_hist_max   = 0.0f;
/* 渲染缓冲：把 float 历史标度成 0~255 供 LCD_DrawEchoCurve 复用。
 * 单独一块而非就地转换，是为了不破坏原始 float 历史（重绘时需重新标度）。 */
static uint8_t  s_trend_render[CURVE_HIST_LEN];

/* 走势 Y 轴范围（s_hist_min/s_hist_max）的"待重算"请求标志。
 * ⚠ 重算（最坏 127 次软浮点比较，F103 无 FPU）若放在 USART1 接收中断链路上会超过 2 字节时间
 *   173.6µs → 溢出(ORE)，且本板无 ErrorCallback ⇒ 接收永久停摆。中断里只置位，扫描搬到主循环。 */
static volatile uint8_t s_histRangeDirty = 0;

/* 走势曲线模式开关（纯内部行为，不接 MENU —— 手册中不存在此菜单项）。
 * 1 = 归档：凑满 128 点即冻结；初值 1 保持既有运行表现。 */
uint8_t gTrendSwitchArchived = 1;

RADAR_PARAM gRadarParam;
static UI_STATE gUIState = UI_HOME;
static uint8_t home_view_mode = 0;
static volatile uint8_t uiDirty = 1;

static char editStr[16] = {0};
static uint8_t editCursorPos = 0;
static EDIT_SUBMODE editTarget = EDIT_MAIN;
/* SELECT 编辑原值快照：Edit_Select 就地修改 *value，K3 取消时据此回滚；
 * FLOAT/STRING 改的是 editStr，随状态退出自然丢弃。 */
static uint8_t editSelectOrig = 0;

/* --- 多语言扁平化字典常量数组 (结构: [Lang_0_Options..., Lang_1_Options...]) --- */
static const char * const dict_bool[] = {
    "否", "是",
    "No", "Yes",
    "No", "Si",
    "Non", "Oui"
};

static const char * const dict_mat[] = {
    "液体", "固体", "微DK",
    "Liquid", "Solid", "Micro DK",
    "Liquido", "Solido", "Micro DK",
    "Liquide", "Solide", "Micro DK"
};

static const char * const dict_wave[] = {
    "正常", "稍强", "较强", "最强", "减弱",
    "Normal", "Slight", "Strong", "Max", "Weak",
    "Normale", "Lieve", "Forte", "Max", "Debole",
    "Normal", "Leger", "Fort", "Max", "Faible"
};

static const char * const dict_outMap[] = {
    "线性", "锥筒",
    "Linear", "Cone",
    "Lineare", "Cono",
    "Lineaire", "Cone"
};

static const char * const dict_scaleUnit[] = {
    "高度", "质量", "流量", "体积", "无量纲",
    "Height", "Mass", "Flow", "Volume", "None",
    "Altezza", "Massa", "Flusso", "Volume", "Nessuno",
    "Hauteur", "Masse", "Flux", "Volume", "Aucun"
};

static const char * const dict_disp[] = {
    "不工作", "空高", "料高", "电流", "百分比",
    "Off", "Empty", "Level", "Current", "Percent",
    "Spento", "Vuoto", "Livello", "Corrente", "Per cento",
    "Eteint", "Vide", "Niveau", "Courant", "Pourcent"
};

static const char * const dict_curve[] = {
    "回波曲线", "虚假回波曲线", "输出走势曲线",
    "Echo Curve", "False Curve", "Trend Curve",
    "Curva Eco", "Curva Falsa", "Curva Trend",
    "Courbe Echo", "Courbe Fausse", "Courbe Trend"
};

static const char * const dict_zoomDir[] = {
    "X轴缩放", "Y轴缩放", "不缩放",
    "X Zoom", "Y Zoom", "No Zoom",
    "Zoom X", "Zoom Y", "No Zoom",
    "Zoom X", "Zoom Y", "Pas Zoom"
};

static const char * const dict_zoomScale[] = {
    "1X", "2X", "5X", "10X",
    "1X", "2X", "5X", "10X",
    "1X", "2X", "5X", "10X",
    "1X", "2X", "5X", "10X"
};

static const char * const dict_sim[] = {
    "百分比", "电流", "空高",
    "Percent", "Current", "Empty",
    "Per cento", "Corrente", "Vuoto",
    "Pourcent", "Courant", "Vide"
};

static const char * const dict_flsEcho[] = {
    "删除", "更新", "新建", "编辑",
    "Delete", "Update", "New", "Edit",
    "Elimina", "Aggiorna", "Nuovo", "Modifica",
    "Effacer", "Maj", "Nouveau", "Editer"
};

static const char * const dict_currMode[] = {
    "4~20mA", "20~4mA",
    "4~20mA", "20~4mA",
    "4~20mA", "20~4mA",
    "4~20mA", "20~4mA"
};

static const char * const dict_faultMode[] = {
    "无变化", "20.5mA", "22.0mA",
    "Hold", "20.5mA", "22.0mA",
    "Mantieni", "20.5mA", "22.0mA",
    "Maintien", "20.5mA", "22.0mA"
};

static const char * const dict_minCurr[] = {
    "4mA", "3.8mA",
    "4mA", "3.8mA",
    "4mA", "3.8mA",
    "4mA", "3.8mA"
};

static const char * const dict_reset[] = {
    "基本复位", "工厂设置", "测量峰值",
    "Basic Reset", "Factory Reset", "Peak Reset",
    "Reset Base", "Reset Fabbrica", "Reset Picco",
    "Reset Base", "Reset Usine", "Reset Pic"
};

static const char * const dict_unit[] = {
    "m(d)", "ft(d)",
    "m(d)", "ft(d)",
    "m(d)", "ft(d)",
    "m(d)", "ft(d)"
};

static const char * const dict_lang[] = {
    "中文", "English", "Italian", "French",
    "中文", "English", "Italian", "French",
    "中文", "English", "Italian", "French",
    "中文", "English", "Italian", "French"
};

static const char * const dict_hart[] = {
    "标准", "多点",
    "Standard", "Multi-drop",
    "Standard", "Multi-drop",
    "Standard", "Multi-drop"
};

static const char * const dict_copy[] = {
    "从传感器复制", "复制到传感器",
    "From Sensor", "To Sensor",
    "Dal Sensore", "Al Sensore",
    "Du Capteur", "Au Capteur"
};

/* --- 多语言资源表 ---
 * ⚠ 每行条目必须与 MENU_ID 枚举同序同长（列索引 = MENU_ID），增删菜单项须同步；
 *   且必须 const，否则 1360B 指针数组会落进 .data 占 RAM（F103CB 仅 20KB）。 */
const char * const gLangTable[4][MENU_ID_MAX] = {
    // 中文
    {
        // 根菜单
        "基本设置", "显示", "诊断", "服务",
        // 基本设置
        "低位调整", "高位调整", "物料性质", "阻尼时间", "输出映射", "定标量单位", "定标", "量程设定", "盲区范围", "传感器标签",
        // 物料性质
        "液体", "固体", "微DK",
        // 液体参数
        "物料快速变化", "首波选择", "表面波动", "泡沫", "DK值小", "导波管测量",
        // 固体参数
        "物料快速变化", "首波选择", "堆角大", "粉尘强", "DK值小", "导波管测量",
        // 微DK参数
        "物料快速变化", "首波选择", "表面波动", "无", "DK值小", "导波管测量",
        // 导波管
        "导波管直径",
        // 显示
        "显示内容", "LCD对比度",
        // 诊断
        "测量峰值", "测量状态", "选择曲线", "回波曲线", "仿真",
        "最小空高值", "最大空高值", "测量可靠性", "传感器状态",
        // 曲线
        "缩放方向", "缩放比例",
        // 服务
        "虚假回波", "电流输出", "复位", "测量单位", "语言", "HART工作模式", "复制传感器数据", "密码", "距离偏量", "阈值设定",
        // 电流输出
        "输出模式", "故障模式", "最小电流",
        // HART
        "工作模式", "地址",
        // 阈值
        "回波阈值", "包络线",
        // 页面标题
        "", "基本设置", "物料性质", "液体", "固体", "微DK", "导波管测量", "显示", "诊断", "回波曲线", "服务", "电流输出", "HART模式", "HART地址", "阈值设定",
        // 其他固定文本
        "正常工作", "回波曲线", "导波管直径:", "mm"
    },
    {
        // 根菜单
        "Basic", "Display", "Diag", "Service",
        // 基本设置
        "Low Adj", "High Adj", "Material", "Damp Time", "Out Map", "Scale Unit", "Scale", "Range", "Blind Zone", "Sensor Tag",
        // 物料性质
        "Liquid", "Solid", "Micro DK",
        // 液体参数
        "Fast Change", "First Wave", "Surf Angle", "Foam", "Small DK", "Pipe",
        // 固体参数
        "Fast Change", "First Wave", "Large Angle", "Dust", "Small DK", "Pipe",
        // 微DK参数
        "Fast Change", "First Wave", "Surf Angle", "None", "Small DK", "Pipe",
        // 导波管
        "Pipe Dia",
        // 显示
        "Disp Content", "LCD Contrast",
        // 诊断
        "Peak", "Meas Status", "Curve Sel", "Curve", "Simulation",
        "Min Empty", "Max Empty", "Meas Reliab", "Sens Status",
        // 曲线
        "Zoom Dir", "Zoom Scale",
        // 服务
        "False Echo", "Current", "Reset", "Unit", "Language", "HART Mode", "Copy Data", "Password", "Offset", "Threshold",
        // 电流输出
        "Out Mode", "Fault Mode", "Min Current",
        // HART
        "Mode", "Addr",
        // 阈值
        "Echo Thresh", "Envelope",
        // 页面标题
        "", "Basic", "Material", "Liquid", "Solid", "Micro DK", "Pipe", "Display", "Diag", "Curve", "Service", "Current", "HART", "HART Addr", "Threshold",
        // 其他固定文本
        "Normal", "Curve", "Pipe Dia:", "mm"
    },
    {
        // 根菜单
        "Base", "Display", "Diag", "Service",
        // 基本设置
        "Basso", "Alto", "Materiale", "Tempo", "Mappa", "Unita", "Scala", "Intervallo", "Zona Cieca", "Tag Sensore",
        // 物料性质
        "Liquido", "Solido", "Micro DK",
        // 液体参数
        "Cambio Rapido", "Prima Onda", "Angolo", "Schiuma", "DK Piccolo", "Tubo",
        // 固体参数
        "Cambio Rapido", "Prima Onda", "Angolo Grande", "Polvere", "DK Piccolo", "Tubo",
        // 微DK参数
        "Cambio Rapido", "Prima Onda", "Angolo", "Nessuno", "DK Piccolo", "Tubo",
        // 导波管
        "Diametro",
        // 显示
        "Contenuto", "Contrasto",
        // 诊断
        "Picco", "Meas Stato", "Sel Curva", "Curva", "Simulazione",
        "Vuoto Min", "Vuoto Max", "Affidabilita", "Sens Stato",
        // 曲线
        "Direzione", "Scala",
        // 服务
        "Eco Falso", "Corrente", "Reset", "Unita", "Lingua", "Modo HART", "Copia Dati", "Password", "Offset", "Soglia",
        // 电流输出
        "Modo", "Guasto", "Min Corrente",
        // HART
        "Modo", "Indirizzo",
        // 阈值
        "Soglia Eco", "Inviluppo",
        // 页面标题
        "", "Base", "Materiale", "Liquido", "Solido", "Micro DK", "Tubo", "Display", "Diag", "Curva", "Service", "Corrente", "HART", "Indirizzo HART", "Soglia",
        // 其他固定文本
        "Normale", "Curva", "Diametro:", "mm"
    },
    {
        // 根菜单
        "Base", "Display", "Diag", "Service",
        // 基本设置
        "Bas", "Haut", "Materiau", "Temps", "Carte", "Unite", "Echelle", "Intervalle", "Zone Aveugle", "Etiquette",
        // 物料性质
        "Liquide", "Solide", "Micro DK",
        // 液体参数
        "Changement Rapide", "Premiere Onde", "Angle", "Mousse", "DK Petit", "Tube",
        // 固体参数
        "Changement Rapide", "Premiere Onde", "Grand Angle", "Poussiere", "DK Petit", "Tube",
        // 微DK参数
        "Changement Rapide", "Premiere Onde", "Angle", "Aucun", "DK Petit", "Tube",
        // 导波管
        "Diametre",
        // 显示
        "Contenu", "Contraste",
        // 诊断
        "Pic", "Meas Etat", "Sel Courbe", "Courbe", "Simulation",
        "Vide Min", "Vide Max", "Fiabilite", "Sens Etat",
        // 曲线
        "Direction", "Echelle",
        // 服务
        "Eco Faux", "Courant", "Reset", "Unite", "Langue", "Mode HART", "Copier Donnees", "Mot de Passe", "Decalage", "Seuil",
        // 电流输出
        "Mode", "Defaut", "Min Courant",
        // HART
        "Mode", "Adresse",
        // 阈值
        "Seuil Eco", "Enveloppe",
        // 页面标题
        "", "Base", "Materiau", "Liquide", "Solide", "Micro DK", "Tube", "Display", "Diag", "Courbe", "Service", "Courant", "HART", "Adresse HART", "Seuil",
        // 其他固定文本
        "Normal", "Courbe", "Diametre:", "mm"
    }
};

/* --- 自定义格式化回调 --- */
/**
 * @brief  低位调整项的显示格式化
 * @param  self   菜单项
 * @param  valBuf 主值输出（百分比）
 * @param  ex1    附带值1输出（当前端点距离）
 * @param  ex2    附带值2输出（实时距离）
 * @retval 无
 */
static void CustomFormat_LowAdj(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    snprintf(valBuf, CUSTOM_BUF_SIZE, "%0.2f %%", *((float *)self->value));
    snprintf(ex1, CUSTOM_BUF_SIZE, "%.3f m(d)", gRadarParam.lowAdjustVal);
    snprintf(ex2, CUSTOM_BUF_SIZE, "%.3f m(d)", gRadarParam.realTimeDistance);
}
/**
 * @brief  高位调整项的显示格式化
 * @param  self   菜单项
 * @param  valBuf 主值输出（百分比）
 * @param  ex1    附带值1输出（当前端点距离）
 * @param  ex2    附带值2输出（实时距离）
 * @retval 无
 */
static void CustomFormat_HighAdj(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    snprintf(valBuf, CUSTOM_BUF_SIZE, "%0.2f %%", *((float *)self->value));
    snprintf(ex1, CUSTOM_BUF_SIZE, "%.3f m(d)", gRadarParam.highAdjustVal);
    snprintf(ex2, CUSTOM_BUF_SIZE, "%.3f m(d)", gRadarParam.realTimeDistance);
}
/**
 * @brief  诊断页空罐峰值区间
 * @param  self   菜单项
 * @param  valBuf 主值输出（最小空高值）
 * @param  ex1    附带值1输出（最大空高值）
 * @param  ex2    未使用
 * @retval 无
 */
static void CustomFormat_peakMinMaxEmpty(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    snprintf(valBuf, CUSTOM_BUF_SIZE, "%s: %.0f m(d)", UI_GetText(TEXT_PEAK_MIN_EMPTY), gRadarParam.peakMinEmpty);
    snprintf(ex1, CUSTOM_BUF_SIZE, "%s: %.0f m(d)", UI_GetText(TEXT_PEAK_MAX_EMPTY), gRadarParam.peakMaxEmpty);
}
/**
 * @brief  诊断页测量状态（可靠性 + 状态位）
 * @param  self   菜单项
 * @param  valBuf 主值输出（可靠度 dB）
 * @param  ex1    附带值1输出（状态位）
 * @param  ex2    未使用
 * @retval 无
 */
static void CustomFormat_diag_status(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    snprintf(valBuf, CUSTOM_BUF_SIZE, "%s: %d dB", UI_GetText(TEXT_DIAG_RELIABILITY), gRadarParam.diagReliability);
    snprintf(ex1, CUSTOM_BUF_SIZE, "%s: %d K", UI_GetText(TEXT_DIAG_STATUS), gRadarParam.diagStatus);
}
/**
 * @brief  服务页电流输出三档汇总
 * @param  self   菜单项
 * @param  valBuf 主值输出（输出模式）
 * @param  ex1    附带值1输出（故障模式）
 * @param  ex2    附带值2输出（最小电流）
 * @retval 无
 */
static void CustomFormat_serv_current(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;   /* 语言索引钳位 */
    /* ⚠ 索引钳位：lang 与三个档位都存在 EEPROM，可能残留越界值；字典是"4 语言 × N 档"
     *   扁平数组（索引 = lang*N + 档位），不钳位会读越界，故按每语言档数(2/3/2)钳位。 */
    uint8_t m  = (gRadarParam.currMode  > 1u) ? 1u : gRadarParam.currMode;
    uint8_t ft = (gRadarParam.currFault > 2u) ? 2u : gRadarParam.currFault;
    uint8_t mn = (gRadarParam.currMin   > 1u) ? 1u : gRadarParam.currMin;
    snprintf(valBuf, CUSTOM_BUF_SIZE, "%s: %s ", UI_GetText(MENU_CURRENT_MODE), dict_currMode[lang * 2 + m]);
    snprintf(ex1, CUSTOM_BUF_SIZE, "%s: %s ", UI_GetText(MENU_CURRENT_FAULT), dict_faultMode[lang * 3 + ft]);
    snprintf(ex2, CUSTOM_BUF_SIZE, "%s: %s ", UI_GetText(MENU_CURRENT_MIN), dict_minCurr[lang * 2 + mn]);
}
/**
 * @brief  服务页 HART 工作模式 + 地址
 * @param  self   菜单项
 * @param  valBuf 主值输出（工作模式）
 * @param  ex1    附带值1输出（地址）
 * @param  ex2    未使用
 * @retval 无
 */
static void CustomFormat_serv_hartaddr(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;   /* 语言索引钳位 */
    snprintf(valBuf, CUSTOM_BUF_SIZE, "%s ", dict_hart[lang * 2 + gRadarParam.servHART]);
    snprintf(ex1, CUSTOM_BUF_SIZE, "%s: %d", UI_GetText(MENU_HART_ADDR), gRadarParam.servHARTAddr);
}
/**
 * @brief  服务页阈值（回波阈值 + 包络线）
 * @param  self   菜单项
 * @param  valBuf 主值输出（回波阈值）
 * @param  ex1    附带值1输出（包络线）
 * @param  ex2    未使用
 * @retval 无
 */
static void CustomFormat_serv_thresh(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    snprintf(valBuf, CUSTOM_BUF_SIZE, "%s: %.0f ", UI_GetText(MENU_THRESH_ECHO), gRadarParam.threshEcho);
    snprintf(ex1, CUSTOM_BUF_SIZE, "%s: %.0f ", UI_GetText(MENU_THRESH_ENV), gRadarParam.threshEnv);
}

/* --- 声明所有页面 --- */
extern MenuPage Page_Root, Page_Basic, Page_Display, Page_Diag, Page_Service;
extern MenuPage Page_Mat, Page_MatLiquid, Page_MatSolid, Page_MatMicroDK, Page_Pipe;
extern MenuPage Page_Curve, Page_Current, Page_HART, Page_HARTAddr, Page_Thresh;
extern MenuPage Page_DiagPeak, Page_DiagStatus;

/* ---------------- 菜单项定义 ---------------- */

static MenuItem items_root[] = {
    {MENU_ROOT_BASIC, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Basic, NULL, NULL},
    {MENU_ROOT_DISPLAY, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Display, NULL, NULL},
    {MENU_ROOT_DIAG, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Diag, NULL, NULL},
    {MENU_ROOT_SERVICE, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Service, NULL, NULL}};

static MenuItem items_basic[] = {
    {MENU_BASIC_LOW_ADJ, MENU_FLOAT,  &gRadarParam.lowAdjustPct,  0, 100, 1,    NULL, 0, "%0.2f", NULL, NULL, NULL, CustomFormat_LowAdj,
     RENDER_DEFAULT, VAL_FLOAT, &gRadarParam.lowAdjustVal, "%.3f", DPARAM_LOW_ADJ_PCT + 1, DPARAM_LOW_ADJ_VAL + 1},
    {MENU_BASIC_HIGH_ADJ, MENU_FLOAT,  &gRadarParam.highAdjustPct, 0, 100, 1,    NULL, 0, "%0.2f", NULL, NULL, NULL, CustomFormat_HighAdj,
     RENDER_DEFAULT, VAL_FLOAT, &gRadarParam.highAdjustVal, "%.3f", DPARAM_HIGH_ADJ_PCT + 1, DPARAM_HIGH_ADJ_VAL + 1},
    {MENU_BASIC_MAT, MENU_PAGE,   &gRadarParam.matType, 0, 0, 0, dict_mat, 3, NULL, NULL, &Page_Mat, NULL, NULL},
    {MENU_BASIC_DAMP_TIME, MENU_FLOAT,  &gRadarParam.dampTime, 0, 100, 1, NULL, 0, "%0.0f", "S", NULL, NULL, NULL,
     RENDER_DEFAULT, VAL_FLOAT, NULL, NULL, DPARAM_DAMP_TIME + 1},
    {MENU_BASIC_OUT_MAP, MENU_SELECT, &gRadarParam.outMap, 0, 1, 1, dict_outMap, 2, NULL, NULL, NULL, NULL, NULL,
     RENDER_DEFAULT, VAL_UINT8, NULL, NULL, DPARAM_OUT_MAP + 1},
    {MENU_BASIC_SCALE_UNIT, MENU_SELECT, &gRadarParam.scaleUnit, 0, 4, 1, dict_scaleUnit, 5, NULL, NULL, NULL, NULL, NULL,
     RENDER_DEFAULT, VAL_UINT8, NULL, NULL, DPARAM_SCALE_UNIT + 1},
    {MENU_BASIC_SCALE_VAL, MENU_FLOAT,  &gRadarParam.scaleVal, 0, 1000, 1, NULL, 0, "%0.2f", NULL, NULL, NULL, NULL,
     RENDER_DEFAULT, VAL_FLOAT, NULL, NULL, DPARAM_SCALE_VAL + 1},
    /* ⚠ 量程设定必须用 "%05.2f"（2 位整数 + 2 位小数）：Edit_Float() 逐位 0~9 循环、不进位，
     *   用 "%0.3f" 只能编辑到 9.999 m。上报米值，主板按 <=15/<=30/<=45/<=60/其余 映射档 1~5；
     *   切档后 EEPROM 基线点数不匹配 ⇒ 基线作废、测量 SUPPRESSED，须空罐重新学习。 */
    {MENU_BASIC_RANGE_SETTING, MENU_FLOAT,  &gRadarParam.rangeSetting, 0, 100, 0.1, NULL, 0, "%05.2f", "m", NULL, NULL, NULL,
     RENDER_DEFAULT, VAL_FLOAT, NULL, NULL, DPARAM_RANGE_SETTING + 1},
    {MENU_BASIC_BLIND_ZONE, MENU_FLOAT,  &gRadarParam.blindZone, 0, 10, 0.01, NULL, 0, "%0.3f", "m", NULL, NULL, NULL,
     RENDER_DEFAULT, VAL_FLOAT, NULL, NULL, DPARAM_BLIND_ZONE + 1},
    {MENU_BASIC_SENSOR_TAG, MENU_STRING, &gRadarParam.sensorTag, 0, 0, 0, NULL, 0, NULL, NULL, NULL, NULL, NULL,
     RENDER_DEFAULT, VAL_STRING, NULL, NULL, DPARAM_SENSOR_TAG + 1}};

static MenuItem items_mat[] = {
    {MENU_MAT_LIQUID, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_MatLiquid, NULL, NULL},
    {MENU_MAT_SOLID, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_MatSolid, NULL, NULL},
    {MENU_MAT_MICRO_DK, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_MatMicroDK, NULL, NULL}};

static MenuItem items_mat_liquid[] = {
    {MENU_MAT_LIQUID_FAST_CHANGE, MENU_SELECT, &gRadarParam.matFastChange, 0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_FAST_CHANGE + 1},
    {MENU_MAT_LIQUID_FIRST_WAVE, MENU_SELECT, &gRadarParam.matFirstWave,  0, 4, 1, dict_wave, 5, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_FIRST_WAVE + 1},
    {MENU_MAT_LIQUID_SURF_ANGLE, MENU_SELECT, &gRadarParam.matSurfAngle,  0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_SURF_ANGLE + 1},
    {MENU_MAT_LIQUID_FOAM, MENU_SELECT, &gRadarParam.matFoamDust,   0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_FOAM_DUST + 1},
    {MENU_MAT_LIQUID_SMALL_DK, MENU_SELECT, &gRadarParam.matSmallDK,    0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_SMALL_DK + 1},
    {MENU_MAT_LIQUID_PIPE, MENU_SELECT, &gRadarParam.matPipe, 0, 1, 1, dict_bool, 2, NULL, NULL, &Page_Pipe, NULL, NULL,
     RENDER_PIPE, VAL_UINT8, NULL, NULL, DPARAM_MAT_PIPE + 1}};

static MenuItem items_mat_solid[] = {
    {MENU_MAT_SOLID_FAST_CHANGE, MENU_SELECT, &gRadarParam.matFastChange, 0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_FAST_CHANGE + 1},
    {MENU_MAT_SOLID_FIRST_WAVE, MENU_SELECT, &gRadarParam.matFirstWave,  0, 4, 1, dict_wave, 5, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_FIRST_WAVE + 1},
    {MENU_MAT_SOLID_SURF_ANGLE, MENU_SELECT, &gRadarParam.matSurfAngle,  0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_SURF_ANGLE + 1},
    {MENU_MAT_SOLID_DUST, MENU_SELECT, &gRadarParam.matFoamDust,   0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_FOAM_DUST + 1},
    {MENU_MAT_SOLID_SMALL_DK, MENU_SELECT, &gRadarParam.matSmallDK,    0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_SMALL_DK + 1},
    {MENU_MAT_SOLID_PIPE, MENU_SELECT, &gRadarParam.matPipe, 0, 1, 1, dict_bool, 2, NULL, NULL, &Page_Pipe, NULL, NULL,
     RENDER_PIPE, VAL_UINT8, NULL, NULL, DPARAM_MAT_PIPE + 1}};

static MenuItem items_mat_microdk[] = {
    {MENU_MAT_MICRO_FAST_CHANGE, MENU_SELECT, &gRadarParam.matFastChange, 0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_FAST_CHANGE + 1},
    {MENU_MAT_MICRO_FIRST_WAVE, MENU_SELECT, &gRadarParam.matFirstWave,  0, 4, 1, dict_wave, 5, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_FIRST_WAVE + 1},
    {MENU_MAT_MICRO_SURF_ANGLE, MENU_SELECT, &gRadarParam.matSurfAngle,  0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_SURF_ANGLE + 1},
    {MENU_MAT_MICRO_NONE, MENU_READONLY, NULL, 0, 0, 0, NULL, 0, NULL, NULL, NULL, NULL, NULL},
    {MENU_MAT_MICRO_SMALL_DK, MENU_SELECT, &gRadarParam.matSmallDK,    0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL, 0, 0, NULL, NULL, DPARAM_MAT_SMALL_DK + 1},
    {MENU_MAT_MICRO_PIPE, MENU_SELECT, &gRadarParam.matPipe, 0, 1, 1, dict_bool, 2, NULL, NULL, &Page_Pipe, NULL, NULL,
     RENDER_PIPE, VAL_UINT8, NULL, NULL, DPARAM_MAT_PIPE + 1}};

static MenuItem items_pipe[] = {
    {MENU_PIPE_DIAMETER, MENU_FLOAT, &gRadarParam.pipeDiameter, 0, 1000, 1, NULL, 0, "%04.0f", "mm", NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_PIPE_DIAMETER + 1},
};

static MenuItem items_display[] = {
    {MENU_DISP_CONTENT, MENU_SELECT, &gRadarParam.dispContent, 0, 4, 1, dict_disp, 5, NULL, NULL, NULL, NULL, NULL},
    {MENU_DISP_CONTRAST, MENU_FLOAT,  &gRadarParam.lcdContrast, 0, 100, 1, NULL, 0, "%03.0f", NULL, NULL, NULL, NULL}};

static MenuItem items_diag[] = {
    {MENU_DIAG_PEAK, MENU_READONLY, NULL, 0, 0, 0, NULL, 0, NULL, NULL, NULL, NULL, CustomFormat_peakMinMaxEmpty,
     RENDER_PEAK, VAL_FLOAT, NULL, NULL},
    {MENU_DIAG_STATUS, MENU_READONLY, NULL, 0, 0, 0, NULL, 0, NULL, NULL, NULL, NULL, CustomFormat_diag_status,
     RENDER_STATUS, VAL_UINT8, NULL, NULL},
    /* 曲线选择：三条曲线（回波/虚假回波/输出走势）均有真实数据源；
     * 输出走势由本板自攒距离历史生成，主控与协议无需参与 */
    {MENU_DIAG_CURVE_SEL, MENU_SELECT, &gRadarParam.diagCurveSel, 0, 2, 1, dict_curve, 3, NULL, NULL, NULL, NULL, NULL},
    {MENU_DIAG_CURVE, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Curve, NULL, NULL},
    {MENU_DIAG_SIM, MENU_SELECT, &gRadarParam.diagSim, 0, 2, 1, dict_sim, 3, NULL, NULL, NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_DIAG_SIM + 1}};

static MenuItem items_curve[] = {
    {MENU_CURVE_ZOOM_DIR, MENU_SELECT, &gRadarParam.diagCurveZoom, 0, 2, 1, dict_zoomDir, 3, NULL, NULL, NULL, NULL, NULL},
    {MENU_CURVE_ZOOM_SCALE, MENU_SELECT, &gRadarParam.diagCurveScale, 0, 3, 1, dict_zoomScale, 4, NULL, NULL, NULL, NULL, NULL}};

static MenuItem items_service[] = {
    {MENU_SERV_FALSE_ECHO, MENU_SELECT, &gRadarParam.servFalseEcho, 0, 3, 1, dict_flsEcho, 4, NULL, NULL, NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_SERV_FALSE_ECHO + 1},
    {MENU_SERV_CURRENT, MENU_PAGE,   NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Current, NULL, CustomFormat_serv_current, RENDER_SERV_CURRENT, VAL_STRING, NULL, NULL},
    {MENU_SERV_RESET, MENU_SELECT, &gRadarParam.servReset, 0, 2, 1, dict_reset, 3, NULL, NULL, NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_SERV_RESET + 1},
    {MENU_SERV_UNIT, MENU_SELECT, &gRadarParam.servUnit, 0, 1, 1, dict_unit, 2, NULL, NULL, NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_SERV_UNIT + 1},
    {MENU_SERV_LANG, MENU_SELECT, &gRadarParam.servLang, 0, 3, 1, dict_lang, 4, NULL, NULL, NULL, NULL, NULL},
    {MENU_SERV_HART, MENU_PAGE,   NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_HART, NULL, CustomFormat_serv_hartaddr, RENDER_SERV_HARTADDR, VAL_STRING, NULL, NULL},
    {MENU_SERV_COPY_DATA, MENU_SELECT, &gRadarParam.servCopyData, 0, 1, 1, dict_copy, 2, NULL, NULL, NULL, NULL, NULL},
    {MENU_SERV_PWD, MENU_SELECT, &gRadarParam.servPwdEn, 0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
    {MENU_SERV_OFFSET, MENU_FLOAT,  &gRadarParam.servOffset, -10, 10, 0.01, NULL, 0, "%+0.2f", "m(d)", NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_SERV_OFFSET + 1},
    {MENU_SERV_THRESH, MENU_PAGE,   NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Thresh, NULL, CustomFormat_serv_thresh, RENDER_SERV_THRESH, VAL_STRING, NULL, NULL}};

static MenuItem items_current[] = {
    {MENU_CURRENT_MODE, MENU_SELECT, &gRadarParam.currMode,  0, 1, 1, dict_currMode, 2, NULL, NULL, NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_CURR_MODE + 1},
    /* max=2：dict_faultMode 每语言 3 档，索引 0..2。
     * ⚠ max 是"业务允许选到哪"的声明值，不可设成 3（max+1=4 > 3 档，直接索引即越界）。 */
    {MENU_CURRENT_FAULT, MENU_SELECT, &gRadarParam.currFault, 0, 2, 1, dict_faultMode, 3, NULL, NULL, NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_CURR_FAULT + 1},
    {MENU_CURRENT_MIN, MENU_SELECT, &gRadarParam.currMin,   0, 1, 1, dict_minCurr, 2, NULL, NULL, NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_CURR_MIN + 1}};

static MenuItem items_hart[] = {
    {MENU_HART_MODE, MENU_SELECT, &gRadarParam.servHART, 0, 1, 1, dict_hart, 2, NULL, NULL, &Page_HARTAddr, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_SERV_HART + 1}};

static MenuItem items_hart_addr[] = {
    {MENU_HART_ADDR, MENU_FLOAT, &gRadarParam.servHARTAddr, 0, 15, 1, NULL, 0, "%02.0f", NULL, NULL, NULL, NULL,
     RENDER_SERV_HARTADDR, VAL_UINT8, NULL, NULL, DPARAM_SERV_HART_ADDR + 1}};

static MenuItem items_thresh[] = {
    {MENU_THRESH_ECHO, MENU_FLOAT, &gRadarParam.threshEcho, 0, 200, 1, NULL, 0, "%02.0f", "mV", NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_THRESH_ECHO + 1},
    {MENU_THRESH_ENV, MENU_FLOAT,  &gRadarParam.threshEnv,  0, 100, 1, NULL, 0, "%02.0f", "mV", NULL, NULL, NULL,
     0, 0, NULL, NULL, DPARAM_THRESH_ENV + 1}};

/* --- 页面实体打包 --- */
#define PACK_PAGE(var, title_id, p_type, arr, flatten) \
    MenuPage var = {title_id, p_type, flatten, arr, sizeof(arr) / sizeof(arr[0])}

PACK_PAGE(Page_Root,       PAGE_TITLE_ROOT,       PAGE_TYPE_ROOT,  items_root,        0);
PACK_PAGE(Page_Basic,      PAGE_TITLE_BASIC,      PAGE_TYPE_PARAM, items_basic,       0);
PACK_PAGE(Page_Mat,        PAGE_TITLE_MAT,        PAGE_TYPE_MENU,  items_mat,         0);
PACK_PAGE(Page_MatLiquid,  PAGE_TITLE_MAT_LIQUID, PAGE_TYPE_PARAM, items_mat_liquid,  1);
PACK_PAGE(Page_MatSolid,   PAGE_TITLE_MAT_SOLID,  PAGE_TYPE_PARAM, items_mat_solid,   1);
PACK_PAGE(Page_MatMicroDK, PAGE_TITLE_MAT_MICRO_DK,PAGE_TYPE_PARAM, items_mat_microdk, 1);
PACK_PAGE(Page_Pipe,       PAGE_TITLE_PIPE,       PAGE_TYPE_PARAM, items_pipe,        1);
PACK_PAGE(Page_Display,    PAGE_TITLE_DISPLAY,    PAGE_TYPE_PARAM, items_display,     0);
PACK_PAGE(Page_Diag,       PAGE_TITLE_DIAG,       PAGE_TYPE_PARAM, items_diag,        0);
PACK_PAGE(Page_Curve,      PAGE_TITLE_CURVE,      PAGE_TYPE_PARAM, items_curve,       0);
PACK_PAGE(Page_Service,    PAGE_TITLE_SERVICE,    PAGE_TYPE_PARAM, items_service,     0);
PACK_PAGE(Page_Current,    PAGE_TITLE_CURRENT,    PAGE_TYPE_PARAM, items_current,     0);
PACK_PAGE(Page_HART,       PAGE_TITLE_HART,       PAGE_TYPE_PARAM, items_hart,        0);
PACK_PAGE(Page_HARTAddr,   PAGE_TITLE_HART_ADDR,  PAGE_TYPE_PARAM, items_hart_addr,   1);
PACK_PAGE(Page_Thresh,     PAGE_TITLE_THRESH,     PAGE_TYPE_PARAM, items_thresh,      0);

/* --- 菜单栈机制 --- */
typedef struct
{
    MenuPage *page;
    uint8_t index;
} NavState;

#define MAX_NAV_DEPTH 6
static NavState navStack[MAX_NAV_DEPTH];
static int8_t navDepth = 0;

static MenuPage *curPage = &Page_Root;
static uint8_t curIndex = 0;
static uint8_t topIndex = 0;

/**
 * @brief  生成当前位置的菜单编号（如 "2-3-1"），写进 buf
 * @param  buf 输出缓冲（调用方保证 ≥16 字节）
 * @retval 无
 */
static void Get_Menu_Code_String(char *buf)
{
    buf[0] = '\0';
    char tmp[12];

    if (curPage->type == PAGE_TYPE_ROOT)
    {
        snprintf(buf, sizeof(buf), "%d", (curIndex / 4) + 1);
        return;
    }
    int limit = curPage->flatten_nav ? (navDepth - 1) : navDepth;
    if (limit > 2) limit = 2;

    for (int i = 0; i < limit; i++)
    {
        snprintf(tmp, sizeof(tmp), "%d-", navStack[i].index + 1);
        strcat(buf, tmp);
    }

    if (curPage->type == PAGE_TYPE_MENU)
    {
        if (strlen(buf) > 0)
            buf[strlen(buf) - 1] = '\0';
    }
    else
    {
        snprintf(tmp, sizeof(tmp), "%d", curIndex + 1);
        strcat(buf, tmp);
    }
}

/**
 * @brief  把菜单项当前值按 formatStr 格式化成编辑串 editStr，光标归位
 * @param  item 菜单项（valueType 决定取值方式）
 * @retval 无
 */
static void Float_To_EditStr(MenuItem *item)
{
    if (!item || !item->value) return;
    float v;
    if (item->valueType == VAL_UINT8)
        v = (float)(*((uint8_t *)item->value));
    else if (item->valueType == VAL_FLOAT || item->valueType == 0)
        v = *((float *)item->value);
    else
        return;
    
    if (item->formatStr)
        snprintf(editStr, sizeof(editStr), item->formatStr, v);
    else
        snprintf(editStr, sizeof(editStr), "%.2f", v);
    editCursorPos = 0;
}

/**
 * @brief  把编辑串解析回数值并按 min/max 钳位后写回菜单项
 * @param  item 菜单项
 * @retval 无
 */
static void Save_EditStr_To_Float(MenuItem *item)
{
    if (!item || !item->value) return;
    float tempVal = (float)atof(editStr);
    if (tempVal < item->min) tempVal = item->min;
    if (tempVal > item->max) tempVal = item->max;
    
    if (item->valueType == VAL_UINT8)
        *((uint8_t *)item->value) = (uint8_t)tempVal;
    else if (item->valueType == VAL_FLOAT || item->valueType == 0)
        *((float *)item->value) = tempVal;
}

/**
 * @brief  浮点项编辑：key 1=改当前位 2=移光标 3=确认（主值→附带值→上报）
 * @param  item 菜单项
 * @param  key  按键码 1/2/3
 * @retval 无
 */
static void Edit_Float(MenuItem *item, uint8_t key)
{
    if (editCursorPos >= sizeof(editStr) - 1)
        editCursorPos = 0;

    if (key == 1)
    {
        char ch = editStr[editCursorPos];
        if (ch >= '0' && ch <= '9')
        {
            if (++ch > '9') ch = '0';
            editStr[editCursorPos] = ch;
        }
        else if (ch == '+' || ch == '-')
            editStr[editCursorPos] = (ch == '+') ? '-' : '+';
    }
    else if (key == 2)
    {
        editCursorPos++;
        if (editCursorPos >= sizeof(editStr) - 1 || editStr[editCursorPos] == '\0' || editStr[editCursorPos] == '%')
            editCursorPos = 0;
        if (editStr[editCursorPos] == '.')
        {
            editCursorPos++;
            if (editCursorPos >= sizeof(editStr) - 1 || editStr[editCursorPos] == '\0' || editStr[editCursorPos] == '%')
                editCursorPos = 0;
        }
    }
    else if (key == 3)
    {
        if (editTarget == EDIT_MAIN)
        {
            Save_EditStr_To_Float(item);
            if (item->exValue)
            {
                editTarget = EDIT_EXTRA;
                snprintf(editStr, sizeof(editStr), item->exFormat ? item->exFormat : "%.3f",
                        *(float *)item->exValue);
                editCursorPos = 0;
            }
            else
            {
                gUIState = UI_BROWSE;
                editTarget = EDIT_MAIN;
                /* 上报主板：主浮点参数（paramId=DISP_PARAM_ID+1，0=不上报） */
                if (item->paramId != 0u)
                    Disp_SendSetParam((uint8_t)(item->paramId - 1u), *(float *)item->value);
                /* ⚠ 扩展参数（如低位调整的距离端点）必须与主值一并上报：否则主板侧
                 *   lowAdjustVal/highAdjustVal 恒为 0、span=0，4-20mA 会恒输出满量程。 */
                if (item->exValue != NULL && item->exParamId != 0u)
                    Disp_SendSetParam((uint8_t)(item->exParamId - 1u), *(float *)item->exValue);
            }
        }
        else
        {
            float tempVal = (float)atof(editStr);
            if (tempVal < item->min) tempVal = item->min;
            if (tempVal > item->max) tempVal = item->max;
            *(float *)item->exValue = tempVal;
            gUIState = UI_BROWSE;
            editTarget = EDIT_MAIN;
            /* 上报主板：主浮点参数 + 扩展浮点参数（两者都要发） */
            if (item->paramId != 0u)
                Disp_SendSetParam((uint8_t)(item->paramId - 1u), *(float *)item->value);
            if (item->exValue != NULL && item->exParamId != 0u)
                Disp_SendSetParam((uint8_t)(item->exParamId - 1u), *(float *)item->exValue);
        }
    }
}

/**
 * @brief  字符串项编辑：key 1=换字母 2=移光标 3=确认并上报
 * @param  item 菜单项
 * @param  key  按键码 1/2/3
 * @retval 无
 */
static void Edit_String(MenuItem *item, uint8_t key)
{
    if (editCursorPos >= sizeof(editStr) - 1)
        editCursorPos = 0;

    if (key == 1)
    {
        char ch = editStr[editCursorPos];
        if (ch >= 'A' && ch <= 'Z')
        {
            if (++ch > 'Z') ch = 'A';
        }
        else
            ch = 'A';
        editStr[editCursorPos] = ch;
    }
    else if (key == 2)
    {
        editCursorPos++;
        if (editCursorPos >= sizeof(editStr) - 1 || editStr[editCursorPos] == '\0')
            editCursorPos = 0;
    }
    else if (key == 3)
    {
        strcpy((char *)item->value, editStr);
        gUIState = UI_BROWSE;
        /* 上报主板：字符串参数 */
        if (item->paramId != 0u)
            Disp_SendStr((uint8_t)(item->paramId - 1u), (const char *)item->value);
    }
}

/**
 * @brief  选择项编辑：key 1/2=循环取值 3=确认（上报并按需进入子页）
 * @param  item 菜单项
 * @param  key  按键码 1/2/3
 * @retval 无
 */
static void Edit_Select(MenuItem *item, uint8_t key)
{
    uint8_t *v = (uint8_t *)item->value;
    /* 可选上限 = min(字典条目数, max+1)。
     * ⚠ optionNum 是"字典里有几条"，max 才是"业务允许选到哪"，二者不可互相替代 ——
     *   只用 optionNum 循环会选到不该出现的项。 */
    uint8_t limit = (uint8_t)(item->max + 1.0f);
    if (limit == 0u || limit > item->optionNum)
        limit = item->optionNum;

    if (key == 1 || key == 2)
    {
        if (++(*v) >= limit)
            *v = 0;
    }
    else if (key == 3)
    {
        gUIState = UI_BROWSE;
        /* 上报主板：选择型参数（u8 -> float） */
        if (item->paramId != 0u)
            Disp_SendSetParam((uint8_t)(item->paramId - 1u), (float)(*v));
        if (item->value == &gRadarParam.servHART && *v == 0)    //hart标准模式，地址默认0
        {
            gRadarParam.servHARTAddr = 0;
        }
        else if (item->subPage && *v == 1 && navDepth < MAX_NAV_DEPTH)
        {
            navStack[navDepth].page = curPage;
            navStack[navDepth].index = curIndex;
            navDepth++;
            curPage = item->subPage;
            curIndex = 0;
            topIndex = 0;
        }
    }
}

/**
 * @brief  按菜单项类型分派编辑按键
 * @param  item 菜单项
 * @param  key  按键码
 * @retval 无
 */
static void Process_Edit_Key(MenuItem *item, uint8_t key)
{
    switch (item->type)
    {
    case MENU_FLOAT:
        Edit_Float(item, key);
        break;
    case MENU_STRING:
        Edit_String(item, key);
        break;
    case MENU_SELECT:
        Edit_Select(item, key);
        break;
    default:
        break;
    }
}

/**
 * @brief  格式化菜单项显示值（优先自定义回调，否则按类型格式化到 valBuf/ex1/ex2）
 * @param  item   菜单项
 * @param  valBuf 主值输出
 * @param  ex1    附带值1输出
 * @param  ex2    附带值2输出
 * @retval 无
 */
static void UI_FormatItemValue(MenuItem *item, char *valBuf, char *ex1, char *ex2)
{
    if (item->customFormat != NULL)
    {
        item->customFormat(item, valBuf, ex1, ex2);
        return;
    }

    if (item->type == MENU_SELECT)
    {
        uint8_t v = *((uint8_t *)item->value);
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;   /* 语言索引钳位 */
        if (item->optionStr && v < item->optionNum && lang < 4)
            snprintf(valBuf, CUSTOM_BUF_SIZE, "%s", item->optionStr[lang * item->optionNum + v]);
        return;
    }

    if (item->type == MENU_FLOAT || item->type == MENU_READONLY)
    {
        float v;
        if (item->valueType == VAL_UINT8)
            v = (float)(*((uint8_t *)item->value));
        else
            v = *((float *)item->value);

        /* NaN / Inf 过滤：非法浮点统一显示为 0 */
        if (!(v >= -1.0e30f && v <= 1.0e30f)) v = 0.0f;

        if (item->formatStr)
            snprintf(valBuf, CUSTOM_BUF_SIZE, item->formatStr, v);
        else
            snprintf(valBuf, CUSTOM_BUF_SIZE, "%.2f", v);

        if (item->unitStr)
        {
            strcat(valBuf, " ");
            strcat(valBuf, item->unitStr);
        }
        return;
    }

    if (item->type == MENU_STRING)
    {
        snprintf(valBuf, CUSTOM_BUF_SIZE, "%s", (char *)item->value);
        return;
    }
}

/**
 * @brief  按 renderMode 把值画到屏幕（含编辑光标）
 * @param  item      菜单项
 * @param  valBuf    主值字符串
 * @param  ex1       附带值1字符串
 * @param  ex2       附带值2字符串
 * @param  isEditing 1=编辑态（画反显光标）
 * @retval 无
 */
static void UI_RenderByMode(MenuItem *item, const char *valBuf,
                            const char *ex1, const char *ex2, uint8_t isEditing)
{
    switch (item->renderMode)
    {
    case RENDER_PIPE:
        if (*(uint8_t *)item->value == 1)
        {
            LCD_ShowStrExCompact(COL_CENTER_X, 2, (uint8_t *)valBuf);
            LCD_ShowArrowEx(2, 3, COL_CENTER_X, (char *)valBuf);
            LCD_ShowStrExCompact(COL_VALUE_X + 16, 4, (uint8_t *)UI_GetText(TEXT_PIPE_DIAMETER_LABEL));
            char pipeBuf[16];
            snprintf(pipeBuf, sizeof(pipeBuf), "%.0f %s", gRadarParam.pipeDiameter, UI_GetText(TEXT_PIPE_DIAMETER_UNIT));
            LCD_ShowStrExCompact(COL_VALUE_X + 16, 6, (uint8_t *)pipeBuf);
        }
        else
        {
            LCD_ShowStrExCompact(COL_CENTER_X, 2, (uint8_t *)valBuf);
            LCD_ShowArrowEx(2, 3, COL_CENTER_X, (char *)valBuf);
        }
        break;

    case RENDER_PEAK:
    case RENDER_STATUS:
        LCD_ShowStrExCompact(COL_VALUE_X, 2, (uint8_t *)valBuf);
        if (strlen(ex1) > 0)
            LCD_ShowStrExCompact(COL_VALUE_X, 4, (uint8_t *)ex1);
        break;

    case RENDER_SERV_CURRENT:
        LCD_ShowStrExCompact(COL_VALUE_X, 2, (uint8_t *)valBuf);
        LCD_ShowArrowEx(2, 0, 0, NULL);
        if (strlen(ex1) > 0)
            LCD_ShowStrExCompact(COL_VALUE_X, 4, (uint8_t *)ex1);
        LCD_ShowArrowEx(4, 0, 0, NULL);
        if (strlen(ex2) > 0)
            LCD_ShowStrExCompact(COL_VALUE_X, 6, (uint8_t *)ex2);
        LCD_ShowArrowEx(6, 0, 0, NULL);
        break;

    case RENDER_SERV_HARTADDR:
        if(navDepth == 3){
            LCD_ShowStrEx(COL_CENTER_X, 2, (uint8_t *)valBuf); //地址设置
        }
        else{
            LCD_ShowStrExCompact(COL_CENTER_X, 2, (uint8_t *)valBuf);
        }
        
        LCD_ShowStrExCompact(COL_CENTER_X, 4, (uint8_t *)ex1);
        break;

    case RENDER_SERV_THRESH:
        LCD_ShowStrExCompact(COL_MENU_ITEM, 2, (uint8_t *)valBuf);
        LCD_ShowArrowEx(2, 0, 0, NULL);
        LCD_ShowStrExCompact(COL_MENU_ITEM, 4, (uint8_t *)ex1);
        LCD_ShowArrowEx(4, 0, 0, NULL);
        break;
        

    case RENDER_DEFAULT:
    default:
        // SELECT和BOOL类型使用紧凑显示，其他类型使用默认显示
        if (item->type == MENU_SELECT || item->type == MENU_BOOL) {
            LCD_ShowStrExCompact(COL_CENTER_X, 2, (uint8_t *)valBuf);
            if (strlen(ex1) > 0)
                LCD_ShowStrExCompact(COL_CENTER_X, 4, (uint8_t *)ex1);
            if (strlen(ex2) > 0)
                LCD_ShowStr_Small(COL_CENTER_X + 6, 6, ex2);

            if (item->type == MENU_SELECT)
            {
                LCD_ShowArrowEx(2, 3, COL_CENTER_X, (char *)valBuf);
            }
        } else {
            LCD_ShowStrEx(COL_CENTER_X, 2, (uint8_t *)valBuf);
            if (strlen(ex1) > 0)
                LCD_ShowStrEx(COL_CENTER_X, 4, (uint8_t *)ex1);
            if (strlen(ex2) > 0)
                LCD_ShowStr_Small(COL_CENTER_X + 6, 6, ex2);

            if (item->type == MENU_SELECT)
            {
                LCD_ShowArrowEx(2, 3, COL_CENTER_X, (char *)valBuf);
            }
        }
        break;
    }

    if (isEditing)
    {
        uint8_t cursorCol = 30 + editCursorPos * 8;
        uint8_t cursorPage = (editTarget == EDIT_MAIN) ? 2 : 4;
        if (editCursorPos < strlen(editStr))
            LCD_ShowCharReverse(cursorCol, cursorPage, editStr[editCursorPos]);
    }
}

/**
 * @brief  画首页：view_mode 0 = 正常读数页，1 = 曲线页
 * @retval 无
 */
static void UI_RenderHome(void)
{
    char val[24];

    if (home_view_mode == 0)
    {
        /* 版式：col 0..15【预留】物位显示区（当前整条填满）；逻辑页 0..3 大字读数（8x16 字模 2 倍放
         * 大为 16x32，在 col 16..127 内居中）；逻辑页 4..5 单位；逻辑页 6..7 传感器标签。
         * ⚠ measPeakCount==0 即本轮无峰、realTimeDistance 无意义，须显式打 "----"；此处字符串不带单位 */
        /* 左侧预留条：一整条亮块（逻辑 col 0..15 × 全部 8 页），0xFF = 亮块（负显）。
         * ⚠ 必须先填后画：绘制只点亮字模为 1 的像素、不写背景，反序会被亮块整条压掉 */
        LCD_FillBlock(0u, 0u, (uint8_t)COL_HOME_LEFT_RESERVE, 8u,
                      (uint8_t)HOME_LEFT_FILL_BYTE);

        if (gRadarParam.measPeakCount > 0u)
            snprintf(val, sizeof(val), "%.3f", gRadarParam.realTimeDistance);
        else
            snprintf(val, sizeof(val), "----");

        /* 大字读数：在「预留区右缘 ~ 屏幕右缘」之间居中。
         * ⚠ 量宽必须用 LCD_GetStrBigWidth（与绘制推进量同口径），不可用 LCD_GetStrCompactWidth
         *   （后者对数字固定按 9 列计，与实际墨迹宽度不等，对齐会偏）。 */
        uint16_t bigW  = LCD_GetStrBigWidth(val);
        uint16_t avail = (uint16_t)(LCD_W - COL_HOME_LEFT_RESERVE);
        uint8_t  bigCol = (bigW <= avail)
                          ? (uint8_t)(COL_HOME_LEFT_RESERVE + (avail - bigW) / 2u)
                          : (uint8_t)COL_HOME_LEFT_RESERVE;
        LCD_ShowStrExBig(bigCol, 0, val);

        /* 单位：只画裸单位（m / ft），右缘对齐读数的【墨迹】右缘；servUnit 越界时按 m 处理。
         * ⚠ 必须逐字符调用 LCD_ShowCharCompact，不可用 LCD_ShowStrExCompact：后者对每字符
         *   先判 "col + 12 > LCD_W"，成立即 col=0、page+=2 —— 单位靠右时起始列 >116 必中，
         *   会被甩到大行4 左侧盖掉传感器标签。 */
        const char *unitStr = (gRadarParam.servUnit == 1u) ? "ft" : "m";
        uint16_t unW = LCD_GetStrExWidth(unitStr);
        /* bigW 是推进量（每字符都含 2 列字距），末字墨迹右缘在 bigCol+bigW-2；
         * 直接对齐到 bigCol+bigW 会让单位外挂 2 列，与读数右缘错开。 */
        uint8_t  numRight = (uint8_t)(bigCol + ((bigW >= 2u) ? (bigW - 2u) : bigW));
        uint8_t  unCol = (numRight >= unW) ? (uint8_t)(numRight - (uint8_t)unW) : 0u;
        for (const char *p = unitStr; *p != '\0'; p++)
            unCol = (uint8_t)(unCol + LCD_ShowCharCompact(unCol, 4, (uint8_t)*p));

        /* 传感器标签：大行4，sensorTag 由 PARAM_DUMP 下发（16 字节，已补 '\0'），空串则整行留空。
         * ⚠ 起始列取 COL_HOME_LEFT_RESERVE 而非 0：预留条要留成干净的一整条，标签压进去会打架。 */
        if (gRadarParam.sensorTag[0] != '\0')
            LCD_ShowStrExCompact((uint8_t)COL_HOME_LEFT_RESERVE, 6,
                                 (uint8_t *)gRadarParam.sensorTag);
    }
    else
    {                                                                              //快捷键
        /* 按 diagCurveSel 取【真正对应】的曲线缓冲 */
        const uint8_t *curve = UI_GetSelectedCurve();
        uint8_t curveSel = gRadarParam.diagCurveSel;
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;   /* 语言索引钳位 */

        /* ⚠ 越界保护：diagCurveSel 存 EEPROM，可能残留过大的旧值，直接索引 dict_curve 会读到别的语言段 */
        if (curveSel >= 3u) curveSel = 0u;

        /* ⚠ 必须先画曲线、再画标题与数值：LCD_DrawEchoCurve() 内部会 memset(frame_buffer,0)
         *   并整屏刷入（见 oled.c），会把之前画的内容全部抹掉。 */
        if (curve != NULL)
            LCD_DrawEchoCurve((uint8_t *)curve, 1, 1);
        else
            LCD_DrawEchoCurve(NULL, 1, 1);   /* 只画坐标轴，不画波形：明确表示"无数据" */

        LCD_ShowStrExCompact(0, 0, (uint8_t *)dict_curve[lang * 3 + curveSel]);

        /* 右上角页码 "3-4" 保留硬编码（曲线视图 = 诊断→选择曲线→曲线显示 的固定入口）。
         * ⚠ 不可改为动态计算：Get_Menu_Code_String() 依赖 curPage/curIndex/navDepth，而退出到
         *   首页时这三者都不复位（K3 从浏览态返回只置 gUIState = UI_HOME），算出来是上次
         *   逛到哪的残留页码，与曲线页无关。 */
        LCD_ShowStr_Small(110, 0, "3-4");
    }
}

/**
 * @brief  画菜单列表页（根菜单或子菜单），左侧箭头指示当前项
 * @param  codeBuf 右上角菜单编号
 * @retval 无
 */
static void UI_RenderMenuList(const char *codeBuf)
{
    LCD_ShowStr_Small(128 - strlen(codeBuf) * 6, 0, codeBuf);

    if (curPage->type == PAGE_TYPE_MENU)
    {
        LCD_ShowStrExCompact(0, 0, (uint8_t *)UI_GetText(curPage->titleId));
        for (uint8_t i = 0; i < 3 && i < curPage->itemCount; i++)
        {
            if (i == curIndex)
                LCD_ShowArrowEx((i + 1) * 2, 1, 0, NULL);
            char nameBuf[32];
            snprintf(nameBuf, sizeof(nameBuf), " %s", UI_GetText(curPage->items[i].id));
            LCD_ShowStrExCompact(10, (i + 1) * 2, (uint8_t *)nameBuf);
        }
    }
    else
    {
        for (uint8_t i = 0; i < 4; i++)
        {
            if (topIndex + i >= curPage->itemCount) break;
            if ((topIndex + i) == curIndex)
                LCD_ShowArrowEx(i * 2, 1, 0, NULL);
            char nameBuf[32];
            snprintf(nameBuf, sizeof(nameBuf), " %s", UI_GetText(curPage->items[topIndex + i].id));
            LCD_ShowStrExCompact(10, i * 2, (uint8_t *)nameBuf);
        }
    }
}

/**
 * @brief  画参数详情页（标题 + 编号 + 值 + 特殊页处理 + 编辑光标）
 * @param  codeBuf 右上角菜单编号
 * @retval 无
 */
static void UI_RenderParamItem(const char *codeBuf)
{
    MenuItem *item = &curPage->items[curIndex];
    char titleBuf[32], valBuf[32] = {0}, ex1[32] = {0}, ex2[32] = {0};

    // --- 标题 + 导航号 ---
    snprintf(titleBuf, sizeof(titleBuf), "%s", UI_GetText(item->id));
    LCD_ShowStrExCompact(0, 0, (uint8_t *)titleBuf);
    LCD_ShowStr_Small(128 - strlen(codeBuf) * 6, 0, codeBuf);

    // --- 特殊: 基本设置页显示当前物料性质记忆值 ---
    if (curPage == &Page_Basic && item->subPage == &Page_Mat)
    {
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;   /* 语言索引钳位 */
        /* ⚠ matType 存 EEPROM，可能残留越界值；dict_mat 每语言 3 档，不钳位会跨语言段读出错误文本。 */
        uint8_t mt = (gRadarParam.matType > 2u) ? 2u : gRadarParam.matType;
        const char *matName = dict_mat[lang * 3 + mt];
        LCD_ShowStrExCompact(30, 2, (uint8_t *)matName);
        LCD_ShowArrowEx(2, 3, 30, (char *)matName);  // 使用mode=3自动计算宽度
    }

    if (curPage == &Page_Pipe)
    {
        LCD_ShowStrExCompact(30, 2, (uint8_t *)UI_GetText(TEXT_PIPE_DIAMETER_LABEL));
        char pipeBuf[16];
        if (gUIState == UI_EDIT)
        {
            strcpy(pipeBuf, editStr);
            strcat(pipeBuf, " ");
            strcat(pipeBuf, UI_GetText(TEXT_PIPE_DIAMETER_UNIT));
        }
        else
        {
            snprintf(pipeBuf, sizeof(pipeBuf), "%04.0f %s", gRadarParam.pipeDiameter, UI_GetText(TEXT_PIPE_DIAMETER_UNIT));
        }
        LCD_ShowStrEx(30, 4, (uint8_t *)pipeBuf);

        if (gUIState == UI_EDIT && editCursorPos < strlen(editStr))
        {
            LCD_ShowCharReverse(30 + editCursorPos * 8, 4, editStr[editCursorPos]);
        }
        return;
    }

    if (curPage == &Page_Diag && curIndex == 3)
    {
        LCD_ClearLine(0);
		LCD_ClearLine(1);
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;   /* 语言索引钳位 */
        const char *dynamicName = dict_curve[lang * 3 + gRadarParam.diagCurveSel];
        LCD_ShowStrExCompact(0, 0, (uint8_t *)dynamicName);
        LCD_ShowStr_Small(128 - strlen(codeBuf) * 6, 0, codeBuf);
    }
    else
    {
        snprintf(titleBuf, sizeof(titleBuf), "%s",  UI_GetText(item->id));
        LCD_ShowStrExCompact(0, 0, (uint8_t *)titleBuf);

    }

    UI_FormatItemValue(item, valBuf, ex1, ex2);

    uint8_t isEditing = (gUIState == UI_EDIT && (item->type == MENU_FLOAT || item->type == MENU_STRING));
    if (isEditing)
    {
        if (editTarget == EDIT_MAIN)
        {
            strcpy(valBuf, editStr);
            if (item->unitStr)
            {
                strcat(valBuf, " ");
                strcat(valBuf, item->unitStr);
            }
            else if (item->customFormat != NULL && item->type == MENU_FLOAT)
                strcat(valBuf, " %");
        }
        else
        {
            strcpy(ex1, editStr);
            strcat(ex1, " m(d)");
        }
    }

    // --- SELECT 编辑态: 显示全选项网格 ---
    if (gUIState == UI_EDIT && item->type == MENU_SELECT)
    {
        uint8_t col = 0, row = 0;
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;   /* 语言索引钳位 */
        for (uint8_t i = 0; i < item->optionNum; i++)
        {
            uint8_t x = (col == 0) ? 10 : 72;
            uint8_t y = (row + 1) * 2;
            if (i == *((uint8_t *)item->value))
                LCD_ShowArrowEx(y, 2, x - 10, NULL);
            LCD_ShowStrExCompact(x, y, (uint8_t *)item->optionStr[lang * item->optionNum + i]);
            if (++row >= 3) { row = 0; col++; }
        }
    }
    else
    {
        UI_RenderByMode(item, valBuf, ex1, ex2, isEditing);
    }
}

/**
 * @brief  按页类型分派浏览态渲染
 * @param  codeBuf 右上角菜单编号
 * @retval 无
 */
static void UI_RenderBrowse(const char *codeBuf)
{
    if (curPage->type == PAGE_TYPE_ROOT || curPage->type == PAGE_TYPE_MENU)
    {
        UI_RenderMenuList(codeBuf);
    }
    else
    {
        UI_RenderParamItem(codeBuf);
    }
}

/**
 * @brief  按键总入口：按当前状态机分派（0=K3 返回、1=K4 上、2=K5 循环、3=K6 确认）
 * @param  key 按键码
 * @retval 无
 */
static void UI_ProcessKey(uint8_t key)
{
    if (gUIState == UI_HOME)
    {
        if (key == 0)
            home_view_mode = !home_view_mode;
        else if (key == 3 && !home_view_mode)
        {
            gUIState = UI_BROWSE;
            curPage = &Page_Root;
            curIndex = 0;
            topIndex = 0;
            navDepth = 0;
        }
    }
    else if (gUIState == UI_EDIT)
    {
        if (key == 0)
        {
            /* K3 取消：SELECT 编辑期已就地改了 *value，需回滚到进入时的快照；
             * FLOAT/STRING 改的是 editStr，随状态退出自然丢弃。 */
            MenuItem *item = &curPage->items[curIndex];
            if (item->type == MENU_SELECT && item->value != 0)
                *(uint8_t *)item->value = editSelectOrig;
            gUIState = UI_BROWSE;
            editTarget = EDIT_MAIN;
        }
        else
        {
            Process_Edit_Key(&curPage->items[curIndex], key);
        }
    }
    else if (gUIState == UI_BROWSE)
    {
        if (key == 0)
        {
            if (navDepth > 0)
            {
                navDepth--;
                curPage = navStack[navDepth].page;
                curIndex = navStack[navDepth].index;
                topIndex = (curPage->type == PAGE_TYPE_ROOT) ? (curIndex / 4) * 4 : 0;
            }
            else
            {
                gUIState = UI_HOME;
            }
        }
        else if (key == 1)
        {
            if (curIndex > 0)
            {
                curIndex--;
                if (curPage->type == PAGE_TYPE_ROOT && curIndex < topIndex)
                    topIndex--;
            }
        }
        else if (key == 2)
        {
            if (curIndex < curPage->itemCount - 1)
            {
                curIndex++;
                if (curPage->type == PAGE_TYPE_ROOT && curIndex >= topIndex + 4)
                    topIndex++;
            }
            else
            {
                curIndex = 0;
                topIndex = 0;
            }
        }
        else if (key == 3)
        {
            MenuItem *item = &curPage->items[curIndex];
            if (item->type == MENU_PAGE && item->subPage && navDepth < MAX_NAV_DEPTH)
            {
                if (curPage == &Page_Mat)
                    gRadarParam.matType = curIndex;

                navStack[navDepth].page = curPage;
                navStack[navDepth].index = curIndex;
                navDepth++;
                curPage = item->subPage;
                curIndex = 0;
                topIndex = 0;
            }
            else if (item->type == MENU_ACTION)
            {
                if (item->actionCallback)
                    item->actionCallback();
            }
            else if (item->type != MENU_READONLY)
            {
                /* 编辑前校验 value 指针有效性 */
                if (!item->value)
                    return;
                gUIState = UI_EDIT;
                editTarget = EDIT_MAIN;
                if (item->type == MENU_FLOAT)
                    Float_To_EditStr(item);
                else if (item->type == MENU_STRING)
                {
                    strncpy(editStr, (char *)item->value, 15);
                    editStr[15] = '\0';
                    editCursorPos = 0;
                }
                else if (item->type == MENU_SELECT)
                {
                    /* 快照原值供 K3 取消回滚（编辑期 K4/K5 就地修改 *value） */
                    editSelectOrig = *(uint8_t *)item->value;
                }
            }
        }
    }
    uiDirty = 1;
}

/* ============================================================
 * 公开 API
 * ============================================================ */
void UI_KeyK3_Back(void)  { UI_ProcessKey(0); }
void UI_KeyK4_Up(void)    { UI_ProcessKey(1); }
void UI_KeyK5_Loop(void)  { UI_ProcessKey(2); }
void UI_KeyK6_Enter(void) { UI_ProcessKey(3); }

/**
 * @brief  重算走势 Y 轴自适应范围（s_hist_min / s_hist_max）
 * @retval 无
 */
static void UI_RefreshTrendRange(void)
{
    /* ⚠ 必须由 UI_Task() 每轮【先】调用，保证返回后 s_hist_min/s_hist_max 是最新值 ——
     *   随后 UI_RenderHome()/UI_GetSelectedCurve() 读到的范围与曲线才一致。
     * 并发：接收中断仍会写 s_hist_head，该下标要么在读取范围外、要么是已回绕的旧槽位；
     *   32 位对齐读写不撕裂，最坏读到上一帧旧值，下轮纠正。 */
    uint8_t n, k;

    if (s_histRangeDirty == 0u) return;
    s_histRangeDirty = 0u;

    n = s_hist_valid;
    if (n == 0u) return;

    s_hist_min = s_hist_dist[0];
    s_hist_max = s_hist_dist[0];
    for (k = 1u; k < n; k++)
    {
        if (s_hist_dist[k] < s_hist_min) s_hist_min = s_hist_dist[k];
        if (s_hist_dist[k] > s_hist_max) s_hist_max = s_hist_dist[k];
    }
}

/* ============================================================
 * 写参数 ACK（0x07）结果提示
 *
 * 确认键只代表"帧发出去了"；主板 EEPROM 写失败、id 被拒、线路丢帧时界面原本仍显示新值，
 * 断电后却回退。有了 ACK 才谈得上双方都知道。实现上不新增页面，只在屏幕最底行（page 7）
 * 覆盖一行小字，1.5s 自动消失，每次全屏重绘后再补画一次，因此 MEAS 刷新不会把它吃掉。
 * ⚠ 文案用 ASCII：gLangTable 没有这几个条目，本地化需另补 4 种语言的枚举与字串。
 * ============================================================ */
#define ACK_TOAST_MS  1500u

static uint8_t  s_ack_toast_kind = 0u;    /* 0=不显示 1=成功 2=主板回失败 3=无应答 */
static uint32_t s_ack_toast_end  = 0u;    /* 到期时刻（HAL_GetTick 基准） */

/**
 * @brief  把 ACK 结果画在屏幕底行；已过期则清状态并请求整屏重绘
 * @retval 无
 */
static void UI_DrawAckToast(void)
{
    /* ⚠ 调用点放在每次 LCD_FullFill() + 渲染【之后】，这样它是最后绘制的、不会被版面覆盖 */
    const char *msg;

    if (s_ack_toast_kind == 0u) return;

    if ((int32_t)(HAL_GetTick() - s_ack_toast_end) >= 0)
    {
        s_ack_toast_kind = 0u;
        /* ⚠ 只清黑会在底行留下永久空白（原来那行内容已被 toast 覆盖掉）：
         *   置 uiDirty 让下一轮完整重绘把它补回来。toast_kind 已清 0，不会来回重绘。 */
        uiDirty = 1;
        return;
    }

    if (s_ack_toast_kind == 1u)      msg = "SAVED OK";
    else if (s_ack_toast_kind == 3u) msg = "NO REPLY";
    else                             msg = "SAVE FAIL";

    LCD_FillBlock(0, 7, LCD_W, 1, 0x00);
    LCD_ShowStr_Small(2, 7, msg);
}

/**
 * @brief  回波曲线 X 轴右端刻度随量程设定刷新（仅在值变化时做一次浮点格式化）
 * @retval 无
 */
static void UI_UpdateCurveAxisLabel(void)
{
    /* ⚠ 由 UI_Task() 在主循环调用，不可放进 USART1 接收中断：snprintf("%f") 耗时远超 173.6µs 失聪窗口
     * rangeSetting<=0 / NaN 表示主板按当前采样档位的物理量程工作，显示 "0.0" */
    static float s_last_range = -1.0f;
    float r = gRadarParam.rangeSetting;
    char buf[8];

    if (r == s_last_range) return;      /* 值未变：复用已写入的标签，零开销 */
    s_last_range = r;

    if (r > 0.0f && r < 1000.0f)
        snprintf(buf, sizeof(buf), "%.1f", r);
    else
        strcpy(buf, "0.0");             /* 纯数字：F6x8 字模不含大写字母，勿用 "Auto" */
    LCD_SetCurveXEndLabel(buf);
}

/**
 * @brief  主循环任务：取 ACK 事件、按脏标记重绘（含曲线与覆盖层）
 * @retval 无
 */
void UI_Task(void)
{
    UI_UpdateCurveAxisLabel();        /* 量程变了才更新 X 轴右端刻度 */
    UI_RefreshTrendRange();   /* 先重算 Y 轴范围（原在接收中断里做，见 s_histRangeDirty） */

    /* 取走 ACK 结果事件（读取即清，避免每次重绘重复触发） */
    if (Disp_SendTakeParamAckEvent())
    {
        uint8_t st  = Disp_SendParamAckState();
        uint8_t res = Disp_SendParamAckResult();
        if (st == DISP_ACK_STATE_OK)            s_ack_toast_kind = 1u;
        else if (res == 0xFFu)                  s_ack_toast_kind = 3u;   /* 0xFF = 主板无应答 */
        else                                    s_ack_toast_kind = 2u;
        s_ack_toast_end = HAL_GetTick() + ACK_TOAST_MS;
        uiDirty = 1;
    }

    if (!uiDirty) return;
    uiDirty = 0;
    LCD_FullFill(0x00);

    if (gUIState == UI_HOME)
    {
        UI_RenderHome();
        UI_DrawAckToast();      /* ⚠ 必须在渲染之后：它是覆盖层 */
        return;
    }

    char codeBuf[16];
    Get_Menu_Code_String(codeBuf);

    if (curPage == &Page_Diag && curIndex == 3)
    {
        /* 与首页一致：按 diagCurveSel 取对应曲线，无数据则只画坐标轴 */
        const uint8_t *curve = UI_GetSelectedCurve();
        LCD_DrawEchoCurve((uint8_t *)curve, 1, 1);
    }

    UI_RenderBrowse(codeBuf);
    UI_DrawAckToast();          /* ⚠ 必须在渲染之后：它是覆盖层 */
}

/**
 * @brief  初始化 UI：置默认数据模型并初始化屏幕
 * @retval 无
 */
void UI_Init(void)
{
    strcpy(gRadarParam.sensorTag, "SENSOR");
    gRadarParam.realTimeDistance = 1.346f;
    gRadarParam.pipeDiameter = 100.0f;
    LCD_init();
    curPage = &Page_Root;
    uiDirty = 1;
}

/* ============================================================
 * 协议数据更新接口（app_disp.c 调用）：写入数据模型并置 uiDirty=1 触发重绘。
 * ⚠ 本组函数运行在 USART1 接收中断里 ⇒ 耗时 = 串口接收失聪窗口，只允许 O(1)
 *   赋值与小拷贝；任何 O(n) 运算必须改为"中断里置标志，主循环里再算"
 *   （范例：s_histRangeDirty / UI_RefreshTrendRange()）。
 * ============================================================ */
/**
 * @brief  写入 MEAS 测量结果，并把有效距离记入走势历史
 * @param  distance   实时距离(m)
 * @param  peak_count 峰值数（0 = 本轮无峰）
 * @param  mode       TOF/标定模式
 * @retval 无
 */
void UI_UpdateMeas(float distance, uint8_t peak_count, uint8_t mode)
{
    __disable_irq();
    gRadarParam.realTimeDistance = distance;
    gRadarParam.measPeakCount = peak_count;   /* 测量状态，不污染配置字段 currMin */
    gRadarParam.measMode      = mode;         /* 测量状态，不污染配置字段 currMode */
    /* MEAS 的 position / delta_amp 已解析但未接入（预留）：
     *   position = 峰的亚采样下标（波形标峰值用），delta_amp = 峰处差分幅度（诊断信号强度用） */

    /* 输出走势历史：每收到一帧 MEAS 记一个距离点。
     * ⚠ 只在 peak_count > 0（本轮确实测到回波）时记录：把无效轮次的 distance=0 也记进去，
     *   走势曲线会周期性砸到底部、像液位反复归零（失真）。 */
    if (peak_count > 0u && distance > 0.0f)
    {
        /* 归档模式（gTrendSwitchArchived）下凑满 128 点即冻结 */
        if (!(gTrendSwitchArchived == 1u && s_hist_valid >= CURVE_HIST_LEN))
        {
            s_hist_dist[s_hist_head] = distance;
            s_hist_head = (uint16_t)((s_hist_head + 1u) % CURVE_HIST_LEN);
            if (s_hist_valid < CURVE_HIST_LEN)
                s_hist_valid++;

            /* ⚠ Y 轴范围重算【不在这里做】：那是最坏 127 次软浮点比较的 O(n) 操作，
             *   放在接收中断链路上会把关窗拖过 173.6µs ⇒ 溢出(ORE) ⇒ 接收永久停摆。
             *   这里只置位请求标志（O(1)），扫描交给 UI_Task() → UI_RefreshTrendRange()。 */
            s_histRangeDirty = 1u;
        }
    }

    uiDirty = 1;
    __enable_irq();
}

/**
 * @brief  写入 DIAG 诊断帧内容
 * @param  reliability  可靠度 0~100
 * @param  status       状态位
 * @param  peakMinEmpty 空罐最小峰值
 * @param  peakMaxEmpty 空罐最大峰值
 * @param  temperature  传感器温度(℃)
 * @retval 无
 */
void UI_UpdateDiag(uint8_t reliability, uint8_t status, float peakMinEmpty, float peakMaxEmpty, float temperature)
{
    __disable_irq();
    gRadarParam.diagReliability = reliability;
    gRadarParam.diagStatus      = status;
    gRadarParam.peakMinEmpty    = peakMinEmpty;
    gRadarParam.peakMaxEmpty    = peakMaxEmpty;
    gRadarParam.sensorTemperature = temperature;   /* 传感器温度，供诊断页显示 */
    uiDirty = 1;
    __enable_irq();
}

/**
 * @brief  PARAM_DUMP 全量配置反序列化，写入 gRadarParam 的配置字段
 * @param  payload 负载（按 dispproto.h 的 81 字节布局）
 * @param  len     负载字节数（<81 直接返回）
 * @retval 无
 */
void UI_UpdateParamDump(const uint8_t *payload, uint8_t len)
{
    /* 仅写配置字段，不动实时测量字段（realTimeDistance/measPeakCount/measMode） */
    if (payload == 0 || len < 81u) return;

    __disable_irq();
    uint16_t off = 0;
    float f; uint8_t u;
    memcpy(&f, &payload[off], 4); gRadarParam.lowAdjustPct = f; off += 4;
    memcpy(&f, &payload[off], 4); gRadarParam.lowAdjustVal = f; off += 4;
    memcpy(&f, &payload[off], 4); gRadarParam.highAdjustPct = f; off += 4;
    memcpy(&f, &payload[off], 4); gRadarParam.highAdjustVal = f; off += 4;
    u = payload[off++]; gRadarParam.matType = u;
    u = payload[off++]; gRadarParam.matFastChange = u;
    u = payload[off++]; gRadarParam.matFirstWave = u;
    u = payload[off++]; gRadarParam.matSurfAngle = u;
    u = payload[off++]; gRadarParam.matFoamDust = u;
    u = payload[off++]; gRadarParam.matSmallDK = u;
    u = payload[off++]; gRadarParam.matPipe = u;
    memcpy(&f, &payload[off], 4); gRadarParam.pipeDiameter = f; off += 4;
    memcpy(&f, &payload[off], 4); gRadarParam.dampTime = f; off += 4;
    u = payload[off++]; gRadarParam.outMap = u;
    u = payload[off++]; gRadarParam.scaleUnit = u;
    memcpy(&f, &payload[off], 4); gRadarParam.scaleVal = f; off += 4;
    memcpy(&f, &payload[off], 4); gRadarParam.rangeSetting = f; off += 4;
    memcpy(&f, &payload[off], 4); gRadarParam.blindZone = f; off += 4;
    u = payload[off++]; gRadarParam.currMode = u;
    u = payload[off++]; gRadarParam.currFault = u;
    u = payload[off++]; gRadarParam.currMin = u;
    u = payload[off++]; gRadarParam.servReset = u;
    u = payload[off++]; gRadarParam.servUnit = u;
    u = payload[off++]; gRadarParam.servHART = u;
    u = payload[off++]; gRadarParam.servHARTAddr = u;
    memcpy(&f, &payload[off], 4); gRadarParam.servOffset = f; off += 4;
    memcpy(&f, &payload[off], 4); gRadarParam.threshEcho = f; off += 4;
    memcpy(&f, &payload[off], 4); gRadarParam.threshEnv = f; off += 4;
    u = payload[off++]; gRadarParam.diagSim = u;
    memcpy(gRadarParam.sensorTag, &payload[off], 16); off += 16;
    gRadarParam.sensorTag[15] = '\0';
    uiDirty = 1;
    __enable_irq();
}

/**
 * @brief  写入回波曲线（0x02 ECHO）
 * @param  echo 128 字节包络
 * @param  len  负载字节数（>128 截断）
 * @retval 无
 */
void UI_UpdateEcho(const uint8_t *echo, uint8_t len)
{
    uint8_t n = (len > 128u) ? 128u : len;
    if (echo != 0 && n > 0u)
    {
        __disable_irq();
        memcpy(radar_echo, echo, n);
        uiDirty = 1;
        __enable_irq();
    }
}

/**
 * @brief  按曲线类型写入对应缓冲（0x06 ECHO_TYPED）
 * @param  curveType 曲线类型（DISP_CURVE_*）
 * @param  data      128 字节曲线数据
 * @param  len       负载字节数（>128 截断）
 * @retval 无
 */
void UI_UpdateEchoTyped(uint8_t curveType, const uint8_t *data, uint8_t len)
{
    uint8_t n = (len > 128u) ? 128u : len;
    uint8_t *dst = 0;

    if (data == 0 || n == 0u) return;

    switch (curveType)
    {
        case DISP_CURVE_ECHO:  dst = radar_echo;       break;
        case DISP_CURVE_FALSE: dst = radar_echo_false; break;
        default:               return;   /* 未知类型：丢弃，避免污染已有曲线 */
    }

    __disable_irq();
    memcpy(dst, data, n);
    uiDirty = 1;
    __enable_irq();
}

/**
 * @brief  把距离历史按 0~255 标度铺进 128 点渲染缓冲（Y 轴用 s_hist_min/max 自适应）
 * @param  out128 输出缓冲（128 字节）
 * @retval 无
 */
static void Trend_BuildRenderBuffer(uint8_t *out128)
{
    uint8_t n = s_hist_valid;
    float   span;
    uint8_t i;

    if (n == 0u) return;   /* 无数据：保持原缓冲（坐标轴会照画，波形空白） */

    span = s_hist_max - s_hist_min;
    if (span < 0.001f) span = 0.001f;   /* 防止液位恒定时的除零 */

    for (i = 0u; i < n; i++)
    {
        float v = (s_hist_dist[i] - s_hist_min) / span;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        out128[i] = (uint8_t)(v * 255.0f);
    }
    /* 归档模式下 n<128：剩余点补最后一个值，让尾部保持持平而非骤降到 0 */
    for (i = n; i < CURVE_HIST_LEN; i++)
        out128[i] = (n > 0u) ? out128[n - 1u] : 0u;
}

/**
 * @brief  取当前 diagCurveSel 对应的曲线数据指针
 * @retval 128 点 0~255 标度缓冲；无对应曲线返回 0
 */
const uint8_t *UI_GetSelectedCurve(void)
{
    switch (gRadarParam.diagCurveSel)
    {
        case 0u: return radar_echo;        /* 回波曲线 */
        case 1u: return radar_echo_false;  /* 虚假回波曲线 */
        case 2u:
            /* 输出走势曲线：本地历史 → 128 点标度 */
            Trend_BuildRenderBuffer(s_trend_render);
            return s_trend_render;
        default: return 0;
    }
}

/**
 * @brief  查询输出走势曲线已累积的有效点数
 * @retval 0~128
 */
uint8_t UI_GetTrendPointCount(void)
{
    return s_hist_valid;
}
