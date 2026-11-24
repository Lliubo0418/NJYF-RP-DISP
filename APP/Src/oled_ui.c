#include "oled_ui.h"
#include "oled.h"
#include "oledfont.h"
#include "dispproto.h"
#include "app_disp.h"
#include "main.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CUSTOM_BUF_SIZE 32u

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
uint8_t radar_echo_false[128] = {0};

#define CURVE_HIST_LEN  128u
static float    s_hist_dist[CURVE_HIST_LEN];
static uint8_t  s_hist_valid = 0;
static uint16_t s_hist_head  = 0;
static float    s_hist_min   = 0.0f;
static float    s_hist_max   = 0.0f;
static uint8_t  s_trend_render[CURVE_HIST_LEN];

static volatile uint8_t s_histRangeDirty = 0;

uint8_t gTrendSwitchArchived = 1;

RADAR_PARAM gRadarParam;
static UI_STATE gUIState = UI_HOME;
static uint8_t home_view_mode = 0;
static volatile uint8_t uiDirty = 1;

static char editStr[16] = {0};
static uint8_t editCursorPos = 0;
static EDIT_SUBMODE editTarget = EDIT_MAIN;
static uint8_t editSelectOrig = 0;

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

const char * const gLangTable[4][MENU_ID_MAX] = {
    {
        "基本设置", "显示", "诊断", "服务",
        "低位调整", "高位调整", "物料性质", "阻尼时间", "输出映射", "定标量单位", "定标", "量程设定", "盲区范围", "传感器标签",
        "液体", "固体", "微DK",
        "物料快速变化", "首波选择", "表面波动", "泡沫", "DK值小", "导波管测量",
        "物料快速变化", "首波选择", "堆角大", "粉尘强", "DK值小", "导波管测量",
        "物料快速变化", "首波选择", "表面波动", "无", "DK值小", "导波管测量",
        "导波管直径",
        "显示内容", "LCD对比度",
        "测量峰值", "测量状态", "选择曲线", "回波曲线", "仿真",
        "最小空高值", "最大空高值", "测量可靠性", "传感器状态",
        "缩放方向", "缩放比例",
        "虚假回波", "电流输出", "复位", "测量单位", "语言", "HART工作模式", "复制传感器数据", "密码", "距离偏量", "阈值设定",
        "输出模式", "故障模式", "最小电流",
        "工作模式", "地址",
        "回波阈值", "包络线",
        "", "基本设置", "物料性质", "液体", "固体", "微DK", "导波管测量", "显示", "诊断", "回波曲线", "服务", "电流输出", "HART模式", "HART地址", "阈值设定",
        "正常工作", "回波曲线", "导波管直径:", "mm"
    },
    {
        "Basic", "Display", "Diag", "Service",
        "Low Adj", "High Adj", "Material", "Damp Time", "Out Map", "Scale Unit", "Scale", "Range", "Blind Zone", "Sensor Tag",
        "Liquid", "Solid", "Micro DK",
        "Fast Change", "First Wave", "Surf Angle", "Foam", "Small DK", "Pipe",
        "Fast Change", "First Wave", "Large Angle", "Dust", "Small DK", "Pipe",
        "Fast Change", "First Wave", "Surf Angle", "None", "Small DK", "Pipe",
        "Pipe Dia",
        "Disp Content", "LCD Contrast",
        "Peak", "Meas Status", "Curve Sel", "Curve", "Simulation",
        "Min Empty", "Max Empty", "Meas Reliab", "Sens Status",
        "Zoom Dir", "Zoom Scale",
        "False Echo", "Current", "Reset", "Unit", "Language", "HART Mode", "Copy Data", "Password", "Offset", "Threshold",
        "Out Mode", "Fault Mode", "Min Current",
        "Mode", "Addr",
        "Echo Thresh", "Envelope",
        "", "Basic", "Material", "Liquid", "Solid", "Micro DK", "Pipe", "Display", "Diag", "Curve", "Service", "Current", "HART", "HART Addr", "Threshold",
        "Normal", "Curve", "Pipe Dia:", "mm"
    },
    {
        "Base", "Display", "Diag", "Service",
        "Basso", "Alto", "Materiale", "Tempo", "Mappa", "Unita", "Scala", "Intervallo", "Zona Cieca", "Tag Sensore",
        "Liquido", "Solido", "Micro DK",
        "Cambio Rapido", "Prima Onda", "Angolo", "Schiuma", "DK Piccolo", "Tubo",
        "Cambio Rapido", "Prima Onda", "Angolo Grande", "Polvere", "DK Piccolo", "Tubo",
        "Cambio Rapido", "Prima Onda", "Angolo", "Nessuno", "DK Piccolo", "Tubo",
        "Diametro",
        "Contenuto", "Contrasto",
        "Picco", "Meas Stato", "Sel Curva", "Curva", "Simulazione",
        "Vuoto Min", "Vuoto Max", "Affidabilita", "Sens Stato",
        "Direzione", "Scala",
        "Eco Falso", "Corrente", "Reset", "Unita", "Lingua", "Modo HART", "Copia Dati", "Password", "Offset", "Soglia",
        "Modo", "Guasto", "Min Corrente",
        "Modo", "Indirizzo",
        "Soglia Eco", "Inviluppo",
        "", "Base", "Materiale", "Liquido", "Solido", "Micro DK", "Tubo", "Display", "Diag", "Curva", "Service", "Corrente", "HART", "Indirizzo HART", "Soglia",
        "Normale", "Curva", "Diametro:", "mm"
    },
    {
        "Base", "Display", "Diag", "Service",
        "Bas", "Haut", "Materiau", "Temps", "Carte", "Unite", "Echelle", "Intervalle", "Zone Aveugle", "Etiquette",
        "Liquide", "Solide", "Micro DK",
        "Changement Rapide", "Premiere Onde", "Angle", "Mousse", "DK Petit", "Tube",
        "Changement Rapide", "Premiere Onde", "Grand Angle", "Poussiere", "DK Petit", "Tube",
        "Changement Rapide", "Premiere Onde", "Angle", "Aucun", "DK Petit", "Tube",
        "Diametre",
        "Contenu", "Contraste",
        "Pic", "Meas Etat", "Sel Courbe", "Courbe", "Simulation",
        "Vide Min", "Vide Max", "Fiabilite", "Sens Etat",
        "Direction", "Echelle",
        "Eco Faux", "Courant", "Reset", "Unite", "Langue", "Mode HART", "Copier Donnees", "Mot de Passe", "Decalage", "Seuil",
        "Mode", "Defaut", "Min Courant",
        "Mode", "Adresse",
        "Seuil Eco", "Enveloppe",
        "", "Base", "Materiau", "Liquide", "Solide", "Micro DK", "Tube", "Display", "Diag", "Courbe", "Service", "Courant", "HART", "Adresse HART", "Seuil",
        "Normal", "Courbe", "Diametre:", "mm"
    }
};

/* --- 自定义格式化回调 --- */
/**
 * @brief  低位调整项的显示格式化
 * @param  self   菜单项
 * @param  valBuf 主值输出
 * @param  ex1    附带值1 输出
 * @param  ex2    附带值2 输出
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
 * @param  valBuf 主值输出
 * @param  ex1    附带值1 输出
 * @param  ex2    附带值2 输出
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
 * @param  valBuf 主值输出
 * @param  ex1    附带值1 输出
 * @param  ex2    未使用
 * @retval 无
 */
static void CustomFormat_peakMinMaxEmpty(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    snprintf(valBuf, CUSTOM_BUF_SIZE, "%s: %.0f m(d)", UI_GetText(TEXT_PEAK_MIN_EMPTY), gRadarParam.peakMinEmpty);
    snprintf(ex1, CUSTOM_BUF_SIZE, "%s: %.0f m(d)", UI_GetText(TEXT_PEAK_MAX_EMPTY), gRadarParam.peakMaxEmpty);
}
/**
 * @brief  诊断页测量状态
 * @param  self   菜单项
 * @param  valBuf 主值输出
 * @param  ex1    附带值1 输出
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
 * @param  valBuf 主值输出
 * @param  ex1    附带值1 输出
 * @param  ex2    附带值2 输出
 * @retval 无
 */
static void CustomFormat_serv_current(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;
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
 * @param  valBuf 主值输出
 * @param  ex1    附带值1 输出
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
 * @brief  服务页阈值
 * @param  self   菜单项
 * @param  valBuf 主值输出
 * @param  ex1    附带值1 输出
 * @param  ex2    未使用
 * @retval 无
 */
static void CustomFormat_serv_thresh(MenuItem *self, char *valBuf, char *ex1, char *ex2)
{
    snprintf(valBuf, CUSTOM_BUF_SIZE, "%s: %.0f ", UI_GetText(MENU_THRESH_ECHO), gRadarParam.threshEcho);
    snprintf(ex1, CUSTOM_BUF_SIZE, "%s: %.0f ", UI_GetText(MENU_THRESH_ENV), gRadarParam.threshEnv);
}

extern MenuPage Page_Root, Page_Basic, Page_Display, Page_Diag, Page_Service;
extern MenuPage Page_Mat, Page_MatLiquid, Page_MatSolid, Page_MatMicroDK, Page_Pipe;
extern MenuPage Page_Curve, Page_Current, Page_HART, Page_HARTAddr, Page_Thresh;
extern MenuPage Page_DiagPeak, Page_DiagStatus;


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
 * @brief  生成当前位置的菜单编号，写进 buf
 * @param  buf 输出缓冲
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
 * @param  item 菜单项
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
        *(uint8_t *)item->value = (uint8_t)tempVal;
    else if (item->valueType == VAL_FLOAT || item->valueType == 0)
        *(float *)item->value = tempVal;
}

/**
 * @brief  浮点项编辑：key 1=改当前位 2=移光标 3=确认
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
                if (item->paramId != 0u)
                    Disp_SendSetParam((uint8_t)(item->paramId - 1u), *(float *)item->value);
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
        if (item->paramId != 0u)
            Disp_SendStr((uint8_t)(item->paramId - 1u), (const char *)item->value);
    }
}

/**
 * @brief  选择项编辑：key 1/2=循环取值 3=确认
 * @param  item 菜单项
 * @param  key  按键码 1/2/3
 * @retval 无
 */
static void Edit_Select(MenuItem *item, uint8_t key)
{
    uint8_t *v = (uint8_t *)item->value;
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
        if (item->paramId != 0u)
            Disp_SendSetParam((uint8_t)(item->paramId - 1u), (float)(*v));
        if (item->value == &gRadarParam.servHART && *v == 0)
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
 * @brief  格式化菜单项显示值
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
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;
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
 * @brief  按 renderMode 把值画到屏幕
 * @param  item      菜单项
 * @param  valBuf    主值字符串
 * @param  ex1       附带值1字符串
 * @param  ex2       附带值2字符串
 * @param  isEditing 1=编辑态
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
            LCD_ShowStrEx(COL_CENTER_X, 2, (uint8_t *)valBuf);
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

        LCD_FillBlock(0u, 0u, (uint8_t)COL_HOME_LEFT_RESERVE, 8u,
                      (uint8_t)HOME_LEFT_FILL_BYTE);

        if (gRadarParam.measPeakCount > 0u)
            snprintf(val, sizeof(val), "%.3f", gRadarParam.realTimeDistance);
        else
            snprintf(val, sizeof(val), "----");

        /* 大字读数：居中。 */
        uint16_t bigW  = LCD_GetStrBigWidth(val);
        uint16_t avail = (uint16_t)(LCD_W - COL_HOME_LEFT_RESERVE);
        uint8_t  bigCol = (bigW <= avail)
                          ? (uint8_t)(COL_HOME_LEFT_RESERVE + (avail - bigW) / 2u)
                          : (uint8_t)COL_HOME_LEFT_RESERVE;
        LCD_ShowStrExBig(bigCol, 0, val);

        const char *unitStr = (gRadarParam.servUnit == 1u) ? "ft" : "m";
        uint16_t unW = LCD_GetStrExWidth(unitStr);
        uint8_t  numRight = (uint8_t)(bigCol + ((bigW >= 2u) ? (bigW - 2u) : bigW));
        uint8_t  unCol = (numRight >= unW) ? (uint8_t)(numRight - (uint8_t)unW) : 0u;
        for (const char *p = unitStr; *p != '\0'; p++)
            unCol = (uint8_t)(unCol + LCD_ShowCharCompact(unCol, 4, (uint8_t)*p));

        if (gRadarParam.sensorTag[0] != '\0')
            LCD_ShowStrExCompact((uint8_t)COL_HOME_LEFT_RESERVE, 6,
                                 (uint8_t *)gRadarParam.sensorTag);
    }
    else
    {                                                                              
        /* 按 diagCurveSel 取对应的曲线缓冲 */
        const uint8_t *curve = UI_GetSelectedCurve();
        uint8_t curveSel = gRadarParam.diagCurveSel;
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;

        if (curveSel >= 3u) curveSel = 0u;

        if (curve != NULL)
            LCD_DrawEchoCurve((uint8_t *)curve, 1, 1);
        else
            LCD_DrawEchoCurve(NULL, 1, 1);   /* 只画坐标轴，不画波形*/

        LCD_ShowStrExCompact(0, 0, (uint8_t *)dict_curve[lang * 3 + curveSel]);

        LCD_ShowStr_Small(110, 0, "3-4");
    }
}

/**
 * @brief  画菜单列表页，左侧箭头指示当前项
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
 * @brief  画参数详情页
 * @param  codeBuf 右上角菜单编号
 * @retval 无
 */
static void UI_RenderParamItem(const char *codeBuf)
{
    MenuItem *item = &curPage->items[curIndex];
    char titleBuf[32], valBuf[32] = {0}, ex1[32] = {0}, ex2[32] = {0};

    // 标题 + 导航号 
    snprintf(titleBuf, sizeof(titleBuf), "%s", UI_GetText(item->id));
    LCD_ShowStrExCompact(0, 0, (uint8_t *)titleBuf);
    LCD_ShowStr_Small(128 - strlen(codeBuf) * 6, 0, codeBuf);

    if (curPage == &Page_Basic && item->subPage == &Page_Mat)
    {
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;
        uint8_t mt = (gRadarParam.matType > 2u) ? 2u : gRadarParam.matType;
        const char *matName = dict_mat[lang * 3 + mt];
        LCD_ShowStrExCompact(30, 2, (uint8_t *)matName);
        LCD_ShowArrowEx(2, 3, 30, (char *)matName);
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
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;
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

    if (gUIState == UI_EDIT && item->type == MENU_SELECT)
    {
        uint8_t col = 0, row = 0;
        uint8_t lang = gRadarParam.servLang; if (lang > 3u) lang = 0u;
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
 * @brief  按键总入口：按当前状态机分派
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
                    editSelectOrig = *(uint8_t *)item->value;
                }
            }
        }
    }
    uiDirty = 1;
}


void UI_KeyK3_Back(void)  { UI_ProcessKey(0); }
void UI_KeyK4_Up(void)    { UI_ProcessKey(1); }
void UI_KeyK5_Loop(void)  { UI_ProcessKey(2); }
void UI_KeyK6_Enter(void) { UI_ProcessKey(3); }

/**
 * @brief  Y 轴自适应范围
 * @retval 无
 */
static void UI_RefreshTrendRange(void)
{
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

#define ACK_TOAST_MS  1500u

static uint8_t  s_ack_toast_kind = 0u;
static uint32_t s_ack_toast_end  = 0u;    /* 到期时刻*/

/**
 * @brief  把 ACK 结果画在屏幕底行；已过期则清状态并请求整屏重绘
 * @retval 无
 */
static void UI_DrawAckToast(void)
{
    const char *msg;

    if (s_ack_toast_kind == 0u) return;

    if ((int32_t)(HAL_GetTick() - s_ack_toast_end) >= 0)
    {
        s_ack_toast_kind = 0u;
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
 * @brief  回波曲线 X 轴右端刻度随量程设定刷新
 * @retval 无
 */
static void UI_UpdateCurveAxisLabel(void)
{
    static float s_last_range = -1.0f;
    float r = gRadarParam.rangeSetting;
    char buf[8];

    if (r == s_last_range) return;
    s_last_range = r;

    if (r > 0.0f && r < 1000.0f)
        snprintf(buf, sizeof(buf), "%.1f", r);
    else
        strcpy(buf, "0.0");             
    LCD_SetCurveXEndLabel(buf);
}

/**
 * @brief  主循环任务：取 ACK 事件、按脏标记重绘
 * @retval 无
 */
void UI_Task(void)
{
    UI_UpdateCurveAxisLabel();        /* 根据量程设定更新 X 轴右端刻度 */   
    UI_RefreshTrendRange();

    if (Disp_SendTakeParamAckEvent())
    {
        uint8_t st  = Disp_SendParamAckState();
        uint8_t res = Disp_SendParamAckResult();
        if (st == DISP_ACK_STATE_OK)            s_ack_toast_kind = 1u;
        else if (res == 0xFFu)                  s_ack_toast_kind = 3u;
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
        UI_DrawAckToast();
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
    UI_DrawAckToast();
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

/**
 * @brief  写入 MEAS 测量结果，并把有效距离记入走势历史
 * @param  distance   实时距离
 * @param  peak_count 峰值数
 * @param  mode       TOF/标定模式
 * @retval 无
 */
void UI_UpdateMeas(float distance, uint8_t peak_count, uint8_t mode)
{
    __disable_irq();
    gRadarParam.realTimeDistance = distance;
    gRadarParam.measPeakCount = peak_count;
    gRadarParam.measMode      = mode;

    if (peak_count > 0u && distance > 0.0f)
    {
        if (!(gTrendSwitchArchived == 1u && s_hist_valid >= CURVE_HIST_LEN))
        {
            s_hist_dist[s_hist_head] = distance;
            s_hist_head = (uint16_t)((s_hist_head + 1u) % CURVE_HIST_LEN);
            if (s_hist_valid < CURVE_HIST_LEN)
                s_hist_valid++;

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
 * @param  temperature  传感器温度
 * @retval 无
 */
void UI_UpdateDiag(uint8_t reliability, uint8_t status, float peakMinEmpty, float peakMaxEmpty, float temperature)
{
    __disable_irq();
    gRadarParam.diagReliability = reliability;
    gRadarParam.diagStatus      = status;
    gRadarParam.peakMinEmpty    = peakMinEmpty;
    gRadarParam.peakMaxEmpty    = peakMaxEmpty;
    gRadarParam.sensorTemperature = temperature;
    uiDirty = 1;
    __enable_irq();
}

/**
 * @brief  PARAM_DUMP 全量配置反序列化，写入 gRadarParam 的配置字段
 * @param  payload 负载
 * @param  len     负载字节数
 * @retval 无
 */
void UI_UpdateParamDump(const uint8_t *payload, uint8_t len)
{
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
 * @brief  写入回波曲线
 * @param  echo 128 字节包络
 * @param  len  负载字节数
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
 * @brief  按曲线类型写入对应缓冲
 * @param  curveType 曲线类型
 * @param  data      128 字节曲线数据
 * @param  len       负载字节数
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
        default:               return;
    }

    __disable_irq();
    memcpy(dst, data, n);
    uiDirty = 1;
    __enable_irq();
}

/**
 * @brief  把距离历史按 0~255 标度铺进 128 点渲染缓冲
 * @param  out128 输出缓冲
 * @retval 无
 */
static void Trend_BuildRenderBuffer(uint8_t *out128)
{
    uint8_t n = s_hist_valid;
    float   span;
    uint8_t i;

    if (n == 0u) return;

    span = s_hist_max - s_hist_min;
    if (span < 0.001f) span = 0.001f;

    for (i = 0u; i < n; i++)
    {
        float v = (s_hist_dist[i] - s_hist_min) / span;
        if (v < 0.0f) v = 0.0f;
        if (v > 1.0f) v = 1.0f;
        out128[i] = (uint8_t)(v * 255.0f);
    }
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
        case 0u: return radar_echo;
        case 1u: return radar_echo_false;
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
