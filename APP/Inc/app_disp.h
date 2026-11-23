#ifndef __APP_DISP_H
#define __APP_DISP_H

#include <stdint.h>
#include "bsp_usart.h"
#include "dispproto.h"

/* 显示板侧协议接口：接收 = 逐字节回调解析 AA 55 CMD LEN PAYLOAD CRC 帧并写入
 * oled_ui 的 gRadarParam / radar_echo；发送 = UI 按键与参数编辑调用 Disp_Send*()。 */

/**
 * @brief  初始化上行接收：注册 USART1 回调、启动接收并请求全量配置
 * @retval 无
 */
void Disp_Init(void);

/* 上行命令（显示板 -> 主板） */
void Disp_SendRequestEcho(void);                 /* 0x81 请求回波帧（无参数） */
void Disp_SendRequestMeas(void);                 /* 0x82 请求测量帧（无参数） */
void Disp_SendKey(uint8_t key);                  /* 0x83 转发按键：key=按键编码(1B) */
void Disp_SendSetParam(uint8_t id, float value); /* 0x84 设置参数：id=参数编号(1B), value=参数值(f32) */
void Disp_SendStr(uint8_t id, const char *str);  /* 0x85 设置字符串参数：id=参数编号, str=字符串 */
void Disp_SendRequestInfo(void);                 /* 0x86 请求传感器信息（无参数） */
void Disp_SendRequestParamDump(void);            /* 0x87 请求全量配置（无参数） */

/* 写参数的结果确认（0x07 ACK）状态
 * 推进方式：发送 → WAIT；收到 0x07 且 result=0 → OK；result≠0 或超时 → FAIL。
 * ⚠ 状态由 Disp_SendAckPoll() 推进，主循环必须每轮调用它。 */
#define DISP_ACK_STATE_NONE  0u   /* 当前没有待确认的写操作 */
#define DISP_ACK_STATE_WAIT  1u   /* 已发出，等待主板 ACK */
#define DISP_ACK_STATE_OK    2u   /* 主板确认：已写入并成功落盘 EEPROM */
#define DISP_ACK_STATE_FAIL  3u   /* 主板回失败，或重试后仍无应答 */

/**
 * @brief  主循环轮询：ACK 超时判定与重发，并驱动配置同步握手
 * @retval 无
 */
void Disp_SendAckPoll(void);

/* 配置同步握手 */
#define DISP_PERIODIC_REQ_ENABLE     1
#define DISP_PERIODIC_HANDSHAKE_MS   500u   /* 未同步时 0x87 重发间隔 */
#define DISP_PERIODIC_FAST_TRIES     20u    /* 快重发次数（20 x 500ms = 10s） */
#define DISP_PERIODIC_SLOW_MS        5000u  /* 快重发用尽后的退避间隔 */
#define DISP_PERIODIC_RESYNC_ENABLE  1      /* 1 = 下行恢复沿重新拉配置 */
#define DISP_PERIODIC_SILENCE_MS     2000u  /* 判定主板静默的门限（正常推流约 535ms/帧） */

/**
 * @brief  握手调度：推进重发与同步状态（由 Disp_SendAckPoll() 末尾调用）
 * @retval 无
 */
void Disp_SendPeriodicTask(void);

/**
 * @brief  查询当前写操作的确认状态
 * @retval DISP_ACK_STATE_*
 */
uint8_t Disp_SendParamAckState(void);

/**
 * @brief  查询最近一次 ACK 的结果码
 * @retval DISP_ACK_*；FAIL 时可能是 EEPROM 写失败 / BAD_ID / 0xFF(主板无应答)
 */
uint8_t Disp_SendParamAckResult(void);

/**
 * @brief  取走并清零"有新的确认结果"事件（读取即清，避免每帧重复提示）
 * @retval 1=自上次调用以来状态变过 OK/FAIL，0=无
 */
uint8_t Disp_SendTakeParamAckEvent(void);

#endif
