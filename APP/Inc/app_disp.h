#ifndef __APP_DISP_H
#define __APP_DISP_H

#include <stdint.h>
#include "bsp_usart.h"
#include "dispproto.h"

/**
 * @brief  初始化上行接收：注册 USART1 回调、启动接收并请求全量配置
 * @retval 无
 */
void Disp_Init(void);

/* 上行命令 */
void Disp_SendRequestEcho(void);                 /* 0x81 请求回波帧 */
void Disp_SendRequestMeas(void);                 /* 0x82 请求测量帧 */
void Disp_SendKey(uint8_t key);                  /* 0x83 转发按键 */
void Disp_SendSetParam(uint8_t id, float value); /* 0x84 设置参数 */
void Disp_SendStr(uint8_t id, const char *str);  /* 0x85 设置字符串参数 */
void Disp_SendRequestInfo(void);                 /* 0x86 请求传感器信息 */
void Disp_SendRequestParamDump(void);

#define DISP_ACK_STATE_NONE  0u   /* 未发送 */
#define DISP_ACK_STATE_WAIT  1u   /* 等待 ACK */
#define DISP_ACK_STATE_OK    2u   /* 主板回 OK */
#define DISP_ACK_STATE_FAIL  3u   /* 主板回失败，或重试后仍无应答 */

/**
 * @brief  主循环轮询：ACK 超时判定与重发，并驱动配置同步握手
 * @retval 无
 */
void Disp_SendAckPoll(void);

#define DISP_PERIODIC_REQ_ENABLE     1
#define DISP_PERIODIC_HANDSHAKE_MS   500u
#define DISP_PERIODIC_FAST_TRIES     20u
#define DISP_PERIODIC_SLOW_MS        5000u
#define DISP_PERIODIC_RESYNC_ENABLE  1
#define DISP_PERIODIC_SILENCE_MS     2000u  /* 判定主板静默的门限 */

/**
 * @brief  握手调度
 * @retval 无
 */
void Disp_SendPeriodicTask(void);

/**
 * @brief  查询当前写操作的确认状态
 * @retval 
 */
uint8_t Disp_SendParamAckState(void);

/**
 * @brief  查询最近一次 ACK 的结果码
 * @retval 
 */
uint8_t Disp_SendParamAckResult(void);

/**
 * @brief  取走并清零"有新的确认结果"事件
 * @retval 1=自上次调用以来状态变过 OK/FAIL，0=无
 */
uint8_t Disp_SendTakeParamAckEvent(void);

#endif
