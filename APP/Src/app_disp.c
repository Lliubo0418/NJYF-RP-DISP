/* 显示板侧：下行帧接收状态机 + 上行命令发送。
 *
 * 通过 BSP_USART_RegisterRxCallback 注册逐字节回调，
 * 解析  AA 55 CMD LEN PAYLOAD CRC  帧，写入 oled_ui 的 gRadarParam / radar_echo。
 *
 * BSP 层（bsp_usart.c）在 HAL_UART_RxCpltCallback 里已自动重新武装接收，
 * 这里只需在收到完整帧后交给 Disp_HandleFrame 处理。
 *
 * 设计原则：本文件只做“传输 + 帧解析 + 写入 UI 数据模型”，
 * 字段显示含义（哪些值画到哪一页）在 oled_ui 中实现。
 */
#include "app_disp.h"
#include "oled_ui.h"
#include "bsp_usart.h"
#include "dispproto.h"
#include <string.h>

/* ---------------- 接收状态机 ---------------- */
typedef enum {
    S_SYNC1 = 0,
    S_SYNC2,
    S_CMD,
    S_LEN,
    S_PAYLOAD,
    S_CRC
} DispRxState_t;

static DispRxState_t s_state = S_SYNC1;
static uint8_t s_cmd   = 0;
static uint8_t s_len   = 0;
static uint8_t s_idx   = 0;
static uint8_t s_crc   = 0;
/* 接收负载缓冲：必须容纳【所有下行命令里最长的 payload】。
 * ★原为 DISP_ECHO_LEN(128)，那是当时最长的一帧（0x02 ECHO）。
 *   新增 0x06 ECHO_TYPED 后最长变成 129（类型 1 + 数据 128），
 *   而 S_LEN 状态的守卫是 `if (b > sizeof(s_payload))` ——
 *   仍按 128 的话，LEN=129 会被判为"超长"直接丢帧，
 *   表现为"虚假回波曲线永远收不到"（静默丢帧，没有任何报错）。
 *   这里改用协议自己的上限 DISP_FRAME_MAX 相关的最大 payload，
 *   免得下次再加命令时又要回来数一遍。 */
static uint8_t s_payload[DISP_PAYLOAD_MAX];

/* 完整帧处理：把解析出来的命令和负载写入 UI 数据模型并触发重绘。
 * 仅做“传输层 -> 数据模型”的搬运，具体字段显示到哪一页为预留项。
 * 参数：
 *   cmd     - 命令字（DISP_CMD_MEAS / DISP_CMD_ECHO / DISP_CMD_DIAG）
 *   payload - 负载数据指针（指向 s_payload）
 *   len     - 负载字节数 */
static void Disp_HandleFrame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    switch (cmd)
    {
        case DISP_CMD_MEAS:
            if (len == DISP_MEAS_LEN)   /* 精确长度校验 */
            {
                float distance, position, delta_amp;
                uint8_t peak_count, mode;
                memcpy(&distance,   &payload[0], 4);
                memcpy(&position,   &payload[4], 4);
                memcpy(&delta_amp,  &payload[8], 4);
                peak_count = payload[12];
                mode       = payload[13];

                UI_UpdateMeas(distance, peak_count, mode);
                /* position / delta_amp 暂存供诊断页扩展 */
            }
            break;

        case DISP_CMD_ECHO:
            /* len 正常应 == DISP_ECHO_LEN(128) */
            UI_UpdateEcho(payload, len);
            break;

        case DISP_CMD_ECHO_TYPED:
            /* payload[0] = 曲线类型(DISP_CURVE_*)，其后为 128 点数据 */
            if (len >= DISP_ECHO_TYPED_LEN)
            {
                UI_UpdateEchoTyped(payload[0], &payload[1], DISP_ECHO_LEN);
            }
            break;

        case DISP_CMD_DIAG:
            if (len == DISP_DIAG_LEN)   /* 14 字节（含 temperature） */
            {
                uint8_t reliability = payload[0];
                uint8_t status      = payload[1];
                float peakMinEmpty, peakMaxEmpty, temperature;
                memcpy(&peakMinEmpty, &payload[2], 4);
                memcpy(&peakMaxEmpty, &payload[6], 4);
                memcpy(&temperature,  &payload[10], 4);
                UI_UpdateDiag(reliability, status, peakMinEmpty, peakMaxEmpty, temperature);
            }
            break;

        case DISP_CMD_INFO:
            if (len == DISP_INFO_LEN)
            {
                /* sensorType(u8) verMajor(u8) verMinor(u8)，暂存供显示扩展 */
                uint8_t sensorType = payload[0];
                (void)sensorType;
            }
            break;

        case DISP_CMD_PARAM_DUMP:
            /* 全量配置：按 dispproto.h 定义的 81 字节布局逐字段反序列化写入 gRadarParam */
            UI_UpdateParamDump(payload, len);
            break;

        default:
            break;
    }
}

/* 逐字节接收回调：HAL 在每次收到 1 字节后调用，驱动状态机解析整帧。
 * 状态流转：S_SYNC1 -> S_SYNC2 -> S_CMD -> S_LEN -> S_PAYLOAD -> S_CRC，
 *   收到完整且 CRC 正确的帧即交给 Disp_HandleFrame；BSP 层已自动重武装接收。
 * 参数：
 *   instance - 触发回调的 USART 实例（此处恒为 USART1，函数内未使用）
 *   b        - 本次收到的 1 字节数据 */
void Disp_OnRxByte(BSP_USART_Instance_t instance, uint8_t b)
{
    (void)instance;
    switch (s_state)
    {
        case S_SYNC1:
            if (b == DISP_SYNC1) s_state = S_SYNC2;
            break;
        case S_SYNC2:
            if (b == DISP_SYNC2)      s_state = S_CMD;
            else if (b == DISP_SYNC1) s_state = S_SYNC2;   /* 容错：连续 0xAA */
            else                      s_state = S_SYNC1;
            break;
        case S_CMD:
            s_cmd = b;
            s_crc = b;
            s_state = S_LEN;
            break;
        case S_LEN:
            if (b > sizeof(s_payload))
            {
                /* 负载超长：丢弃帧，回到同步头 */
                s_state = S_SYNC1;
                break;
            }
            s_len = b;
            s_crc ^= b;
            s_idx = 0;
            s_state = (b > 0) ? S_PAYLOAD : S_CRC;
            break;
        case S_PAYLOAD:
            s_payload[s_idx++] = b;
            s_crc ^= b;
            if (s_idx >= s_len) s_state = S_CRC;
            break;
        case S_CRC:
            if (b == s_crc)
            {
                Disp_HandleFrame(s_cmd, s_payload, s_len);
            }
            /* 无论校验对错，回到同步头重新等待 */
            s_state = S_SYNC1;
            break;
        default:
            s_state = S_SYNC1;
            break;
    }
}

/* 协议初始化：注册 USART1 接收回调并启动逐字节接收。
 * 必须在 BSP_USART_Init() 之后调用（main.c 中已挂接）。
 * 参数：无 */
void Disp_Init(void)
{
    BSP_USART_RegisterRxCallback(BSP_USART_INSTANCE_1, Disp_OnRxByte);
    BSP_USART_StartRx(BSP_USART_INSTANCE_1);   /* 启动 USART1 逐字节接收 */
    /* 开机配置同步：请求主板下发全量配置 PARAM_DUMP，覆盖本地默认值 */
    Disp_UpRequestParamDump();
}

/* ---------------- 上行命令发送（显示板 -> 主板） ---------------- */

/* 上行帧组装并发送：把 cmd + payload 按 AA 55 帧格式封装后发往主板。
 * 参数：
 *   cmd     - 上行命令字（DISP_CMD_REQ_ECHO / REQ_MEAS / KEY / SET_PARAM）
 *   payload - 负载数据指针（无负载时传 NULL）
 *   len     - 负载字节数 */
static void Disp_UpSendFrame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t frame[2 + 1 + 1 + DISP_ECHO_LEN + 1];
    uint8_t idx = 0;
    frame[idx++] = DISP_SYNC1;
    frame[idx++] = DISP_SYNC2;
    frame[idx++] = cmd;
    frame[idx++] = len;
    for (uint8_t i = 0; i < len; i++) frame[idx++] = payload[i];
    frame[idx++] = Disp_CRC(cmd, payload, len);
    BSP_USART_Send(BSP_USART_INSTANCE_1, frame, idx, 100);
}

/* 上行：请求主板回送一帧 ECHO 回波数据（命令 0x81，无负载）。
 * 参数：无 */
void Disp_UpRequestEcho(void) { Disp_UpSendFrame(DISP_CMD_REQ_ECHO, 0, 0); }

/* 上行：请求主板回送一帧 MEAS 测量数据（命令 0x82，无负载）。
 * 参数：无 */
void Disp_UpRequestMeas(void) { Disp_UpSendFrame(DISP_CMD_REQ_MEAS, 0, 0); }

/* 上行：把显示板按键码转发给主板（命令 0x83）。
 * 参数：
 *   key - 按键编码（1 字节，业务自定义含义） */
void Disp_UpSendKey(uint8_t key) { Disp_UpSendFrame(DISP_CMD_KEY, &key, 1); }

/* 上行：向主板设置某个参数（命令 0x84）。
 * 参数：
 *   id    - 参数编号（1 字节，业务自定义）
 *   value - 参数值（float，按小端打包成 4 字节负载） */
void Disp_UpSetParam(uint8_t id, float value)
{
    uint8_t payload[5];
    payload[0] = id;
    memcpy(&payload[1], &value, 4);
    Disp_UpSendFrame(DISP_CMD_SET_PARAM, payload, 5);
}

/* 上行：设置字符串参数（命令 0x85）。
 * 参数：id - 参数编号；str - 字符串（最长 250 字节，超过截断） */
void Disp_UpSendStr(uint8_t id, const char *str)
{
    uint8_t payload[256];
    uint8_t slen = 0;
    if (str != 0)
    {
        while (str[slen] != '\0' && slen < 250u) slen++;
    }
    payload[0] = id;
    payload[1] = slen;
    if (slen > 0u) memcpy(&payload[2], str, slen);
    Disp_UpSendFrame(DISP_CMD_SET_STR, payload, (uint8_t)(2u + slen));
}

/* 上行：请求传感器信息（命令 0x86，无负载） */
void Disp_UpRequestInfo(void) { Disp_UpSendFrame(DISP_CMD_REQ_INFO, 0, 0); }

/* 上行：请求全量配置（命令 0x87，无负载） */
void Disp_UpRequestParamDump(void) { Disp_UpSendFrame(DISP_CMD_REQ_PARAM_DUMP, 0, 0); }
