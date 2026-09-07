   1→#include "oled_ui.h"
   2→#include "oled.h"
   3→#include "oledfont.h"
   4→#include <stdio.h>
   5→#include <string.h>
   6→#include <stdlib.h>
   7→
   8→volatile uint8_t radar_echo[128] = {
   9→    8,  7,  9,  8, 10,  9, 11, 10,  9,  8,  9, 10, 11, 12, 10, 11,
  10→   12, 13, 14, 15, 13, 12, 11, 10, 12, 13, 14, 15, 16, 18, 20, 22,
  11→   25, 28, 32, 36, 40, 44, 48, 52, 58, 65, 72, 80, 88, 96,104,112,
  12→  120,128,135,142,148,152,155,158,160,162,158,152,145,138,130,122,
  13→  114,106, 98, 90, 82, 74, 68, 62, 58, 54, 50, 46, 44, 42, 40, 38,
  14→   35, 32, 30, 28, 26, 24, 22, 20, 18, 16, 14, 12, 10,  9,  8,  7,
  15→    6,  5,  4,  3,  2,  1,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9,
  16→   10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25
  17→};
  18→
  19→volatile RADAR_PARAM gRadarParam;
  20→static UI_STATE gUIState = UI_HOME;
  21→static uint8_t home_view_mode = 0;
  22→static volatile uint8_t uiDirty = 1;
  23→
  24→static char editStr[16] = {0};
  25→static uint8_t editCursorPos = 0;
  26→static EDIT_SUBMODE editTarget = EDIT_MAIN;
  27→
  28→/* --- 多语言扁平化字典常量数组 (结构: [Lang_0_Options..., Lang_1_Options...]) --- */
  29→static const char * const dict_bool[] = {
  30→    "否", "是",                 // CN
  31→    "No", "Yes",                // EN
  32→    "No", "Si",                 // IT
  33→    "Non", "Oui"                // FR
  34→};
  35→
  36→static const char * const dict_mat[] = {
  37→    "液体", "固体", "微DK",
  38→    "Liquid", "Solid", "Micro DK",
  39→    "Liquido", "Solido", "Micro DK",
  40→    "Liquide", "Solide", "Micro DK"
  41→};
  42→
  43→static const char * const dict_wave[] = {
  44→    "正常", "稍强", "较强", "最强", "减弱",
  45→    "Normal", "Slight", "Strong", "Max", "Weak",
  46→    "Normale", "Lieve", "Forte", "Max", "Debole",
  47→    "Normal", "Leger", "Fort", "Max", "Faible"
  48→};
  49→
  50→static const char * const dict_outMap[] = {
  51→    "线性", "锥筒",
  52→    "Linear", "Cone",
  53→    "Lineare", "Cono",
  54→    "Lineaire", "Cone"
  55→};
  56→
  57→static const char * const dict_scaleUnit[] = {
  58→    "高度", "质量", "流量", "体积", "无量纲",
  59→    "Height", "Mass", "Flow", "Volume", "None",
  60→    "Altezza", "Massa", "Flusso", "Volume", "Nessuno",
  61→    "Hauteur", "Masse", "Flux", "Volume", "Aucun"
  62→};
  63→
  64→static const char * const dict_disp[] = {
  65→    "不工作", "空高", "料高", "电流", "百分比",
  66→    "Off", "Empty", "Level", "Current", "Percent",
  67→    "Spento", "Vuoto", "Livello", "Corrente", "Per cento",
  68→    "Eteint", "Vide", "Niveau", "Courant", "Pourcent"
  69→};
  70→
  71→static const char * const dict_curve[] = {
  72→    "回波曲线", "虚假回波曲线", "输出走势曲线",
  73→    "Echo Curve", "False Curve", "Trend Curve",
  74→    "Curva Eco", "Curva Falsa", "Curva Trend",
  75→    "Courbe Echo", "Courbe Fausse", "Courbe Trend"
  76→};
  77→
  78→static const char * const dict_zoomDir[] = {
  79→    "X轴缩放", "Y轴缩放", "不缩放",
  80→    "X Zoom", "Y Zoom", "No Zoom",
  81→    "Zoom X", "Zoom Y", "No Zoom",
  82→    "Zoom X", "Zoom Y", "Pas Zoom"
  83→};
  84→
  85→static const char * const dict_zoomScale[] = {
  86→    "1X", "2X", "5X", "10X",
  87→    "1X", "2X", "5X", "10X",
  88→    "1X", "2X", "5X", "10X",
  89→    "1X", "2X", "5X", "10X"
  90→};
  91→
  92→static const char * const dict_sim[] = {
  93→    "百分比", "电流", "空高",
  94→    "Percent", "Current", "Empty",
  95→    "Per cento", "Corrente", "Vuoto",
  96→    "Pourcent", "Courant", "Vide"
  97→};
  98→
  99→static const char * const dict_flsEcho[] = {
 100→    "删除", "更新", "新建", "编辑",
 101→    "Delete", "Update", "New", "Edit",
 102→    "Elimina", "Aggiorna", "Nuovo", "Modifica",
 103→    "Effacer", "Maj", "Nouveau", "Editer"
 104→};
 105→
 106→static const char * const dict_currMode[] = {
 107→    "4~20mA", "20~4mA",
 108→    "4~20mA", "20~4mA",
 109→    "4~20mA", "20~4mA",
 110→    "4~20mA", "20~4mA"
 111→};
 112→
 113→static const char * const dict_faultMode[] = {
 114→    "无变化", "20.5mA", "22.0mA",
 115→    "Hold", "20.5mA", "22.0mA",
 116→    "Mantieni", "20.5mA", "22.0mA",
 117→    "Maintien", "20.5mA", "22.0mA"
 118→};
 119→
 120→static const char * const dict_minCurr[] = {
 121→    "4mA", "3.8mA",
 122→    "4mA", "3.8mA",
 123→    "4mA", "3.8mA",
 124→    "4mA", "3.8mA"
 125→};
 126→
 127→static const char * const dict_reset[] = {
 128→    "基本复位", "工厂设置", "测量峰值",
 129→    "Basic Reset", "Factory Reset", "Peak Reset",
 130→    "Reset Base", "Reset Fabbrica", "Reset Picco",
 131→    "Reset Base", "Reset Usine", "Reset Pic"
 132→};
 133→
 134→static const char * const dict_unit[] = {
 135→    "m(d)", "ft(d)",
 136→    "m(d)", "ft(d)",
 137→    "m(d)", "ft(d)",
 138→    "m(d)", "ft(d)"
 139→};
 140→
 141→static const char * const dict_lang[] = {
 142→    "中文", "English", "Italian", "French",
 143→    "中文", "English", "Italian", "French",
 144→    "中文", "English", "Italian", "French",
 145→    "中文", "English", "Italian", "French"
 146→};
 147→
 148→static const char * const dict_hart[] = {
 149→    "标准", "多点",
 150→    "Standard", "Multi-drop",
 151→    "Standard", "Multi-drop",
 152→    "Standard", "Multi-drop"
 153→};
 154→
 155→static const char * const dict_copy[] = {
 156→    "从传感器复制", "复制到传感器",
 157→    "From Sensor", "To Sensor",
 158→    "Dal Sensore", "Al Sensore",
 159→    "Du Capteur", "Au Capteur"
 160→};
 161→
 162→/* --- 多语言资源表 --- */
 163→const char *gLangTable[4][MENU_ID_MAX] = {
 164→    // 中文
 165→    {
 166→        // 根菜单
 167→        "基本设置", "显示", "诊断", "服务",
 168→        // 基本设置
 169→        "低位调整", "高位调整", "物料性质", "阻尼时间", "输出映射", "定标量单位", "定标", "量程设定", "盲区范围", "传感器标签",
 170→        // 物料性质
 171→        "液体", "固体", "微DK",
 172→        // 液体参数
 173→        "物料快速变化", "首波选择", "表面波动", "泡沫", "DK值小", "导波管测量",
 174→        // 固体参数
 175→        "物料快速变化", "首波选择", "堆角大", "粉尘强", "DK值小", "导波管测量",
 176→        // 微DK参数
 177→        "物料快速变化", "首波选择", "表面波动", "无", "DK值小", "导波管测量",
 178→        // 导波管
 179→        "导波管直径",
 180→        // 显示
 181→        "显示内容", "LCD对比度",
 182→        // 诊断
 183→        "测量峰值", "测量状态", "选择曲线", "回波曲线", "仿真",
 184→        "最小空高值", "最大空高值", "测量可靠性", "传感器状态",
 185→        // 曲线
 186→        "缩放方向", "缩放比例",
 187→        // 服务
 188→        "虚假回波", "电流输出", "复位", "测量单位", "语言", "HART工作模式", "复制传感器数据", "密码", "距离偏量", "阈值设定",
 189→        // 电流输出
 190→        "输出模式", "故障模式", "最小电流",
 191→        // HART
 192→        "工作模式", "地址",
 193→        // 阈值
 194→        "回波阈值", "包络线",
 195→        // 页面标题
 196→        "", "基本设置", "物料性质", "液体", "固体", "微DK", "导波管测量", "显示", "诊断", "回波曲线", "服务", "电流输出", "HART模式", "HART地址", "阈值设定",
 197→        // 其他固定文本
 198→        "正常工作", "回波曲线", "导波管直径:", "mm"
 199→    },
 200→    // English
 201→    {
 202→        // 根菜单
 203→        "Basic", "Display", "Diag", "Service",
 204→        // 基本设置
 205→        "Low Adj", "High Adj", "Material", "Damp Time", "Out Map", "Scale Unit", "Scale", "Range", "Blind Zone", "Sensor Tag",
 206→        // 物料性质
 207→        "Liquid", "Solid", "Micro DK",
 208→        // 液体参数
 209→        "Fast Change", "First Wave", "Surf Angle", "Foam", "Small DK", "Pipe",
 210→        // 固体参数
 211→        "Fast Change", "First Wave", "Large Angle", "Dust", "Small DK", "Pipe",
 212→        // 微DK参数
 213→        "Fast Change", "First Wave", "Surf Angle", "None", "Small DK", "Pipe",
 214→        // 导波管
 215→        "Pipe Dia",
 216→        // 显示
 217→        "Disp Content", "LCD Contrast",
 218→        // 诊断
 219→        "Peak", "Meas Status", "Curve Sel", "Curve", "Simulation",
 220→        "Min Empty", "Max Empty", "Meas Reliab", "Sens Status",
 221→        // 曲线
 222→        "Zoom Dir", "Zoom Scale",
 223→        // 服务
 224→        "False Echo", "Current", "Reset", "Unit", "Language", "HART Mode", "Copy Data", "Password", "Offset", "Threshold",
 225→        // 电流输出
 226→        "Out Mode", "Fault Mode", "Min Current",
 227→        // HART
 228→        "Mode", "Addr",
 229→        // 阈值
 230→        "Echo Thresh", "Envelope",
 231→        // 页面标题
 232→        "", "Basic", "Material", "Liquid", "Solid", "Micro DK", "Pipe", "Display", "Diag", "Curve", "Service", "Current", "HART", "HART Addr", "Threshold",
 233→        // 其他固定文本
 234→        "Normal", "Curve", "Pipe Dia:", "mm"
 235→    },
 236→    // Italian
 237→    {
 238→        // 根菜单
 239→        "Base", "Display", "Diag", "Service",
 240→        // 基本设置
 241→        "Basso", "Alto", "Materiale", "Tempo", "Mappa", "Unita", "Scala", "Intervallo", "Zona Cieca", "Tag Sensore",
 242→        // 物料性质
 243→        "Liquido", "Solido", "Micro DK",
 244→        // 液体参数
 245→        "Cambio Rapido", "Prima Onda", "Angolo", "Schiuma", "DK Piccolo", "Tubo",
 246→        // 固体参数
 247→        "Cambio Rapido", "Prima Onda", "Angolo Grande", "Polvere", "DK Piccolo", "Tubo",
 248→        // 微DK参数
 249→        "Cambio Rapido", "Prima Onda", "Angolo", "Nessuno", "DK Piccolo", "Tubo",
 250→        // 导波管
 251→        "Diametro",
 252→        // 显示
 253→        "Contenuto", "Contrasto",
 254→        // 诊断
 255→        "Picco", "Meas Stato", "Sel Curva", "Curva", "Simulazione",
 256→        "Vuoto Min", "Vuoto Max", "Affidabilita", "Sens Stato",
 257→        // 曲线
 258→        "Direzione", "Scala",
 259→        // 服务
 260→        "Eco Falso", "Corrente", "Reset", "Unita", "Lingua", "Modo HART", "Copia Dati", "Password", "Offset", "Soglia",
 261→        // 电流输出
 262→        "Modo", "Guasto", "Min Corrente",
 263→        // HART
 264→        "Modo", "Indirizzo",
 265→        // 阈值
 266→        "Soglia Eco", "Inviluppo",
 267→        // 页面标题
 268→        "", "Base", "Materiale", "Liquido", "Solido", "Micro DK", "Tubo", "Display", "Diag", "Curva", "Service", "Corrente", "HART", "Indirizzo HART", "Soglia",
 269→        // 其他固定文本
 270→        "Normale", "Curva", "Diametro:", "mm"
 271→    },
 272→    // French
 273→    {
 274→        // 根菜单
 275→        "Base", "Display", "Diag", "Service",
 276→        // 基本设置
 277→        "Bas", "Haut", "Materiau", "Temps", "Carte", "Unite", "Echelle", "Intervalle", "Zone Aveugle", "Etiquette",
 278→        // 物料性质
 279→        "Liquide", "Solide", "Micro DK",
 280→        // 液体参数
 281→        "Changement Rapide", "Premiere Onde", "Angle", "Mousse", "DK Petit", "Tube",
 282→        // 固体参数
 283→        "Changement Rapide", "Premiere Onde", "Grand Angle", "Poussiere", "DK Petit", "Tube",
 284→        // 微DK参数
 285→        "Changement Rapide", "Premiere Onde", "Angle", "Aucun", "DK Petit", "Tube",
 286→        // 导波管
 287→        "Diametre",
 288→        // 显示
 289→        "Contenu", "Contraste",
 290→        // 诊断
 291→        "Pic", "Meas Etat", "Sel Courbe", "Courbe", "Simulation",
 292→        "Vide Min", "Vide Max", "Fiabilite", "Sens Etat",
 293→        // 曲线
 294→        "Direction", "Echelle",
 295→        // 服务
 296→        "Eco Faux", "Courant", "Reset", "Unite", "Langue", "Mode HART", "Copier Donnees", "Mot de Passe", "Decalage", "Seuil",
 297→        // 电流输出
 298→        "Mode", "Defaut", "Min Courant",
 299→        // HART
 300→        "Mode", "Adresse",
 301→        // 阈值
 302→        "Seuil Eco", "Enveloppe",
 303→        // 页面标题
 304→        "", "Base", "Materiau", "Liquide", "Solide", "Micro DK", "Tube", "Display", "Diag", "Courbe", "Service", "Courant", "HART", "Adresse HART", "Seuil",
 305→        // 其他固定文本
 306→        "Normal", "Courbe", "Diametre:", "mm"
 307→    }
 308→};
 309→
 310→/* --- 自定义格式化回调 --- */
 311→static void CustomFormat_LowAdj(MenuItem *self, char *valBuf, char *ex1, char *ex2)
 312→{
 313→    snprintf(valBuf, sizeof(valBuf), "%0.2f %%", *((float *)self->value));
 314→    snprintf(ex1, sizeof(ex1), "%.3f m(d)", gRadarParam.lowAdjustVal);
 315→    snprintf(ex2, sizeof(ex2), "%.3f m(d)", gRadarParam.realTimeDistance);
 316→}
 317→static void CustomFormat_HighAdj(MenuItem *self, char *valBuf, char *ex1, char *ex2)
 318→{
 319→    snprintf(valBuf, sizeof(valBuf), "%0.2f %%", *((float *)self->value));
 320→    snprintf(ex1, sizeof(ex1), "%.3f m(d)", gRadarParam.highAdjustVal);
 321→    snprintf(ex2, sizeof(ex2), "%.3f m(d)", gRadarParam.realTimeDistance);
 322→}
 323→static void CustomFormat_peakMinMaxEmpty(MenuItem *self, char *valBuf, char *ex1, char *ex2)
 324→{
 325→    snprintf(valBuf, sizeof(valBuf), "%s: %.0f m(d)", UI_GetText(TEXT_PEAK_MIN_EMPTY), gRadarParam.peakMinEmpty);
 326→    snprintf(ex1, sizeof(ex1), "%s: %.0f m(d)", UI_GetText(TEXT_PEAK_MAX_EMPTY), gRadarParam.peakMaxEmpty);
 327→}
 328→static void CustomFormat_diag_status(MenuItem *self, char *valBuf, char *ex1, char *ex2)
 329→{
 330→    snprintf(valBuf, sizeof(valBuf), "%s: %d dB", UI_GetText(TEXT_DIAG_RELIABILITY), gRadarParam.diagReliability);
 331→    snprintf(ex1, sizeof(ex1), "%s: %d K", UI_GetText(TEXT_DIAG_STATUS), gRadarParam.diagStatus);
 332→}
 333→static void CustomFormat_serv_current(MenuItem *self, char *valBuf, char *ex1, char *ex2)
 334→{
 335→    uint8_t lang = gRadarParam.servLang;
 336→    snprintf(valBuf, sizeof(valBuf), "%s: %s ", UI_GetText(MENU_CURRENT_MODE), dict_currMode[lang * 2 + gRadarParam.currMode]);
 337→    snprintf(ex1, sizeof(ex1), "%s: %s ", UI_GetText(MENU_CURRENT_FAULT), dict_faultMode[lang * 3 + gRadarParam.currFault]);
 338→    snprintf(ex2, sizeof(ex2), "%s: %s ", UI_GetText(MENU_CURRENT_MIN), dict_minCurr[lang * 2 + gRadarParam.currMin]);
 339→}
 340→static void CustomFormat_serv_hartaddr(MenuItem *self, char *valBuf, char *ex1, char *ex2)
 341→{
 342→    uint8_t lang = gRadarParam.servLang;
 343→    snprintf(valBuf, sizeof(valBuf), "%s ", dict_hart[lang * 2 + gRadarParam.servHART]);
 344→    snprintf(ex1, sizeof(ex1), "%s: %d", UI_GetText(MENU_HART_ADDR), gRadarParam.servHARTAddr);
 345→}
 346→static void CustomFormat_serv_thresh(MenuItem *self, char *valBuf, char *ex1, char *ex2)
 347→{
 348→    snprintf(valBuf, sizeof(valBuf), "%s: %.0f ", UI_GetText(MENU_THRESH_ECHO), gRadarParam.threshEcho);
 349→    snprintf(ex1, sizeof(ex1), "%s: %.0f ", UI_GetText(MENU_THRESH_ENV), gRadarParam.threshEnv);
 350→}
 351→
 352→/* --- 声明所有页面 --- */
 353→extern MenuPage Page_Root, Page_Basic, Page_Display, Page_Diag, Page_Service;
 354→extern MenuPage Page_Mat, Page_MatLiquid, Page_MatSolid, Page_MatMicroDK, Page_Pipe;
 355→extern MenuPage Page_Curve, Page_Current, Page_HART, Page_HARTAddr, Page_Thresh;
 356→extern MenuPage Page_DiagPeak, Page_DiagStatus;
 357→
 358→/* ---------------- 菜单项定义 ---------------- */
 359→
 360→static MenuItem items_root[] = {
 361→    {MENU_ROOT_BASIC, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Basic, NULL, NULL},
 362→    {MENU_ROOT_DISPLAY, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Display, NULL, NULL},
 363→    {MENU_ROOT_DIAG, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Diag, NULL, NULL},
 364→    {MENU_ROOT_SERVICE, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Service, NULL, NULL}};
 365→
 366→static MenuItem items_basic[] = {
 367→    {MENU_BASIC_LOW_ADJ, MENU_FLOAT,  &gRadarParam.lowAdjustPct,  0, 100, 1,    NULL, 0, "%0.2f", NULL, NULL, NULL, CustomFormat_LowAdj,
 368→     RENDER_DEFAULT, VAL_FLOAT, &gRadarParam.lowAdjustVal, "%.3f"},
 369→    {MENU_BASIC_HIGH_ADJ, MENU_FLOAT,  &gRadarParam.highAdjustPct, 0, 100, 1,    NULL, 0, "%0.2f", NULL, NULL, NULL, CustomFormat_HighAdj,
 370→     RENDER_DEFAULT, VAL_FLOAT, &gRadarParam.highAdjustVal, "%.3f"},
 371→    {MENU_BASIC_MAT, MENU_PAGE,   &gRadarParam.matType, 0, 0, 0, dict_mat, 3, NULL, NULL, &Page_Mat, NULL, NULL},
 372→    {MENU_BASIC_DAMP_TIME, MENU_FLOAT,  &gRadarParam.dampTime, 0, 100, 1, NULL, 0, "%0.0f", "S", NULL, NULL, NULL},
 373→    {MENU_BASIC_OUT_MAP, MENU_SELECT, &gRadarParam.outMap, 0, 1, 1, dict_outMap, 2, NULL, NULL, NULL, NULL, NULL},
 374→    {MENU_BASIC_SCALE_UNIT, MENU_SELECT, &gRadarParam.scaleUnit, 0, 4, 1, dict_scaleUnit, 5, NULL, NULL, NULL, NULL, NULL},
 375→    {MENU_BASIC_SCALE_VAL, MENU_FLOAT,  &gRadarParam.scaleVal, 0, 1000, 1, NULL, 0, "%0.2f", NULL, NULL, NULL, NULL},
 376→    {MENU_BASIC_RANGE_SETTING, MENU_FLOAT,  &gRadarParam.rangeSetting, 0, 100, 0.1, NULL, 0, "%0.3f", "m", NULL, NULL, NULL},
 377→    {MENU_BASIC_BLIND_ZONE, MENU_FLOAT,  &gRadarParam.blindZone, 0, 10, 0.01, NULL, 0, "%0.3f", "m", NULL, NULL, NULL},
 378→    {MENU_BASIC_SENSOR_TAG, MENU_STRING, &gRadarParam.sensorTag, 0, 0, 0, NULL, 0, NULL, NULL, NULL, NULL, NULL}};
 379→
 380→static MenuItem items_mat[] = {
 381→    {MENU_MAT_LIQUID, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_MatLiquid, NULL, NULL},
 382→    {MENU_MAT_SOLID, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_MatSolid, NULL, NULL},
 383→    {MENU_MAT_MICRO_DK, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_MatMicroDK, NULL, NULL}};
 384→
 385→static MenuItem items_mat_liquid[] = {
 386→    {MENU_MAT_LIQUID_FAST_CHANGE, MENU_SELECT, &gRadarParam.matFastChange, 0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 387→    {MENU_MAT_LIQUID_FIRST_WAVE, MENU_SELECT, &gRadarParam.matFirstWave,  0, 4, 1, dict_wave, 5, NULL, NULL, NULL, NULL, NULL},
 388→    {MENU_MAT_LIQUID_SURF_ANGLE, MENU_SELECT, &gRadarParam.matSurfAngle,  0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 389→    {MENU_MAT_LIQUID_FOAM, MENU_SELECT, &gRadarParam.matFoamDust,   0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 390→    {MENU_MAT_LIQUID_SMALL_DK, MENU_SELECT, &gRadarParam.matSmallDK,    0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 391→    {MENU_MAT_LIQUID_PIPE, MENU_SELECT, &gRadarParam.matPipe, 0, 1, 1, dict_bool, 2, NULL, NULL, &Page_Pipe, NULL, NULL,
 392→     RENDER_PIPE, VAL_UINT8, NULL, NULL}};
 393→
 394→static MenuItem items_mat_solid[] = {
 395→    {MENU_MAT_SOLID_FAST_CHANGE, MENU_SELECT, &gRadarParam.matFastChange, 0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 396→    {MENU_MAT_SOLID_FIRST_WAVE, MENU_SELECT, &gRadarParam.matFirstWave,  0, 4, 1, dict_wave, 5, NULL, NULL, NULL, NULL, NULL},
 397→    {MENU_MAT_SOLID_SURF_ANGLE, MENU_SELECT, &gRadarParam.matSurfAngle,  0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 398→    {MENU_MAT_SOLID_DUST, MENU_SELECT, &gRadarParam.matFoamDust,   0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 399→    {MENU_MAT_SOLID_SMALL_DK, MENU_SELECT, &gRadarParam.matSmallDK,    0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 400→    {MENU_MAT_SOLID_PIPE, MENU_SELECT, &gRadarParam.matPipe, 0, 1, 1, dict_bool, 2, NULL, NULL, &Page_Pipe, NULL, NULL,
 401→     RENDER_PIPE, VAL_UINT8, NULL, NULL}};
 402→
 403→static MenuItem items_mat_microdk[] = {
 404→    {MENU_MAT_MICRO_FAST_CHANGE, MENU_SELECT, &gRadarParam.matFastChange, 0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 405→    {MENU_MAT_MICRO_FIRST_WAVE, MENU_SELECT, &gRadarParam.matFirstWave,  0, 4, 1, dict_wave, 5, NULL, NULL, NULL, NULL, NULL},
 406→    {MENU_MAT_MICRO_SURF_ANGLE, MENU_SELECT, &gRadarParam.matSurfAngle,  0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 407→    {MENU_MAT_MICRO_NONE, MENU_READONLY, NULL, 0, 0, 0, NULL, 0, NULL, NULL, NULL, NULL, NULL},
 408→    {MENU_MAT_MICRO_SMALL_DK, MENU_SELECT, &gRadarParam.matSmallDK,    0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 409→    {MENU_MAT_MICRO_PIPE, MENU_SELECT, &gRadarParam.matPipe, 0, 1, 1, dict_bool, 2, NULL, NULL, &Page_Pipe, NULL, NULL,
 410→     RENDER_PIPE, VAL_UINT8, NULL, NULL}};
 411→
 412→static MenuItem items_pipe[] = {
 413→    {MENU_PIPE_DIAMETER, MENU_FLOAT, &gRadarParam.pipeDiameter, 0, 1000, 1, NULL, 0, "%04.0f", "mm", NULL, NULL, NULL},
 414→};
 415→
 416→static MenuItem items_display[] = {
 417→    {MENU_DISP_CONTENT, MENU_SELECT, &gRadarParam.dispContent, 0, 4, 1, dict_disp, 5, NULL, NULL, NULL, NULL, NULL},
 418→    {MENU_DISP_CONTRAST, MENU_FLOAT,  &gRadarParam.lcdContrast, 0, 100, 1, NULL, 0, "%03.0f", NULL, NULL, NULL, NULL}};
 419→
 420→static MenuItem items_diag[] = {
 421→    {MENU_DIAG_PEAK, MENU_READONLY, NULL, 0, 0, 0, NULL, 0, NULL, NULL, NULL, NULL, CustomFormat_peakMinMaxEmpty,
 422→     RENDER_PEAK, VAL_FLOAT, NULL, NULL},
 423→    {MENU_DIAG_STATUS, MENU_READONLY, NULL, 0, 0, 0, NULL, 0, NULL, NULL, NULL, NULL, CustomFormat_diag_status,
 424→     RENDER_STATUS, VAL_UINT8, NULL, NULL},
 425→    {MENU_DIAG_CURVE_SEL, MENU_SELECT, &gRadarParam.diagCurveSel, 0, 2, 1, dict_curve, 3, NULL, NULL, NULL, NULL, NULL},
 426→    {MENU_DIAG_CURVE, MENU_PAGE, NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Curve, NULL, NULL},
 427→    {MENU_DIAG_SIM, MENU_SELECT, &gRadarParam.diagSim, 0, 2, 1, dict_sim, 3, NULL, NULL, NULL, NULL, NULL}};
 428→
 429→static MenuItem items_curve[] = {
 430→    {MENU_CURVE_ZOOM_DIR, MENU_SELECT, &gRadarParam.diagCurveZoom, 0, 2, 1, dict_zoomDir, 3, NULL, NULL, NULL, NULL, NULL},
 431→    {MENU_CURVE_ZOOM_SCALE, MENU_SELECT, &gRadarParam.diagCurveScale, 0, 3, 1, dict_zoomScale, 4, NULL, NULL, NULL, NULL, NULL}};
 432→
 433→static MenuItem items_service[] = {
 434→    {MENU_SERV_FALSE_ECHO, MENU_SELECT, &gRadarParam.servFalseEcho, 0, 3, 1, dict_flsEcho, 4, NULL, NULL, NULL, NULL, NULL},
 435→    {MENU_SERV_CURRENT, MENU_PAGE,   NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Current, NULL, CustomFormat_serv_current, RENDER_SERV_CURRENT, VAL_STRING, NULL, NULL},
 436→    {MENU_SERV_RESET, MENU_SELECT, &gRadarParam.servReset, 0, 2, 1, dict_reset, 3, NULL, NULL, NULL, NULL, NULL},
 437→    {MENU_SERV_UNIT, MENU_SELECT, &gRadarParam.servUnit, 0, 1, 1, dict_unit, 2, NULL, NULL, NULL, NULL, NULL},
 438→    {MENU_SERV_LANG, MENU_SELECT, &gRadarParam.servLang, 0, 3, 1, dict_lang, 4, NULL, NULL, NULL, NULL, NULL},
 439→    {MENU_SERV_HART, MENU_PAGE,   NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_HART, NULL, CustomFormat_serv_hartaddr, RENDER_SERV_HARTADDR, VAL_STRING, NULL, NULL},
 440→    {MENU_SERV_COPY_DATA, MENU_SELECT, &gRadarParam.servCopyData, 0, 1, 1, dict_copy, 2, NULL, NULL, NULL, NULL, NULL},
 441→    {MENU_SERV_PWD, MENU_SELECT, &gRadarParam.servPwdEn, 0, 1, 1, dict_bool, 2, NULL, NULL, NULL, NULL, NULL},
 442→    {MENU_SERV_OFFSET, MENU_FLOAT,  &gRadarParam.servOffset, -10, 10, 0.01, NULL, 0, "%+0.2f", "m(d)", NULL, NULL, NULL},
 443→    {MENU_SERV_THRESH, MENU_PAGE,   NULL, 0, 0, 0, NULL, 0, NULL, NULL, &Page_Thresh, NULL, CustomFormat_serv_thresh, RENDER_SERV_THRESH, VAL_STRING, NULL, NULL}};
 444→
 445→static MenuItem items_current[] = {
 446→    {MENU_CURRENT_MODE, MENU_SELECT, &gRadarParam.currMode,  0, 1, 1, dict_currMode, 2, NULL, NULL, NULL, NULL, NULL},
 447→    {MENU_CURRENT_FAULT, MENU_SELECT, &gRadarParam.currFault, 0, 3, 1, dict_faultMode, 3, NULL, NULL, NULL, NULL, NULL},
 448→    {MENU_CURRENT_MIN, MENU_SELECT, &gRadarParam.currMin,   0, 1, 1, dict_minCurr, 2, NULL, NULL, NULL, NULL, NULL}};
 449→
 450→static MenuItem items_hart[] = {
 451→    {MENU_HART_MODE, MENU_SELECT, &gRadarParam.servHART, 0, 1, 1, dict_hart, 2, NULL, NULL, &Page_HARTAddr, NULL, NULL}};
 452→
 453→static MenuItem items_hart_addr[] = {
 454→    {MENU_HART_ADDR, MENU_FLOAT, &gRadarParam.servHARTAddr, 0, 15, 1, NULL, 0, "%02.0f", NULL, NULL, NULL, NULL,
 455→     RENDER_SERV_HARTADDR, VAL_UINT8, NULL, NULL}};
 456→
 457→static MenuItem items_thresh[] = {
 458→    {MENU_THRESH_ECHO, MENU_FLOAT, &gRadarParam.threshEcho, 0, 200, 1, NULL, 0, "%02.0f", "mV", NULL, NULL, NULL},
 459→    {MENU_THRESH_ENV, MENU_FLOAT,  &gRadarParam.threshEnv,  0, 100, 1, NULL, 0, "%02.0f", "mV", NULL, NULL, NULL}};
 460→
 461→/* --- 页面实体打包 --- */
 462→#define PACK_PAGE(var, title_id, p_type, arr, flatten) \
 463→    MenuPage var = {title_id, p_type, flatten, arr, sizeof(arr) / sizeof(arr[0])}
 464→
 465→PACK_PAGE(Page_Root,       PAGE_TITLE_ROOT,       PAGE_TYPE_ROOT,  items_root,        0);
 466→PACK_PAGE(Page_Basic,      PAGE_TITLE_BASIC,      PAGE_TYPE_PARAM, items_basic,       0);
 467→PACK_PAGE(Page_Mat,        PAGE_TITLE_MAT,        PAGE_TYPE_MENU,  items_mat,         0);
 468→PACK_PAGE(Page_MatLiquid,  PAGE_TITLE_MAT_LIQUID, PAGE_TYPE_PARAM, items_mat_liquid,  1);
 469→PACK_PAGE(Page_MatSolid,   PAGE_TITLE_MAT_SOLID,  PAGE_TYPE_PARAM, items_mat_solid,   1);
 470→PACK_PAGE(Page_MatMicroDK, PAGE_TITLE_MAT_MICRO_DK,PAGE_TYPE_PARAM, items_mat_microdk, 1);
 471→PACK_PAGE(Page_Pipe,       PAGE_TITLE_PIPE,       PAGE_TYPE_PARAM, items_pipe,        1);
 472→PACK_PAGE(Page_Display,    PAGE_TITLE_DISPLAY,    PAGE_TYPE_PARAM, items_display,     0);
 473→PACK_PAGE(Page_Diag,       PAGE_TITLE_DIAG,       PAGE_TYPE_PARAM, items_diag,        0);
 474→PACK_PAGE(Page_Curve,      PAGE_TITLE_CURVE,      PAGE_TYPE_PARAM, items_curve,       0);
 475→PACK_PAGE(Page_Service,    PAGE_TITLE_SERVICE,    PAGE_TYPE_PARAM, items_service,     0);
 476→PACK_PAGE(Page_Current,    PAGE_TITLE_CURRENT,    PAGE_TYPE_PARAM, items_current,     0);
 477→PACK_PAGE(Page_HART,       PAGE_TITLE_HART,       PAGE_TYPE_PARAM, items_hart,        0);
 478→PACK_PAGE(Page_HARTAddr,   PAGE_TITLE_HART_ADDR,  PAGE_TYPE_PARAM, items_hart_addr,   1);
 479→PACK_PAGE(Page_Thresh,     PAGE_TITLE_THRESH,     PAGE_TYPE_PARAM, items_thresh,      0);
 480→
 481→/* --- 菜单栈机制 --- */
 482→typedef struct
 483→{
 484→    MenuPage *page;
 485→    uint8_t index;
 486→} NavState;
 487→
 488→#define MAX_NAV_DEPTH 6
 489→static NavState navStack[MAX_NAV_DEPTH];
 490→static int8_t navDepth = 0;
 491→
 492→static MenuPage *curPage = &Page_Root;
 493→static uint8_t curIndex = 0;
 494→static uint8_t topIndex = 0;
 495→
 496→static void Get_Menu_Code_String(char *buf)
 497→{
 498→    buf[0] = '\0';
 499→    char tmp[12];
 500→
 501→    if (curPage->type == PAGE_TYPE_ROOT)
 502→    {
 503→        snprintf(buf, sizeof(buf), "%d", (curIndex / 4) + 1);
 504→        return;
 505→    }
 506→    int limit = curPage->flatten_nav ? (navDepth - 1) : navDepth;
 507→    if (limit > 2) limit = 2;
 508→
 509→    for (int i = 0; i < limit; i++)
 510→    {
 511→        snprintf(tmp, sizeof(tmp), "%d-", navStack[i].index + 1);
 512→        strcat(buf, tmp);
 513→    }
 514→
 515→    if (curPage->type == PAGE_TYPE_MENU)
 516→    {
 517→        if (strlen(buf) > 0)
 518→            buf[strlen(buf) - 1] = '\0';
 519→    }
 520→    else
 521→    {
 522→        snprintf(tmp, sizeof(tmp), "%d", curIndex + 1);
 523→        strcat(buf, tmp);
 524→    }
 525→}
 526→
 527→static void Float_To_EditStr(MenuItem *item)
 528→{
 529→    if (!item || !item->value) return;
 530→    float v;
 531→    if (item->valueType == VAL_UINT8)
 532→        v = (float)(*((uint8_t *)item->value));
 533→    else if (item->valueType == VAL_FLOAT || item->valueType == 0)
 534→        v = *((float *)item->value);
 535→    else
 536→        return;
 537→    
 538→    if (item->formatStr)
 539→        snprintf(editStr, sizeof(editStr), item->formatStr, v);
 540→    else
 541→        snprintf(editStr, sizeof(editStr), "%.2f", v);
 542→    editCursorPos = 0;
 543→}
 544→
 545→static void Save_EditStr_To_Float(MenuItem *item)
 546→{
 547→    if (!item || !item->value) return;
 548→    float tempVal = (float)atof(editStr);
 549→    if (tempVal < item->min) tempVal = item->min;
 550→    if (tempVal > item->max) tempVal = item->max;
 551→    
 552→    if (item->valueType == VAL_UINT8)
 553→        *((uint8_t *)item->value) = (uint8_t)tempVal;
 554→    else if (item->valueType == VAL_FLOAT || item->valueType == 0)
 555→        *((float *)item->value) = tempVal;
 556→}
 557→
 558→static void Edit_Float(MenuItem *item, uint8_t key)
 559→{
 560→    /* 光标边界保护：确保不越出 editStr 数组 */
 561→    if (editCursorPos >= sizeof(editStr) - 1)
 562→        editCursorPos = 0;
 563→
 564→    if (key == 1)
 565→    {
 566→        char ch = editStr[editCursorPos];
 567→        if (ch >= '0' && ch <= '9')
 568→        {
 569→            if (++ch > '9') ch = '0';
 570→            editStr[editCursorPos] = ch;
 571→        }
 572→        else if (ch == '+' || ch == '-')
 573→            editStr[editCursorPos] = (ch == '+') ? '-' : '+';
 574→    }
 575→    else if (key == 2)
 576→    {
 577→        editCursorPos++;
 578→        if (editCursorPos >= sizeof(editStr) - 1 || editStr[editCursorPos] == '\0' || editStr[editCursorPos] == '%')
 579→            editCursorPos = 0;
 580→        if (editStr[editCursorPos] == '.')
 581→        {
 582→            editCursorPos++;
 583→            if (editCursorPos >= sizeof(editStr) - 1 || editStr[editCursorPos] == '\0' || editStr[editCursorPos] == '%')
 584→                editCursorPos = 0;
 585→        }
 586→    }
 587→    else if (key == 3)
 588→    {
 589→        if (editTarget == EDIT_MAIN)
 590→        {
 591→            Save_EditStr_To_Float(item);
 592→            if (item->exValue)
 593→            {
 594→                editTarget = EDIT_EXTRA;
 595→                snprintf(editStr, sizeof(editStr), item->exFormat ? item->exFormat : "%.3f",
 596→                        *(float *)item->exValue);
 597→                editCursorPos = 0;
 598→            }
 599→            else
 600→            {
 601→                gUIState = UI_BROWSE;
 602→                editTarget = EDIT_MAIN;
 603→            }
 604→        }
 605→        else
 606→        {
 607→            float tempVal = (float)atof(editStr);
 608→            if (tempVal < item->min) tempVal = item->min;
 609→            if (tempVal > item->max) tempVal = item->max;
 610→            *(float *)item->exValue = tempVal;
 611→            gUIState = UI_BROWSE;
 612→            editTarget = EDIT_MAIN;
 613→        }
 614→    }
 615→}
 616→
 617→static void Edit_String(MenuItem *item, uint8_t key)
 618→{
 619→    /* 光标边界保护：确保不越出 editStr 数组 */
 620→    if (editCursorPos >= sizeof(editStr) - 1)
 621→        editCursorPos = 0;
 622→
 623→    if (key == 1)
 624→    {
 625→        char ch = editStr[editCursorPos];
 626→        if (ch >= 'A' && ch <= 'Z')
 627→        {
 628→            if (++ch > 'Z') ch = 'A';
 629→        }
 630→        else
 631→            ch = 'A';
 632→        editStr[editCursorPos] = ch;
 633→    }
 634→    else if (key == 2)
 635→    {
 636→        editCursorPos++;
 637→        if (editCursorPos >= sizeof(editStr) - 1 || editStr[editCursorPos] == '\0')
 638→            editCursorPos = 0;
 639→    }
 640→    else if (key == 3)
 641→    {
 642→        strcpy((char *)item->value, editStr);
 643→        gUIState = UI_BROWSE;
 644→    }
 645→}
 646→
 647→static void Edit_Select(MenuItem *item, uint8_t key)
 648→{
 649→    uint8_t *v = (uint8_t *)item->value;
 650→    if (key == 1 || key == 2)
 651→    {
 652→        if (++(*v) >= item->optionNum)
 653→            *v = 0;
 654→    }
 655→    else if (key == 3)
 656→    {
 657→        gUIState = UI_BROWSE;
 658→        if (item->value == &gRadarParam.servHART && *v == 0)    //hart标准模式，地址默认0
 659→        {
 660→            gRadarParam.servHARTAddr = 0;
 661→        }
 662→        else if (item->subPage && *v == 1 && navDepth < MAX_NAV_DEPTH)
 663→        {
 664→            navStack[navDepth].page = curPage;
 665→            navStack[navDepth].index = curIndex;
 666→            navDepth++;
 667→            curPage = item->subPage;
 668→            curIndex = 0;
 669→            topIndex = 0;
 670→        }
 671→    }
 672→}
 673→
 674→static void Process_Edit_Key(MenuItem *item, uint8_t key)
 675→{
 676→    switch (item->type)
 677→    {
 678→    case MENU_FLOAT:
 679→        Edit_Float(item, key);
 680→        break;
 681→    case MENU_STRING:
 682→        Edit_String(item, key);
 683→        break;
 684→    case MENU_SELECT:
 685→        Edit_Select(item, key);
 686→        break;
 687→    default:
 688→        break;
 689→    }
 690→}
 691→
 692→static void UI_FormatItemValue(MenuItem *item, char *valBuf, char *ex1, char *ex2)
 693→{
 694→    // 1. customFormat 回调优先
 695→    if (item->customFormat != NULL)
 696→    {
 697→        item->customFormat(item, valBuf, ex1, ex2);
 698→        return;
 699→    }
 700→
 701→    // 2. SELECT 型: 从扁平化多语言 optionStr 读取选项文本
 702→    if (item->type == MENU_SELECT)
 703→    {
 704→        uint8_t v = *((uint8_t *)item->value);
 705→        uint8_t lang = gRadarParam.servLang;
 706→        if (item->optionStr && v < item->optionNum && lang < 4)
 707→            snprintf(valBuf, sizeof(valBuf), "%s", item->optionStr[lang * item->optionNum + v]);
 708→        return;
 709→    }
 710→
 711→    // 3. FLOAT / READONLY 型
 712→    if (item->type == MENU_FLOAT || item->type == MENU_READONLY)
 713→    {
 714→        float v;
 715→        if (item->valueType == VAL_UINT8)
 716→            v = (float)(*((uint8_t *)item->value));
 717→        else
 718→            v = *((float *)item->value);
 719→
 720→        if (item->formatStr)
 721→            snprintf(valBuf, sizeof(valBuf), item->formatStr, v);
 722→        else
 723→            snprintf(valBuf, sizeof(valBuf), "%.2f", v);
 724→
 725→        if (item->unitStr)
 726→        {
 727→            strcat(valBuf, " ");
 728→            strcat(valBuf, item->unitStr);
 729→        }
 730→        return;
 731→    }
 732→
 733→    // 4. STRING 型: 直接拷贝
 734→    if (item->type == MENU_STRING)
 735→    {
 736→        snprintf(valBuf, sizeof(valBuf), "%s", (char *)item->value);
 737→        return;
 738→    }
 739→}
 740→
 741→static void UI_RenderByMode(MenuItem *item, const char *valBuf,
 742→                            const char *ex1, const char *ex2, uint8_t isEditing)
 743→{
 744→    switch (item->renderMode)
 745→    {
 746→    case RENDER_PIPE:
 747→        if (*(uint8_t *)item->value == 1)
 748→        {
 749→            LCD_ShowStrExCompact(COL_CENTER_X, 2, (uint8_t *)valBuf);
 750→            LCD_ShowArrowEx(2, 3, COL_CENTER_X, (char *)valBuf);
 751→            LCD_ShowStrExCompact(COL_VALUE_X + 16, 4, (uint8_t *)UI_GetText(TEXT_PIPE_DIAMETER_LABEL));
 752→            char pipeBuf[16];
 753→            snprintf(pipeBuf, sizeof(pipeBuf), "%.0f %s", gRadarParam.pipeDiameter, UI_GetText(TEXT_PIPE_DIAMETER_UNIT));
 754→            LCD_ShowStrExCompact(COL_VALUE_X + 16, 6, (uint8_t *)pipeBuf);
 755→        }
 756→        else
 757→        {
 758→            LCD_ShowStrExCompact(COL_CENTER_X, 2, (uint8_t *)valBuf);
 759→            LCD_ShowArrowEx(2, 3, COL_CENTER_X, (char *)valBuf);
 760→        }
 761→        break;
 762→
 763→    case RENDER_PEAK:
 764→    case RENDER_STATUS:
 765→        LCD_ShowStrExCompact(COL_VALUE_X, 2, (uint8_t *)valBuf);
 766→        if (strlen(ex1) > 0)
 767→            LCD_ShowStrExCompact(COL_VALUE_X, 4, (uint8_t *)ex1);
 768→        break;
 769→
 770→    case RENDER_SERV_CURRENT:
 771→        LCD_ShowStrExCompact(COL_VALUE_X, 2, (uint8_t *)valBuf);
 772→        LCD_ShowArrowEx(2, 0, 0, NULL);
 773→        if (strlen(ex1) > 0)
 774→            LCD_ShowStrExCompact(COL_VALUE_X, 4, (uint8_t *)ex1);
 775→        LCD_ShowArrowEx(4, 0, 0, NULL);
 776→        if (strlen(ex2) > 0)
 777→            LCD_ShowStrExCompact(COL_VALUE_X, 6, (uint8_t *)ex2);
 778→        LCD_ShowArrowEx(6, 0, 0, NULL);
 779→        break;
 780→
 781→    case RENDER_SERV_HARTADDR:
 782→        if(navDepth == 3){
 783→            LCD_ShowStrEx(COL_CENTER_X, 2, (uint8_t *)valBuf); //地址设置
 784→        }
 785→        else{
 786→            LCD_ShowStrExCompact(COL_CENTER_X, 2, (uint8_t *)valBuf);
 787→        }
 788→        
 789→        LCD_ShowStrExCompact(COL_CENTER_X, 4, (uint8_t *)ex1);
 790→        break;
 791→
 792→    case RENDER_SERV_THRESH:
 793→        LCD_ShowStrExCompact(COL_MENU_ITEM, 2, (uint8_t *)valBuf);
 794→        LCD_ShowArrowEx(2, 0, 0, NULL);
 795→        LCD_ShowStrExCompact(COL_MENU_ITEM, 4, (uint8_t *)ex1);
 796→        LCD_ShowArrowEx(4, 0, 0, NULL);
 797→        break;
 798→        
 799→
 800→    case RENDER_DEFAULT:
 801→    default:
 802→        // SELECT和BOOL类型使用紧凑显示，其他类型使用默认显示
 803→        if (item->type == MENU_SELECT || item->type == MENU_BOOL) {
 804→            LCD_ShowStrExCompact(COL_CENTER_X, 2, (uint8_t *)valBuf);
 805→            if (strlen(ex1) > 0)
 806→                LCD_ShowStrExCompact(COL_CENTER_X, 4, (uint8_t *)ex1);
 807→            if (strlen(ex2) > 0)
 808→                LCD_ShowStr_Small(COL_CENTER_X + 6, 6, ex2);
 809→
 810→            if (item->type == MENU_SELECT)
 811→            {
 812→                LCD_ShowArrowEx(2, 3, COL_CENTER_X, (char *)valBuf);
 813→            }
 814→        } else {
 815→            LCD_ShowStrEx(COL_CENTER_X, 2, (uint8_t *)valBuf);
 816→            if (strlen(ex1) > 0)
 817→                LCD_ShowStrEx(COL_CENTER_X, 4, (uint8_t *)ex1);
 818→            if (strlen(ex2) > 0)
 819→                LCD_ShowStr_Small(COL_CENTER_X + 6, 6, ex2);
 820→
 821→            if (item->type == MENU_SELECT)
 822→            {
 823→                LCD_ShowArrowEx(2, 3, COL_CENTER_X, (char *)valBuf);
 824→            }
 825→        }
 826→        break;
 827→    }
 828→
 829→    if (isEditing)
 830→    {
 831→        uint8_t cursorCol = 30 + editCursorPos * 8;
 832→        uint8_t cursorPage = (editTarget == EDIT_MAIN) ? 2 : 4;
 833→        if (editCursorPos < strlen(editStr))
 834→            LCD_ShowCharReverse(cursorCol, cursorPage, editStr[editCursorPos]);
 835→    }
 836→}
 837→
 838→static void UI_RenderHome(void)
 839→{
 840→    if (home_view_mode == 0)
 841→    {
 842→        LCD_ShowStrExCompact(0, 0, (uint8_t *)UI_GetText(TEXT_HOME_NORMAL));   //首页
 843→    }
 844→    else
 845→    {                                                                              //快捷键
 846→        LCD_DrawEchoCurve(radar_echo, 1, 1);
 847→        uint8_t curveSel = gRadarParam.diagCurveSel;
 848→        uint8_t lang = gRadarParam.servLang;
 849→        if (curveSel >= 0 && curveSel < 3)
 850→            LCD_ShowStrExCompact(0, 0, (uint8_t *)dict_curve[lang * 3 + curveSel]);
 851→        else
 852→            LCD_ShowStrExCompact(0, 0, (uint8_t *)UI_GetText(TEXT_HOME_CURVE));
 853→        LCD_ShowStr_Small(110, 0, "3-4");
 854→    }
 855→}
 856→
 857→static void UI_RenderMenuList(const char *codeBuf)
 858→{
 859→    LCD_ShowStr_Small(128 - strlen(codeBuf) * 6, 0, codeBuf);
 860→
 861→    if (curPage->type == PAGE_TYPE_MENU)
 862→    {
 863→        LCD_ShowStrExCompact(0, 0, (uint8_t *)UI_GetText(curPage->titleId));
 864→        for (uint8_t i = 0; i < 3 && i < curPage->itemCount; i++)
 865→        {
 866→            if (i == curIndex)
 867→                LCD_ShowArrowEx((i + 1) * 2, 1, 0, NULL);
 868→            char nameBuf[32];
 869→            snprintf(nameBuf, sizeof(nameBuf), " %s", UI_GetText(curPage->items[i].id));
 870→            LCD_ShowStrExCompact(10, (i + 1) * 2, (uint8_t *)nameBuf);
 871→        }
 872→    }
 873→    else
 874→    {
 875→        for (uint8_t i = 0; i < 4; i++)
 876→        {
 877→            if (topIndex + i >= curPage->itemCount) break;
 878→            if ((topIndex + i) == curIndex)
 879→                LCD_ShowArrowEx(i * 2, 1, 0, NULL);
 880→            char nameBuf[32];
 881→            snprintf(nameBuf, sizeof(nameBuf), " %s", UI_GetText(curPage->items[topIndex + i].id));
 882→            LCD_ShowStrExCompact(10, i * 2, (uint8_t *)nameBuf);
 883→        }
 884→    }
 885→}
 886→
 887→static void UI_RenderParamItem(const char *codeBuf)
 888→{
 889→    MenuItem *item = &curPage->items[curIndex];
 890→    char titleBuf[32], valBuf[32] = {0}, ex1[32] = {0}, ex2[32] = {0};
 891→
 892→    // --- 标题 + 导航号 ---
 893→    snprintf(titleBuf, sizeof(titleBuf), "%s", UI_GetText(item->id));
 894→    LCD_ShowStrExCompact(0, 0, (uint8_t *)titleBuf);
 895→    LCD_ShowStr_Small(128 - strlen(codeBuf) * 6, 0, codeBuf);
 896→
 897→    // --- 特殊: 基本设置页显示当前物料性质记忆值 ---
 898→    if (curPage == &Page_Basic && item->subPage == &Page_Mat)
 899→    {
 900→        uint8_t lang = gRadarParam.servLang;
 901→        const char *matName = dict_mat[lang * 3 + gRadarParam.matType];
 902→        LCD_ShowStrExCompact(30, 2, (uint8_t *)matName);
 903→        LCD_ShowArrowEx(2, 3, 30, (char *)matName);  // 使用mode=3自动计算宽度
 904→    }
 905→
 906→    if (curPage == &Page_Pipe)
 907→    {
 908→        LCD_ShowStrExCompact(30, 2, (uint8_t *)UI_GetText(TEXT_PIPE_DIAMETER_LABEL));
 909→        char pipeBuf[16];
 910→        if (gUIState == UI_EDIT)
 911→        {
 912→            strcpy(pipeBuf, editStr);
 913→            strcat(pipeBuf, " ");
 914→            strcat(pipeBuf, UI_GetText(TEXT_PIPE_DIAMETER_UNIT));
 915→        }
 916→        else
 917→        {
 918→            snprintf(pipeBuf, sizeof(pipeBuf), "%04.0f %s", gRadarParam.pipeDiameter, UI_GetText(TEXT_PIPE_DIAMETER_UNIT));
 919→        }
 920→        LCD_ShowStrEx(30, 4, (uint8_t *)pipeBuf);
 921→
 922→        if (gUIState == UI_EDIT && editCursorPos < strlen(editStr))
 923→        {
 924→            LCD_ShowCharReverse(30 + editCursorPos * 8, 4, editStr[editCursorPos]);
 925→        }
 926→        return;
 927→    }
 928→
 929→    if (curPage == &Page_Diag && curIndex == 3)
 930→    {
 931→        LCD_ClearLine(0);
 932→		LCD_ClearLine(1);
 933→        uint8_t lang = gRadarParam.servLang;
 934→        const char *dynamicName = dict_curve[lang * 3 + gRadarParam.diagCurveSel];
 935→        LCD_ShowStrExCompact(0, 0, (uint8_t *)dynamicName);
 936→        LCD_ShowStr_Small(128 - strlen(codeBuf) * 6, 0, codeBuf);
 937→    }
 938→    else
 939→    {
 940→        snprintf(titleBuf, sizeof(titleBuf), "%s",  UI_GetText(item->id));
 941→        LCD_ShowStrExCompact(0, 0, (uint8_t *)titleBuf);
 942→
 943→    }
 944→
 945→    UI_FormatItemValue(item, valBuf, ex1, ex2);
 946→
 947→    uint8_t isEditing = (gUIState == UI_EDIT && (item->type == MENU_FLOAT || item->type == MENU_STRING));
 948→    if (isEditing)
 949→    {
 950→        if (editTarget == EDIT_MAIN)
 951→        {
 952→            strcpy(valBuf, editStr);
 953→            if (item->unitStr)
 954→            {
 955→                strcat(valBuf, " ");
 956→                strcat(valBuf, item->unitStr);
 957→            }
 958→            else if (item->customFormat != NULL && item->type == MENU_FLOAT)
 959→                strcat(valBuf, " %");
 960→        }
 961→        else
 962→        {
 963→            strcpy(ex1, editStr);
 964→            strcat(ex1, " m(d)");
 965→        }
 966→    }
 967→
 968→    // --- SELECT 编辑态: 显示全选项网格 ---
 969→    if (gUIState == UI_EDIT && item->type == MENU_SELECT)
 970→    {
 971→        uint8_t col = 0, row = 0;
 972→        uint8_t lang = gRadarParam.servLang;
 973→        for (uint8_t i = 0; i < item->optionNum; i++)
 974→        {
 975→            uint8_t x = (col == 0) ? 10 : 72;
 976→            uint8_t y = (row + 1) * 2;
 977→            if (i == *((uint8_t *)item->value))
 978→                LCD_ShowArrowEx(y, 2, x - 10, NULL);
 979→            LCD_ShowStrExCompact(x, y, (uint8_t *)item->optionStr[lang * item->optionNum + i]);
 980→            if (++row >= 3) { row = 0; col++; }
 981→        }
 982→    }
 983→    else
 984→    {
 985→        UI_RenderByMode(item, valBuf, ex1, ex2, isEditing);
 986→    }
 987→}
 988→
 989→static void UI_RenderBrowse(const char *codeBuf)
 990→{
 991→    if (curPage->type == PAGE_TYPE_ROOT || curPage->type == PAGE_TYPE_MENU)
 992→    {
 993→        UI_RenderMenuList(codeBuf);
 994→    }
 995→    else
 996→    {
 997→        UI_RenderParamItem(codeBuf);
 998→    }
 999→}
1000→
1001→static void UI_ProcessKey(uint8_t key)
1002→{
1003→    if (gUIState == UI_HOME)
1004→    {
1005→        if (key == 0)
1006→            home_view_mode = !home_view_mode;
1007→        else if (key == 3 && !home_view_mode)
1008→        {
1009→            gUIState = UI_BROWSE;
1010→            curPage = &Page_Root;
1011→            curIndex = 0;
1012→            topIndex = 0;
1013→            navDepth = 0;
1014→        }
1015→    }
1016→    else if (gUIState == UI_EDIT)
1017→    {
1018→        if (key == 0)
1019→        {
1020→            gUIState = UI_BROWSE;
1021→            editTarget = EDIT_MAIN;
1022→        }
1023→        else
1024→        {
1025→            Process_Edit_Key(&curPage->items[curIndex], key);
1026→        }
1027→    }
1028→    else if (gUIState == UI_BROWSE)
1029→    {
1030→        if (key == 0)
1031→        {
1032→            if (navDepth > 0)
1033→            {
1034→                navDepth--;
1035→                curPage = navStack[navDepth].page;
1036→                curIndex = navStack[navDepth].index;
1037→                topIndex = (curPage->type == PAGE_TYPE_ROOT) ? (curIndex / 4) * 4 : 0;
1038→            }
1039→            else
1040→            {
1041→                gUIState = UI_HOME;
1042→            }
1043→        }
1044→        else if (key == 1)
1045→        {
1046→            if (curIndex > 0)
1047→            {
1048→                curIndex--;
1049→                if (curPage->type == PAGE_TYPE_ROOT && curIndex < topIndex)
1050→                    topIndex--;
1051→            }
1052→        }
1053→        else if (key == 2)
1054→        {
1055→            if (curIndex < curPage->itemCount - 1)
1056→            {
1057→                curIndex++;
1058→                if (curPage->type == PAGE_TYPE_ROOT && curIndex >= topIndex + 4)
1059→                    topIndex++;
1060→            }
1061→            else
1062→            {
1063→                curIndex = 0;
1064→                topIndex = 0;
1065→            }
1066→        }
1067→        else if (key == 3)
1068→        {
1069→            MenuItem *item = &curPage->items[curIndex];
1070→            if (item->type == MENU_PAGE && item->subPage && navDepth < MAX_NAV_DEPTH)
1071→            {
1072→                if (curPage == &Page_Mat)
1073→                    gRadarParam.matType = curIndex;
1074→
1075→                navStack[navDepth].page = curPage;
1076→                navStack[navDepth].index = curIndex;
1077→                navDepth++;
1078→                curPage = item->subPage;
1079→                curIndex = 0;
1080→                topIndex = 0;
1081→            }
1082→            else if (item->type == MENU_ACTION)
1083→            {
1084→                if (item->actionCallback)
1085→                    item->actionCallback();
1086→            }
1087→            else if (item->type != MENU_READONLY)
1088→            {
1089→                /* 编辑前校验 value 指针有效性 */
1090→                if (!item->value)
1091→                    return;
1092→                gUIState = UI_EDIT;
1093→                editTarget = EDIT_MAIN;
1094→                if (item->type == MENU_FLOAT)
1095→                    Float_To_EditStr(item);
1096→                else if (item->type == MENU_STRING)
1097→                {
1098→                    strncpy(editStr, (char *)item->value, 15);
1099→                    editStr[15] = '\0';
1100→                    editCursorPos = 0;
1101→                }
1102→            }
1103→        }
1104→    }
1105→    uiDirty = 1;
1106→}
1107→
1108→/* ============================================================
1109→ * 公开 API
1110→ * ============================================================ */
1111→void UI_KeyK3_Back(void)  { UI_ProcessKey(0); }
1112→void UI_KeyK4_Up(void)    { UI_ProcessKey(1); }
1113→void UI_KeyK5_Loop(void)  { UI_ProcessKey(2); }
1114→void UI_KeyK6_Enter(void) { UI_ProcessKey(3); }
1115→
1116→void UI_Task(void)
1117→{
1118→    if (!uiDirty) return;
1119→    uiDirty = 0;
1120→    LCD_FullFill(0x00);
1121→
1122→    if (gUIState == UI_HOME)
1123→    {
1124→        UI_RenderHome();
1125→        return;
1126→    }
1127→
1128→    char codeBuf[16];
1129→    Get_Menu_Code_String(codeBuf);
1130→
1131→    if (curPage == &Page_Diag && curIndex == 3)
1132→    {
1133→        LCD_DrawEchoCurve(radar_echo, 1, 1);
1134→    }
1135→
1136→    UI_RenderBrowse(codeBuf);
1137→}
1138→
1139→void UI_Init(void)
1140→{
1141→    strcpy(gRadarParam.sensorTag, "SENSOR");
1142→    gRadarParam.realTimeDistance = 1.346f;
1143→    gRadarParam.pipeDiameter = 100.0f;
1144→    LCD_init();
1145→    curPage = &Page_Root;
1146→    uiDirty = 1;
1147→}
1148→
1149→/* ============================================================
1150→ * 协议数据更新接口（APP/app_disp.c 调用）
1151→ * 写入数据模型并置 uiDirty=1 触发重绘。
1152→ * 字段映射为“预留”，用户可据实际显示需求细化。
1153→ * ============================================================ */
1154→void UI_UpdateMeas(float distance, uint8_t peak_count, uint8_t mode)
1155→{
1156→    __disable_irq();
1157→    gRadarParam.realTimeDistance = distance;
1158→    gRadarParam.currMin  = peak_count;
1159→    gRadarParam.currMode = mode;
1160→    uiDirty = 1;
1161→    __enable_irq();
1162→}
1163→
1164→void UI_UpdateDiag(uint8_t reliability, uint8_t status, float peakMinEmpty, float peakMaxEmpty)
1165→{
1166→    __disable_irq();
1167→    gRadarParam.diagReliability = reliability;
1168→    gRadarParam.diagStatus      = status;
1169→    gRadarParam.peakMinEmpty    = peakMinEmpty;
1170→    gRadarParam.peakMaxEmpty    = peakMaxEmpty;
1171→    uiDirty = 1;
1172→    __enable_irq();
1173→}
1174→
1175→void UI_UpdateEcho(const uint8_t *echo, uint8_t len)
1176→{
1177→    uint8_t n = (len > 128u) ? 128u : len;
1178→    if (echo != 0 && n > 0u)
1179→    {
1180→        __disable_irq();
1181→        memcpy((uint8_t *)radar_echo, echo, n);
1182→        uiDirty = 1;
1183→        __enable_irq();
1184→    }
1185→}