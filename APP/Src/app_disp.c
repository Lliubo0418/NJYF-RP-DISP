#include "app_disp.h"
#include "oled_ui.h"
#include "bsp_usart.h"
#include "dispproto.h"
#include "main.h"
#include <string.h>

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
static uint8_t s_payload[DISP_PAYLOAD_MAX];

#if DISP_PERIODIC_REQ_ENABLE
static volatile uint8_t  s_per_dump_rx      = 0u;
static volatile uint32_t s_per_last_down_ms = 0u;
static uint8_t  s_per_synced       = 0u;
static uint8_t  s_per_silence_seen = 0u;
static uint8_t  s_per_tries        = 0u;
static uint32_t s_per_next_ms      = 0u;
static uint8_t  s_per_inited       = 0u;
#endif

#define ACK_SLOT_MAX        2u
#define ACK_TIMEOUT_MS      500u
#define ACK_TIMEOUT_SLOW_MS 3000u
#define ACK_RETRY_MAX       1u     /* 超时后允许重发 1 次 */


static void Disp_SendFrame(uint8_t cmd, const uint8_t *payload, uint8_t len);

typedef struct
{
    uint8_t  used;
    uint8_t  cmd;
    uint8_t  id;
    uint8_t  len;
    uint8_t  payload[2u + DISP_UP_STR_MAX];
    uint32_t t_ms;
    uint8_t  retried;
} DispAckSlot_t;

static DispAckSlot_t s_ack_slot[ACK_SLOT_MAX];
static volatile uint8_t s_ack_state  = DISP_ACK_STATE_NONE;
static volatile uint8_t s_ack_result = 0xFFu;
static uint8_t s_ack_failed = 0u;
static uint8_t s_ack_event  = 0u;   /* 有新结果待 UI 提示 */

/**
 * @brief  该参数超时未回 ACK 时能否安全重发
 * @param  id 参数编号
 * @retval 1=可重发，0=禁止
 */
static uint8_t Disp_SendAckRetrySafe(uint8_t id)
{
    if (id == (uint8_t)DPARAM_SERV_FALSE_ECHO) return 0u;
    if (id == (uint8_t)DPARAM_SERV_RESET)      return 0u;
    return 1u;
}

/**
 * @brief  该参数的 ACK 超时门限
 * @param  id 参数编号
 * @retval 超时时间
 */
static uint32_t Disp_SendAckTimeoutMs(uint8_t id)
{
    if (id == (uint8_t)DPARAM_SERV_FALSE_ECHO) return ACK_TIMEOUT_SLOW_MS;
    if (id == (uint8_t)DPARAM_SERV_RESET)      return ACK_TIMEOUT_SLOW_MS;
    return ACK_TIMEOUT_MS;
}

/**
 * @brief  登记一帧待确认的写操作
 * @param  cmd     命令字
 * @param  payload 负载
 * @param  len     负载字节数
 * @retval 无
 */
static void Disp_SendAckTrack(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t i, slot = 0xFFu;

    if (len == 0u || len > sizeof(s_ack_slot[0].payload)) return;

    if (s_ack_state != DISP_ACK_STATE_WAIT)
    {
        s_ack_failed = 0u;
        for (i = 0; i < ACK_SLOT_MAX; i++) s_ack_slot[i].used = 0u;
    }

    for (i = 0; i < ACK_SLOT_MAX; i++)
    {
        if (!s_ack_slot[i].used) { slot = i; break; }
    }
    if (slot == 0xFFu) slot = 0u;

    s_ack_slot[slot].cmd     = cmd;
    s_ack_slot[slot].id      = payload[0];
    s_ack_slot[slot].len     = len;
    s_ack_slot[slot].t_ms    = HAL_GetTick();
    s_ack_slot[slot].retried = 0u;
    memcpy(s_ack_slot[slot].payload, payload, len);
    s_ack_slot[slot].used    = 1u;

    s_ack_state = DISP_ACK_STATE_WAIT;
}

/**
 * @brief  收到 ACK 帧
 * @param  payload ACK 负载
 * @param  len     负载字节数
 * @retval 无
 */
static void Disp_RecvHandleAck(const uint8_t *payload, uint8_t len)
{
    uint8_t i, done = 1u;

    if (payload == 0 || len != DISP_ACK_LEN) return;

    for (i = 0; i < ACK_SLOT_MAX; i++)
    {
        if (s_ack_slot[i].used &&
            s_ack_slot[i].cmd == payload[0] &&
            s_ack_slot[i].id  == payload[2])
        {
            s_ack_slot[i].used = 0u;
            if (payload[1] != DISP_ACK_OK)
            {
                s_ack_failed = 1u;
                s_ack_result = payload[1];
            }
            break;
        }
    }

    for (i = 0; i < ACK_SLOT_MAX; i++)
    {
        if (s_ack_slot[i].used) { done = 0u; break; }
    }

    if (done && s_ack_state == DISP_ACK_STATE_WAIT)
    {
        s_ack_state = s_ack_failed ? DISP_ACK_STATE_FAIL : DISP_ACK_STATE_OK;
        if (!s_ack_failed) s_ack_result = DISP_ACK_OK;
        s_ack_event = 1u;
    }
}

/**
 * @brief  主循环轮询：ACK 超时判定与重发
 * @retval 无
 */
void Disp_SendAckPoll(void)
{
    uint32_t now = HAL_GetTick();
    uint8_t i, done = 1u, resend = 0xFFu;


    __disable_irq();
    for (i = 0; i < ACK_SLOT_MAX; i++)
    {
        DispAckSlot_t *s = &s_ack_slot[i];
        if (!s->used) continue;

        done = 0u;   

        if ((now - s->t_ms) < Disp_SendAckTimeoutMs(s->id)) continue;

        if (s->retried < ACK_RETRY_MAX && Disp_SendAckRetrySafe(s->id))
        {
            s->retried++;
            s->t_ms = now;
            if (resend == 0xFFu) resend = i;
            continue;
        }


        s->used      = 0u;
        s_ack_failed = 1u;
        s_ack_result = 0xFFu;
    }

    if (done && s_ack_state == DISP_ACK_STATE_WAIT)
    {
        s_ack_state = s_ack_failed ? DISP_ACK_STATE_FAIL : DISP_ACK_STATE_OK;
        if (!s_ack_failed) s_ack_result = DISP_ACK_OK;
        s_ack_event = 1u;
    }
    __enable_irq();

    if (resend != 0xFFu)
    {
        Disp_SendFrame(s_ack_slot[resend].cmd,
                         s_ack_slot[resend].payload,
                         s_ack_slot[resend].len);
    }

#if DISP_PERIODIC_REQ_ENABLE
    Disp_SendPeriodicTask();
#endif
}

#if DISP_PERIODIC_REQ_ENABLE
/**
 * @brief  配置同步握手：未同步时重发 0x87
 * @retval 无
 */
void Disp_SendPeriodicTask(void)
{
    uint32_t now = HAL_GetTick();

    if (s_per_inited == 0u)
    {
        s_per_inited       = 1u;
        s_per_next_ms      = now + DISP_PERIODIC_HANDSHAKE_MS;
        s_per_last_down_ms = now;
        return;
    }


    __disable_irq();
    if (s_per_dump_rx != 0u)
    {
        s_per_dump_rx = 0u;
        __enable_irq();
        s_per_synced       = 1u;
        s_per_silence_seen = 0u;
        s_per_tries        = 0u;
        return;
    }
    __enable_irq();

    if (s_per_synced != 0u)
    {
#if DISP_PERIODIC_RESYNC_ENABLE
        uint32_t last = s_per_last_down_ms;
        if ((uint32_t)(now - last) >= DISP_PERIODIC_SILENCE_MS)
        {
            s_per_silence_seen = 1u;
        }
        else if (s_per_silence_seen != 0u)
        {
            s_per_silence_seen = 0u;
            s_per_synced       = 0u;
            s_per_tries        = 0u;
            s_per_next_ms      = now;
        }
#endif
        return;
    }

    if ((int32_t)(now - s_per_next_ms) >= 0)
    {
        uint32_t period = (s_per_tries < DISP_PERIODIC_FAST_TRIES)
                        ? (uint32_t)DISP_PERIODIC_HANDSHAKE_MS
                        : (uint32_t)DISP_PERIODIC_SLOW_MS;
        s_per_next_ms = now + period;
        if (s_per_tries < 0xFFu) s_per_tries++;
        Disp_SendRequestParamDump();
    }
}
#endif

/**
 * @brief  查询当前写操作的确认状态
 * @retval 
 */
uint8_t Disp_SendParamAckState(void) { return s_ack_state; }

/**
 * @brief  查询最近一次 ACK 的结果码
 * @retval 
 */
uint8_t Disp_SendParamAckResult(void) { return s_ack_result; }

/**
 * @brief  取走并清零"有新的确认结果"事件
 * @retval 1=有新结果，0=无
 */
uint8_t Disp_SendTakeParamAckEvent(void)
{
    uint8_t e = s_ack_event;
    s_ack_event = 0u;
    return e;
}

/**
 * @brief  完整帧处理：写入 UI 数据模型并触发重绘
 * @param  cmd     命令字
 * @param  payload 负载指针
 * @param  len     负载字节数
 * @retval 无
 */
static void Disp_RecvHandleFrame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    switch (cmd)
    {
        case DISP_CMD_MEAS:
            if (len == DISP_MEAS_LEN)   
            {
                float distance, position, delta_amp;
                uint8_t peak_count, mode;
                memcpy(&distance,   &payload[0], 4);
                memcpy(&position,   &payload[4], 4);
                memcpy(&delta_amp,  &payload[8], 4);
                peak_count = payload[12];
                mode       = payload[13];

                UI_UpdateMeas(distance, peak_count, mode);
            }
            break;

        case DISP_CMD_ECHO:
            UI_UpdateEcho(payload, len);
            break;

        case DISP_CMD_ECHO_TYPED:
            if (len >= DISP_ECHO_TYPED_LEN)
            {
                UI_UpdateEchoTyped(payload[0], &payload[1], DISP_ECHO_LEN);
            }
            break;

        case DISP_CMD_DIAG:
            if (len == DISP_DIAG_LEN)
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
                uint8_t sensorType = payload[0];
                (void)sensorType;
            }
            break;

        case DISP_CMD_PARAM_DUMP:
            /* 全量配置 */
            UI_UpdateParamDump(payload, len);
#if DISP_PERIODIC_REQ_ENABLE
            s_per_dump_rx = 1u;
#endif
            break;

        case DISP_CMD_ACK:
            Disp_RecvHandleAck(payload, len);
            break;

        default:
            break;
    }
}

/**
 * @brief  逐字节接收回调
 * @param  instance USART 实例
 * @param  b        本次收到的字节
 * @retval 无
 */
void Disp_RecvByte(BSP_USART_Instance_t instance, uint8_t b)
{
    (void)instance;
    switch (s_state)
    {
        case S_SYNC1:
            if (b == DISP_SYNC1) s_state = S_SYNC2;
            break;
        case S_SYNC2:
            if (b == DISP_SYNC2)      s_state = S_CMD;
            else if (b == DISP_SYNC1) s_state = S_SYNC2;
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
#if DISP_PERIODIC_REQ_ENABLE
                if (s_cmd != DISP_CMD_PARAM_DUMP)
                {
                    s_per_last_down_ms = HAL_GetTick();
                }
#endif
                Disp_RecvHandleFrame(s_cmd, s_payload, s_len);
            }
            s_state = S_SYNC1;
            break;
        default:
            s_state = S_SYNC1;
            break;
    }
}

/**
 * @brief  初始化上行接收
 * @retval 无
 */
void Disp_Init(void)
{
    /*  必须在 BSP_USART_Init 之后调用 */
    BSP_USART_RegisterRxCallback(BSP_USART_INSTANCE_1, Disp_RecvByte);
    BSP_USART_StartRx(BSP_USART_INSTANCE_1);   

    Disp_SendRequestParamDump();
}

/* ---------------- 发送：上行命令 ---------------- */

/**
 * @brief  组装并发送一帧
 * @param  cmd     上行命令字
 * @param  payload 负载指针
 * @param  len     负载字节数
 * @retval 无
 */
static void Disp_SendFrame(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t frame[DISP_FRAME_MAX];
    uint16_t idx = 0;
    frame[idx++] = DISP_SYNC1;
    frame[idx++] = DISP_SYNC2;
    frame[idx++] = cmd;
    frame[idx++] = len;
    for (uint8_t i = 0; i < len; i++) frame[idx++] = payload[i];
    frame[idx++] = Disp_CRC(cmd, payload, len);
    BSP_USART_Send(BSP_USART_INSTANCE_1, frame, idx, 100);
}

/**
 * @brief  上行请求主板回送一帧 ECHO 回波数据
 * @retval 无
 */
void Disp_SendRequestEcho(void) { Disp_SendFrame(DISP_CMD_REQ_ECHO, 0, 0); }

/**
 * @brief  上行请求主板回送一帧 MEAS 测量数据
 * @retval 无
 */
void Disp_SendRequestMeas(void) { Disp_SendFrame(DISP_CMD_REQ_MEAS, 0, 0); }

/**
 * @brief  上行转发按键码给主板
 * @param  key 按键编码
 * @retval 无
 */
void Disp_SendKey(uint8_t key) { Disp_SendFrame(DISP_CMD_KEY, &key, 1); }

/**
 * @brief  上行设置某个参数
 * @param  id    参数编号
 * @param  value 参数值
 * @retval 无
 */
void Disp_SendSetParam(uint8_t id, float value)
{
    uint8_t payload[5];
    payload[0] = id;
    memcpy(&payload[1], &value, 4);
    Disp_SendAckTrack(DISP_CMD_SET_PARAM, payload, 5);
    Disp_SendFrame(DISP_CMD_SET_PARAM, payload, 5);
}

/**
 * @brief  上行设置字符串参数
 * @param  id  参数编号
 * @param  str 字符串
 * @retval 无
 */
void Disp_SendStr(uint8_t id, const char *str)
{
    uint8_t payload[2u + DISP_UP_STR_MAX];
    uint8_t slen = 0;
    if (str != 0)
    {
        while (slen < DISP_UP_STR_MAX && str[slen] != '\0') slen++;
    }
    payload[0] = id;
    payload[1] = slen;
    if (slen > 0u) memcpy(&payload[2], str, slen);
    Disp_SendAckTrack(DISP_CMD_SET_STR, payload, (uint8_t)(2u + slen));   /* 先登记再发 */
    Disp_SendFrame(DISP_CMD_SET_STR, payload, (uint8_t)(2u + slen));
}

/**
 * @brief  上行请求传感器信息
 * @retval 无
 */
void Disp_SendRequestInfo(void) { Disp_SendFrame(DISP_CMD_REQ_INFO, 0, 0); }

/**
 * @brief  上行请求全量配置
 * @retval 无
 */
void Disp_SendRequestParamDump(void) { Disp_SendFrame(DISP_CMD_REQ_PARAM_DUMP, 0, 0); }
