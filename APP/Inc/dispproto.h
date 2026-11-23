#ifndef __DISP_PROTO_H
#define __DISP_PROTO_H

#include <stdint.h>

/* 双板自定义协议（主板 STM32L496 <-> 显示板 STM32F1）：USART1，115200 / 8N1，小端
 * 帧格式 [SYNC1][SYNC2][CMD][LEN][PAYLOAD][CRC] = 0xAA 0x55 CMD LEN … CRC
 * CRC = CMD ^ LEN ^ (PAYLOAD 逐字节异或)
 * 下行 0x01 MEAS(14B) / 0x02 ECHO(128B) / 0x03 DIAG(14B) / 0x04 INFO(3B)
 *      0x05 PARAM_DUMP(81B) / 0x06 ECHO_TYPED(129B) / 0x07 ACK(3B)
 * 上行 0x81 REQ_ECHO / 0x82 REQ_MEAS / 0x83 KEY(1B) / 0x84 SET_PARAM(5B)
 *      0x85 SET_STR(2B+len) / 0x86 REQ_INFO / 0x87 REQ_PARAM_DUMP
 * 字段映射见各板 app_disp.c；本文件只规定帧结构与校验。 */

#define DISP_SYNC1         0xAA
#define DISP_SYNC2         0x55

/* 下行命令 */
#define DISP_CMD_MEAS      0x01
#define DISP_CMD_ECHO      0x02
#define DISP_CMD_DIAG      0x03
#define DISP_CMD_INFO      0x04   /* 传感器信息（主板 -> 显示板） */
#define DISP_CMD_PARAM_DUMP 0x05  /* 全量配置（主板 -> 显示板） */
#define DISP_CMD_ECHO_TYPED  0x06   /* 带类型标识的曲线帧（主板 -> 显示板）*/
#define DISP_CMD_ACK       0x07   /* 上行命令写结果确认（主板 -> 显示板），见 DISP_ACK_LEN */

/* ACK 负载（0x07）：acked_cmd(u8) result(u8) param_id(u8) = 3B
 * 收到 result=0 才代表主板已落盘 EEPROM；无 ACK 或 result≠0 均须提示用户。
 * ⚠ 只能新开 0x07，不可给 0x84 加应答位或改其负载布局：协议无序列号，
 *   新旧固件混用会把 value(f32) 错位解析。
 * ⚠ 显示板侧重发约束：仅"可安全重复执行"的参数可超时重发；
 *   SERV_FALSE_ECHO（重写 10KB 基线）与 SERV_RESET 重发后果不可逆，只能报无应答。 */

#define DISP_ACK_LEN       3u    /* acked_cmd(u8) + result(u8) + param_id(u8) */

/* ACK result 码 */
#define DISP_ACK_OK        0u    /* 参数已写入 RAM 并成功落盘 EEPROM */
#define DISP_ACK_EEPROM_ERR 1u   /* 参数已进 RAM，但 EEPROM 写失败（断电会丢） */
#define DISP_ACK_BAD_ID    2u    /* param_id 未知 / 该参数无写入分支，未做任何动作 */
#define DISP_ACK_BAD_FRAME 3u    /* 负载长度或格式不合法，未做任何动作 */

/* 曲线类型（ECHO_TYPED 的 payload[0]）
 * ⚠ 必须新开 0x06，不可给 0x02 ECHO 加类型字节：旧接收端会把类型字节当第一个数据点，
 *   整条曲线右移一格。新命令字对旧固件天然向后兼容（走 default 丢弃）。 */

#define DISP_CURVE_ECHO      0x01   /* 回波曲线：adc_buf 原始包络 */
#define DISP_CURVE_FALSE     0x02   /* 虚假回波曲线：空罐学习的基线 */
/* 0x03 预留给"输出走势曲线"，落地时在此登记 */

/* 上行命令 */
#define DISP_CMD_REQ_ECHO  0x81
#define DISP_CMD_REQ_MEAS  0x82
#define DISP_CMD_KEY       0x83
#define DISP_CMD_SET_PARAM 0x84   /* 设置参数：param_id(u8) + value(f32) = 5B */
#define DISP_CMD_SET_STR   0x85   /* 设置字符串参数：param_id(u8) + len(u8) + data = 2B+len */
#define DISP_CMD_REQ_INFO  0x86   /* 请求传感器信息（无 payload） */
#define DISP_CMD_REQ_PARAM_DUMP 0x87  /* 请求全量配置（无 payload） */

/* 固定长度 */
#define DISP_ECHO_LEN      128u
#define DISP_MEAS_LEN      14u
#define DISP_DIAG_LEN      14u   /* reliability(u8) status(u8) peakMinEmpty(f32) peakMaxEmpty(f32) temperature(f32) */
#define DISP_INFO_LEN      3u    /* sensorType(u8) verMajor(u8) verMinor(u8) */
#define DISP_ECHO_TYPED_LEN 129u /* curveType(u8) + 128×u8 */

/* PARAM_DUMP(0x05) 负载 = gRadarConfig 字段按此固定顺序 memcpy 序列化，共 81 字节
 * （避开结构体 padding；⚠ 双板必须严格同序，改布局须两板同步）
 *   [0..3]   lowAdjustPct(f32)    [4..7]  lowAdjustVal(f32)   [8..11]  highAdjustPct(f32)
 *   [12..15] highAdjustVal(f32)   [16]    matType(u8)         [17]     matFastChange(u8)
 *   [18]     matFirstWave(u8)     [19]    matSurfAngle(u8)    [20]     matFoamDust(u8)
 *   [21]     matSmallDK(u8)       [22]    matPipe(u8)         [23..26] pipeDiameter(f32)
 *   [27..30] dampTime(f32)        [31]    outMap(u8)          [32]     scaleUnit(u8)
 *   [33..36] scaleVal(f32)        [37..40] rangeSetting(f32)  [41..44] blindZone(f32)
 *   [45]     currMode(u8)         [46]    currFault(u8)       [47]     currMin(u8)
 *   [48]     servReset(u8)        [49]    servUnit(u8)        [50]     servHART(u8)
 *   [51]     servHARTAddr(u8)     [52..55] servOffset(f32)    [56..59] threshEcho(f32)
 *   [60..63] threshEnv(f32)       [64]    diagSim(u8)         [65..80] sensorTag(char[16])
 */

/* 协议帧最大长度（含头尾），用于本地缓冲 */
#define DISP_FRAME_MAX     (2u + 1u + 1u + 255u + 1u)   /* 260 */

/* 单帧 payload 上限（LEN 字段为 u8），接收侧缓冲按此尺寸开辟。
 * ⚠ 协议允许值，非当前用量（当前最长 0x06 ECHO_TYPED = 129） */
#define DISP_PAYLOAD_MAX   255u

/* 上行字符串参数（0x85）长度上限：对应 PARAM_DUMP 的 sensorTag(char[16])，不含 '\0'
 * 显示板按此截断（payload 只需 2+16=18B），主板用 char buf[17] 承载并补 '\0'
 * ⚠ sensorTag 为 C 字符串，实际可存字符数 = 本值 - 1 */
#define DISP_UP_STR_MAX   16u

/**
 * @brief  CRC 校验（CMD ^ LEN ^ PAYLOAD）
 * @param  cmd     命令字
 * @param  payload 负载（可为 NULL，此时只算 CMD ^ LEN）
 * @param  len     负载字节数
 * @retval 校验字节
 */
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
        crc ^= len;
    }
    return crc;
}

/* 参数编号（SET_PARAM / SET_STR 的 param_id）：两板必须完全一致，且与
 * 显示板 oled_ui.h 的 RADAR_PARAM 字段一一对应。
 * ⚠ 新增 ID 只能追加在 DPARAM_COUNT 之前，不得插入或重排 */
typedef enum
{
    DPARAM_LOW_ADJ_PCT = 0,   /* 低位调整(%) */
    DPARAM_LOW_ADJ_VAL,       /* 低位调整(距离) */
    DPARAM_HIGH_ADJ_PCT,      /* 高位调整(%) */
    DPARAM_HIGH_ADJ_VAL,      /* 高位调整(距离) */
    DPARAM_MAT_TYPE,          /* 物料类型 */
    DPARAM_MAT_FAST_CHANGE,   /* 物料快速变化 */
    DPARAM_MAT_FIRST_WAVE,    /* 首波选择 */
    DPARAM_MAT_SURF_ANGLE,    /* 表面波动/堆角 */
    DPARAM_MAT_FOAM_DUST,     /* 泡沫/粉尘 */
    DPARAM_MAT_SMALL_DK,      /* DK值小 */
    DPARAM_MAT_PIPE,          /* 导波管测量 */
    DPARAM_PIPE_DIAMETER,     /* 导波管直径 */
    DPARAM_DAMP_TIME,         /* 阻尼时间 */
    DPARAM_OUT_MAP,           /* 输出映射 */
    DPARAM_SCALE_UNIT,        /* 定标单位 */
    DPARAM_SCALE_VAL,         /* 定标值 */
    DPARAM_RANGE_SETTING,     /* 量程设定 */
    DPARAM_BLIND_ZONE,        /* 盲区范围 */
    DPARAM_CURR_MODE,         /* 电流输出模式 */
    DPARAM_CURR_FAULT,        /* 故障电流 */
    DPARAM_CURR_MIN,          /* 最小电流 */
    DPARAM_SERV_RESET,        /* 复位 */
    DPARAM_SERV_UNIT,         /* 测量单位 */
    DPARAM_SERV_HART,         /* HART工作模式 */
    DPARAM_SERV_HART_ADDR,    /* HART地址 */
    DPARAM_SERV_OFFSET,       /* 距离偏量 */
    DPARAM_THRESH_ECHO,       /* 回波阈值 */
    DPARAM_THRESH_ENV,        /* 包络阈值 */
    DPARAM_DIAG_SIM,          /* 仿真 */
    DPARAM_SENSOR_TAG,        /* 传感器标签(字符串，用 SET_STR) */
    DPARAM_SERV_FALSE_ECHO,   /* 虚假回波学习（触发空罐基线重学习） */
    DPARAM_COUNT
} DISP_PARAM_ID;

#endif
