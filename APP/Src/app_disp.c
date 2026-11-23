/* 显示板侧：下行帧接收状态机 + 上行命令发送。
 * 解析 AA 55 CMD LEN PAYLOAD CRC 帧，写入 oled_ui 的 gRadarParam / radar_echo；
 * BSP 层（bsp_usart.c）已自动重武装接收，本文件只负责收齐整帧后交给 Disp_RecvHandleFrame。
 * 本文件只做传输 + 帧解析 + 写数据模型，字段显示含义在 oled_ui 中实现。 */
#include "app_disp.h"
#include "oled_ui.h"
#include "bsp_usart.h"
#include "dispproto.h"
#include "main.h"      /* HAL_GetTick：ACK 超时/重发计时（见下方 Disp_SendAckPoll） */
#include <string.h>

/* ---------------- 接收：下行帧状态机（主板 -> 显示板） ---------------- */
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
/* 接收负载缓冲：须容纳最长的下行 payload（当前最长 129 = ECHO_TYPED）
 * ⚠ 改小则超长帧被 S_LEN 守卫静默丢弃，表现为该曲线永远收不到且无报错 */
static uint8_t s_payload[DISP_PAYLOAD_MAX];

#if DISP_PERIODIC_REQ_ENABLE
/* 配置同步握手状态：s_per_dump_rx / s_per_last_down_ms 由接收中断写、主循环读，
 * ⚠ 必须 volatile；中断里只做单次赋值，不做状态迁移 */
static volatile uint8_t  s_per_dump_rx      = 0u;  /* 收到 0x05 PARAM_DUMP */
static volatile uint32_t s_per_last_down_ms = 0u;  /* 最近一次非-0x05 下行帧时刻 */
static uint8_t  s_per_synced       = 0u;  /* 1 = 已拿到全量配置 */
static uint8_t  s_per_silence_seen = 0u;  /* 1 = 已观察到下行静默 */
static uint8_t  s_per_tries        = 0u;  /* 快重发计数 */
static uint32_t s_per_next_ms      = 0u;  /* 下次重发时刻 */
static uint8_t  s_per_inited       = 0u;
#endif

/* ---------------- 写参数 ACK 跟踪（0x07） ----------------
 * 收 ACK 在 USART1 接收中断里，只允许"遍历 slot + 赋值"的 O(1) 操作；
 * 超时重发在主循环 Disp_SendAckPoll()。 */
#define ACK_SLOT_MAX        2u     /* 一次确认最多连发 2 帧 */
#define ACK_TIMEOUT_MS      500u   /* 普通参数：主板写 81B 配置只需几十 ms，留 10 倍余量 */
#define ACK_TIMEOUT_SLOW_MS 3000u  /* 虚假回波学习/复位：会重写 10KB 基线（≈160ms） */
#define ACK_RETRY_MAX       1u     /* 超时后允许重发 1 次 */

/* 前向声明：Disp_SendAckPoll() 重发要复用上行发送函数（定义在本文件末段） */
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
static uint8_t s_ack_failed = 0u;   /* 本轮写操作是否已出现失败 */
static uint8_t s_ack_event  = 0u;   /* 有新结果待 UI 提示（读取即清） */

/**
 * @brief  该参数超时未回 ACK 时能否安全重发
 * @param  id 参数编号（DISP_PARAM_ID）
 * @retval 1=可重发，0=禁止（仅"重复执行等于执行一次"的参数才允许）
 */
static uint8_t Disp_SendAckRetrySafe(uint8_t id)
{
    /* ⚠ SERV_FALSE_ECHO 重发 = 重复学习空罐基线（重写 10KB 且采样全停）；
     *   SERV_RESET 重发 = 重复复位（会把用户刚改的其他参数再冲掉一轮） */
    if (id == (uint8_t)DPARAM_SERV_FALSE_ECHO) return 0u;
    if (id == (uint8_t)DPARAM_SERV_RESET)      return 0u;
    return 1u;
}

/**
 * @brief  该参数的 ACK 超时门限
 * @param  id 参数编号（DISP_PARAM_ID）
 * @retval 超时时间(ms)
 */
static uint32_t Disp_SendAckTimeoutMs(uint8_t id)
{
    if (id == (uint8_t)DPARAM_SERV_FALSE_ECHO) return ACK_TIMEOUT_SLOW_MS;
    if (id == (uint8_t)DPARAM_SERV_RESET)      return ACK_TIMEOUT_SLOW_MS;
    return ACK_TIMEOUT_MS;
}

/**
 * @brief  登记一帧待确认的写操作（必须在真正发送之前调用）
 * @param  cmd     命令字
 * @param  payload 负载
 * @param  len     负载字节数
 * @retval 无
 */
static void Disp_SendAckTrack(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t i, slot = 0xFFu;

    if (len == 0u || len > sizeof(s_ack_slot[0].payload)) return;

    /* 新一轮（上一轮已确认完）：清失败标志与残留 slot */
    if (s_ack_state != DISP_ACK_STATE_WAIT)
    {
        s_ack_failed = 0u;
        for (i = 0; i < ACK_SLOT_MAX; i++) s_ack_slot[i].used = 0u;
    }

    for (i = 0; i < ACK_SLOT_MAX; i++)
    {
        if (!s_ack_slot[i].used) { slot = i; break; }
    }
    if (slot == 0xFFu) slot = 0u;   /* 理论不可达（一次确认 ≤2 帧）；满了覆盖最旧 */

    /* ⚠ 先把整条记录填满，最后才置 used=1：反序时接收中断可能看到 cmd 还是旧值的
     *   半成品记录，匹配不上 ⇒ 这一帧白等一个超时周期 */
    s_ack_slot[slot].cmd     = cmd;
    s_ack_slot[slot].id      = payload[0];
    s_ack_slot[slot].len     = len;
    s_ack_slot[slot].t_ms    = HAL_GetTick();
    s_ack_slot[slot].retried = 0u;
    memcpy(s_ack_slot[slot].payload, payload, len);
    s_ack_slot[slot].used    = 1u;   /* 发布点：此后接收中断才会认这条记录 */

    s_ack_state = DISP_ACK_STATE_WAIT;
}

/**
 * @brief  收到 ACK 帧：按 cmd+id 匹配 slot 并汇总结果（运行在接收中断里，O(1)）
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
 * @brief  主循环轮询：ACK 超时判定与重发（非阻塞，与 UI_Task 同频调用）
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
            if (resend == 0xFFu) resend = i;   /* 每轮最多重发一帧，多帧下轮继续 */
            continue;
        }

        
        s->used      = 0u;
        s_ack_failed = 1u;
        s_ack_result = 0xFFu;   /* 0xFF = 主板无应答 */
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
    Disp_SendPeriodicTask();   /* 配置同步握手：随主循环推进，非阻塞 */
#endif
}

/* ---------------- 配置同步握手 ----------------
 * 由 Disp_SendAckPoll() 末尾驱动，不依赖额外定时器中断；状态迁移全在主循环，中断只置标志。 */
#if DISP_PERIODIC_REQ_ENABLE
/**
 * @brief  配置同步握手：未同步时重发 0x87，已同步时只监测下行静默后的恢复沿
 * @retval 无
 */
void Disp_SendPeriodicTask(void)
{
    uint32_t now = HAL_GetTick();

    if (s_per_inited == 0u)
    {
        s_per_inited       = 1u;
        s_per_next_ms      = now + DISP_PERIODIC_HANDSHAKE_MS;
        s_per_last_down_ms = now;   /* 从上电起算静默 */
        return;   /* 首轮只定相位：让 Disp_Init() 的 0x87 先走 */
    }

    /* ① 握手应答优先：拿到 0x05 即完成同步，上行立即归零。
     * ⚠ 标志由接收中断写，读-清必须成临界区，否则"读走后、清零前"来的 0x05 会被清掉 */
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

    /* ② 已同步：不发任何周期帧，只监测"主板重启后的下行恢复沿" */
    if (s_per_synced != 0u)
    {
#if DISP_PERIODIC_RESYNC_ENABLE
        /* 单次读取中断写的时刻，避免判定过程中被改写导致门限抖动 */
        uint32_t last = s_per_last_down_ms;
        if ((uint32_t)(now - last) >= DISP_PERIODIC_SILENCE_MS)
        {
            s_per_silence_seen = 1u;
        }
        else if (s_per_silence_seen != 0u)
        {
            /* ⚠ 静默后又出现非-0x05 下行帧 ⇒ 主板可能单独重启，本板 gRadarParam 仍是旧副本。
             *   事件触发，不是周期请求 */
            s_per_silence_seen = 0u;
            s_per_synced       = 0u;
            s_per_tries        = 0u;
            s_per_next_ms      = now;   /* 下一拍立即重发 0x87 */
        }
#endif
        return;
    }

    /* ③ 未同步：握手重发。快 N 次后降频，避免主板长期缺席时刷总线。
     * 用 deadline 推进而非"上次时刻 + 周期"，主循环偶发长耗时只跳拍、不补发突发 */
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
 * @retval DISP_ACK_STATE_*
 */
uint8_t Disp_SendParamAckState(void) { return s_ack_state; }

/**
 * @brief  查询最近一次 ACK 的结果码
 * @retval DISP_ACK_*
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
 * @brief  完整帧处理：把命令与负载写入 UI 数据模型并触发重绘（纯搬运，不做显示映射）
 * @param  cmd     命令字
 * @param  payload 负载指针（指向 s_payload）
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
                /* ⚠ position / delta_amp 已解析但未接入 UI（预留）：
                 *   position = 峰的亚采样下标（用于波形标峰值），delta_amp = 峰处差分幅度（诊断信号强度）
                 *   接入方式：UI_UpdateMeas 加形参 → gRadarParam 加字段 → LCD_DrawEchoCurve 加标记绘制 */
            }
            break;

        case DISP_CMD_ECHO:
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
            /* 全量配置：按 dispproto.h 的 81 字节布局逐字段反序列化写入 gRadarParam */
            UI_UpdateParamDump(payload, len);
#if DISP_PERIODIC_REQ_ENABLE
            s_per_dump_rx = 1u;   /* 中断里只置标志，状态迁移留给 Disp_SendPeriodicTask */
#endif
            break;

        case DISP_CMD_ACK:
            /* ⚠ 本分支运行在接收中断里，只允许 O(1)；UI 提示由主循环取事件后画 */
            Disp_RecvHandleAck(payload, len);
            break;

        default:
            break;
    }
}

/**
 * @brief  逐字节接收回调（ISR 上下文）：驱动状态机，CRC 通过即交 Disp_RecvHandleFrame
 * @param  instance USART 实例（恒为 USART1，未使用）
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
                /* 记录有效下行帧到达时刻。⚠ PARAM_DUMP 必须排除：它由本板握手请求触发，
                 * 不能用来判断"主板自己恢复推流"，否则握手会自激 */
                if (s_cmd != DISP_CMD_PARAM_DUMP)
                {
                    s_per_last_down_ms = HAL_GetTick();
                }
#endif
                Disp_RecvHandleFrame(s_cmd, s_payload, s_len);
            }
            /* 无论校验对错，回到同步头重新等待 */
            s_state = S_SYNC1;
            break;
        default:
            s_state = S_SYNC1;
            break;
    }
}

/**
 * @brief  初始化上行接收：注册回调、启动接收并请求全量配置
 * @retval 无
 */
void Disp_Init(void)
{
    /* ⚠ 必须在 BSP_USART_Init() 之后调用 */
    BSP_USART_RegisterRxCallback(BSP_USART_INSTANCE_1, Disp_RecvByte);
    BSP_USART_StartRx(BSP_USART_INSTANCE_1);   
    
    Disp_SendRequestParamDump();
}

/* ---------------- 发送：上行命令（显示板 -> 主板） ---------------- */

/**
 * @brief  组装并发送一帧（AA 55 CMD LEN PAYLOAD CRC）
 * @param  cmd     上行命令字（DISP_CMD_*）
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
 * @brief  上行请求主板回送一帧 ECHO 回波数据（0x81，无负载）
 * @retval 无
 */
void Disp_SendRequestEcho(void) { Disp_SendFrame(DISP_CMD_REQ_ECHO, 0, 0); }

/**
 * @brief  上行请求主板回送一帧 MEAS 测量数据（0x82，无负载）
 * @retval 无
 */
void Disp_SendRequestMeas(void) { Disp_SendFrame(DISP_CMD_REQ_MEAS, 0, 0); }

/**
 * @brief  上行转发按键码给主板（0x83）
 * @param  key 按键编码（1 字节）
 * @retval 无
 */
void Disp_SendKey(uint8_t key) { Disp_SendFrame(DISP_CMD_KEY, &key, 1); }

/**
 * @brief  上行设置某个参数（0x84）
 * @param  id    参数编号
 * @param  value 参数值（小端 4 字节负载）
 * @retval 无
 */
void Disp_SendSetParam(uint8_t id, float value)
{
    /* ⚠ 先登记 ACK 再发送：发送是阻塞的，期间接收中断可能已收到 ACK；
     *   先发后登记会把这次 ACK 当成"没有对应的待确认项"丢弃，界面一直停在等待 */
    uint8_t payload[5];
    payload[0] = id;
    memcpy(&payload[1], &value, 4);
    Disp_SendAckTrack(DISP_CMD_SET_PARAM, payload, 5);
    Disp_SendFrame(DISP_CMD_SET_PARAM, payload, 5);
}

/**
 * @brief  上行设置字符串参数（0x85），超出 DISP_UP_STR_MAX(16) 截断
 * @param  id  参数编号
 * @param  str 字符串（可传 NULL）
 * @retval 无
 */
void Disp_SendStr(uint8_t id, const char *str)
{
    uint8_t payload[2u + DISP_UP_STR_MAX];
    uint8_t slen = 0;
    if (str != 0)
    {
        /* ⚠ 先判界再取值：避免 str 恰在 16 字节处结束时读到越界字节 */
        while (slen < DISP_UP_STR_MAX && str[slen] != '\0') slen++;
    }
    payload[0] = id;
    payload[1] = slen;
    if (slen > 0u) memcpy(&payload[2], str, slen);
    Disp_SendAckTrack(DISP_CMD_SET_STR, payload, (uint8_t)(2u + slen));   /* 先登记再发，理由同 Disp_SendSetParam */
    Disp_SendFrame(DISP_CMD_SET_STR, payload, (uint8_t)(2u + slen));
}

/**
 * @brief  上行请求传感器信息（0x86，无负载）
 * @retval 无
 */
void Disp_SendRequestInfo(void) { Disp_SendFrame(DISP_CMD_REQ_INFO, 0, 0); }

/**
 * @brief  上行请求全量配置（0x87，无负载）
 * @retval 无
 */
void Disp_SendRequestParamDump(void) { Disp_SendFrame(DISP_CMD_REQ_PARAM_DUMP, 0, 0); }
