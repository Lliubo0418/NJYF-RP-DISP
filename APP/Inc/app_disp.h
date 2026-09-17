#ifndef __APP_DISP_H
#define __APP_DISP_H

#include <stdint.h>
#include "bsp_usart.h"
#include "dispproto.h"

/* 显示板侧协议接口。
 * 接收：通过 BSP_USART_RegisterRxCallback 注册逐字节回调，解析 AA 55 CMD LEN PAYLOAD CRC，
 *       再把数据写入 oled_ui 的 gRadarParam / radar_echo，并触发 UI 重绘。
 * 发送：上行命令（显示板 -> 主板）为预留接口，按需调用。
 */

/* 协议初始化：注册 USART1 接收回调并启动逐字节接收。无参数。
 * 注意：须在 BSP_USART_Init() 之后调用。 */
void Disp_Init(void);

/* 上行命令（显示板 -> 主板），预留，按需调用 */
void Disp_UpRequestEcho(void);                 /* 0x81 请求回波帧（无参数） */
void Disp_UpRequestMeas(void);                 /* 0x82 请求测量帧（无参数） */
void Disp_UpSendKey(uint8_t key);             /* 0x83 转发按键：key=按键编码(1B) */
void Disp_UpSetParam(uint8_t id, float value);/* 0x84 设置参数：id=参数编号(1B), value=参数值(f32) */
void Disp_UpSendStr(uint8_t id, const char *str); /* 0x85 设置字符串参数：id=参数编号, str=字符串 */
void Disp_UpRequestInfo(void);                /* 0x86 请求传感器信息（无参数） */
void Disp_UpRequestParamDump(void);           /* 0x87 请求全量配置（无参数） */

#endif /* __APP_DISP_H */
