#ifndef __DISP_PROTO_H
#define __DISP_PROTO_H

#include <stdint.h>


#define DISP_SYNC1         0xAA
#define DISP_SYNC2         0x55

#define DISP_CMD_MEAS      0x01
#define DISP_CMD_ECHO      0x02
#define DISP_CMD_DIAG      0x03
#define DISP_CMD_INFO      0x04
#define DISP_CMD_PARAM_DUMP 0x05
#define DISP_CMD_ECHO_TYPED  0x06
#define DISP_CMD_ACK       0x07


#define DISP_ACK_LEN       3u

#define DISP_ACK_OK        0u
#define DISP_ACK_EEPROM_ERR 1u
#define DISP_ACK_BAD_ID    2u
#define DISP_ACK_BAD_FRAME 3u


#define DISP_CURVE_ECHO      0x01
#define DISP_CURVE_FALSE     0x02

#define DISP_CMD_REQ_ECHO  0x81
#define DISP_CMD_REQ_MEAS  0x82
#define DISP_CMD_KEY       0x83
#define DISP_CMD_SET_PARAM 0x84
#define DISP_CMD_SET_STR   0x85
#define DISP_CMD_REQ_INFO  0x86
#define DISP_CMD_REQ_PARAM_DUMP 0x87

#define DISP_ECHO_LEN      128u
#define DISP_MEAS_LEN      14u
#define DISP_DIAG_LEN      14u
#define DISP_INFO_LEN      3u
#define DISP_ECHO_TYPED_LEN 129u


#define DISP_FRAME_MAX     (2u + 1u + 1u + 255u + 1u)

#define DISP_PAYLOAD_MAX   255u

#define DISP_UP_STR_MAX   16u

/**
 * @brief  CRC 校验
 * @param  cmd     命令字
 * @param  payload 负载
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

/* 参数编号：两板必须完全一致，且与显示板 oled_ui.h 的 RADAR_PARAM 字段一一对应。
 * 新增 ID 只能追加在 DPARAM_COUNT 之前，不得插入或重排 */
typedef enum
{
    DPARAM_LOW_ADJ_PCT = 0,   /* 低位调整 */
    DPARAM_LOW_ADJ_VAL,       /* 低位调整 */
    DPARAM_HIGH_ADJ_PCT,      /* 高位调整 */
    DPARAM_HIGH_ADJ_VAL,      /* 高位调整 */
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
    DPARAM_SENSOR_TAG,        /* 传感器标签 */
    DPARAM_SERV_FALSE_ECHO,   /* 虚假回波学习 */
    DPARAM_COUNT
} DISP_PARAM_ID;

#endif
