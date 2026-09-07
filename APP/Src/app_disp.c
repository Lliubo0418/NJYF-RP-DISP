/* 显示板侧：自定义协议接收状态机 + 上行命令发送（预留）
 *
 * 通过 BSP_USART_RegisterRxCallback 注册逐字节回调，
 * 解析  AA 55 CMD LEN PAYLOAD CRC  帧，写入 oled_ui 的 gRadarParam / radar_echo。
 *
 * BSP 层（bsp_usart.c）在 HAL_UART_RxCpltCallback 里已自动重新武装接收，
 * 这里只需在收到完整帧后交给 Disp_HandleFrame 处理。
 *
 * 设计原则：本文件只做“传输 + 帧解析 + 写入 UI 数据模型”，
 * 具体的字段显示含义（哪些值画到哪一页）是预留项，用户按需细化。
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
static uint8_t s_payload[DISP_ECHO_LEN];   /* 最大 payload = 128 */

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
            if (len >= DISP_MEAS_LEN)
            {
                float distance, position, delta_amp;
                uint8_t peak_count, mode;
                memcpy(&distance,   &payload[0], 4);
                memcpy(&position,   &payload[4], 4);
                memcpy(&delta_amp,  &payload[8], 4);
                peak_count = payload[12];
                mode       = payload[13];

                UI_UpdateMeas(distance, peak_count, mode);
                /* TODO(用户补充)：position / delta_amp 的显示用途可自行扩展 */
            }
            break;

        case DISP_CMD_ECHO:
            /* len 正常应 == DISP_ECHO_LEN(128) */
            UI_UpdateEcho(payload, len);
            break;

        case DISP_CMD_DIAG:
            if (len >= DISP_DIAG_LEN)
            {
                uint8_t reliability = payload[0];
                uint8_t status      = payload[1];
                float peakMinEmpty, peakMaxEmpty;
                memcpy(&peakMinEmpty, &payload[2], 4);
                memcpy(&peakMaxEmpty, &payload[6], 4);
                UI_UpdateDiag(reliability, status, peakMinEmpty, peakMaxEmpty);
            }
            break;

        default:
            /* 未知命令：预留扩展 / 调试计数 */
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
}

/* ---------------- 上行命令发送（预留） ---------------- */

/* 上行帧组装并发送：把 cmd + payload 按 AA 55 帧格式封装后发往主板。
 * 参数：
 *   cmd     - 上行命令字（DISP_CMD_REQ_ECHO / REQ_MEAS / KEY / SET_PARAM）
 *   payload - 负载数据指针（无负载时传 NULL）
 *   len     - 负载字节数 */
static void Disp_SendUplink(uint8_t cmd, const uint8_t *payload, uint8_t len)
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
void Disp_RequestEcho(void) { Disp_SendUplink(DISP_CMD_REQ_ECHO, 0, 0); }

/* 上行：请求主板回送一帧 MEAS 测量数据（命令 0x82，无负载）。
 * 参数：无 */
void Disp_RequestMeas(void) { Disp_SendUplink(DISP_CMD_REQ_MEAS, 0, 0); }

/* 上行：把显示板按键码转发给主板（命令 0x83）。
 * 参数：
 *   key - 按键编码（1 字节，业务自定义含义） */
void Disp_SendKey(uint8_t key) { Disp_SendUplink(DISP_CMD_KEY, &key, 1); }

/* 上行：向主板设置某个参数（命令 0x84，预留）。
 * 参数：
 *   id    - 参数编号（1 字节，业务自定义）
 *   value - 参数值（float，按小端打包成 4 字节负载） */
void Disp_SetParam(uint8_t id, float value)
{
    uint8_t payload[5];
    payload[0] = id;
    memcpy(&payload[1], &value, 4);
    Disp_SendUplink(DISP_CMD_SET_PARAM, payload, 5);
}
