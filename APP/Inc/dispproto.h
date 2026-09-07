#ifndef __DISP_PROTO_H
#define __DISP_PROTO_H

#include <stdint.h>

/* ============================================================
 * 双板自定义通信协议（雷达主板 STM32L496  <->  显示板 STM32F1）
 * 物理链路：USART1，115200 / 8N1
 *
 * 帧格式（小端字节序，STM32 原生）：
 *   [SYNC1][SYNC2][CMD][LEN][  PAYLOAD (LEN 字节)  ][CRC]
 *    0xAA   0x55  1B   1B          LEN 字节         1B
 *   CRC = CMD ^ LEN ^ (PAYLOAD 逐字节异或)
 *
 * 命令字：
 *   下行（主板 -> 显示板）
 *     0x01 MEAS  测量结果：distance(f32) position(f32) delta_amp(f32) peak_count(u8) mode(u8) = 14B
 *     0x02 ECHO  回波包络：128×u8（已归一化 0~255）                                        = 128B
 *     0x03 DIAG  诊断：reliability(u8) status(u8) peakMinEmpty(f32) peakMaxEmpty(f32)       = 10B
 *   上行（显示板 -> 主板）
 *     0x81 REQ_ECHO   请求回波帧（无 payload）
 *     0x82 REQ_MEAS   请求测量帧（无 payload）
 *     0x83 KEY        按键转发：key(u8)                                                      = 1B
 *     0x84 SET_PARAM  设置参数（预留）：param_id(u8) value(f32)                              = 5B
 *
 * 说明：字段映射 / 回波降采样算法等“业务细节”为预留项，由用户在对应
 *       APP 文件中按需填充；本文件只规定帧结构与校验。
 * ============================================================ */

#define DISP_SYNC1         0xAA
#define DISP_SYNC2         0x55

/* 下行命令 */
#define DISP_CMD_MEAS      0x01
#define DISP_CMD_ECHO      0x02
#define DISP_CMD_DIAG      0x03

/* 上行命令 */
#define DISP_CMD_REQ_ECHO  0x81
#define DISP_CMD_REQ_MEAS  0x82
#define DISP_CMD_KEY       0x83
#define DISP_CMD_SET_PARAM 0x84

/* 固定长度 */
#define DISP_ECHO_LEN      128u
#define DISP_MEAS_LEN      14u
#define DISP_DIAG_LEN      10u

/* 协议帧最大长度（含头尾），用于本地缓冲 */
#define DISP_FRAME_MAX     (2u + 1u + 1u + 255u + 1u)   /* 260 */

/* CRC 校验值计算：对 CMD、LEN 及 PAYLOAD 逐字节异或。
 * 发送端把返回值填入帧尾，接收端用同样算法比对以判定帧是否正确。
 * 参数：
 *   cmd     - 命令字（参与异或）
 *   payload - 负载数据指针（可为 NULL，此时只异或 CMD^LEN）
 *   len     - 负载字节数
 * 返回：1 字节校验值 */
static inline uint8_t Disp_CRC(uint8_t cmd, const uint8_t *payload, uint8_t len)
{
    uint8_t crc = cmd;
    if (payload != 0 && len > 0u)
    {
        crc ^= len;
        for (uint8_t i = 0u; i < len; i++)
        {
            crc ^= payload[i];
        }
    }
    else
    {
        crc ^= len;   /* len == 0 时仅 CMD ^ LEN(=0) */
    }
    return crc;
}

#endif /* __DISP_PROTO_H */
