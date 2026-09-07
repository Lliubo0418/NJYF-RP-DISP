#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* 串口实例：显示板仅 USART1 与 STM32L496 雷达主板通信（自定义协议）
 * 接口与 RD_L496 的 BSP/Inc/bsp_usart.h 保持一致，方便双板协议对接。
 */
typedef enum {
    BSP_USART_INSTANCE_1 = 0,   /* 与 STM32L496 雷达主板通信（自定义协议） */
    BSP_USART_INSTANCE_MAX
} BSP_USART_Instance_t;

/* 每字节接收完成回调（用于协议状态机逐字节解析） */
typedef void (*BSP_USART_RxCallback_t)(BSP_USART_Instance_t instance, uint8_t data);

/* 初始化 BSP 串口层（回调表等；底层 huart1 由 CubeMX MX_USART1_UART_Init 完成） */
HAL_StatusTypeDef BSP_USART_Init(void);

/* 阻塞发送 / 接收 */
HAL_StatusTypeDef BSP_USART_Send(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef BSP_USART_Receive(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size, uint32_t Timeout);

/* 中断方式发送 / 接收 */
HAL_StatusTypeDef BSP_USART_Transmit_IT(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef BSP_USART_Receive_IT(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size);

/* 注册逐字节接收回调 */
void BSP_USART_RegisterRxCallback(BSP_USART_Instance_t instance, BSP_USART_RxCallback_t callback);

/* 取消中断接收 */
HAL_StatusTypeDef BSP_USART_AbortReceive_IT(BSP_USART_Instance_t instance);

/* 启动逐字节中断接收（内部使用固定缓冲 g_RxByte，HAL 回调里自动重新武装）。
 * 协议层 Disp_Init() 会调用它来开启 USART1 持续接收。 */
HAL_StatusTypeDef BSP_USART_StartRx(BSP_USART_Instance_t instance);

/* 获取串口句柄 */
UART_HandleTypeDef* BSP_USART_GetHandle(BSP_USART_Instance_t instance);

#endif /* __BSP_USART_H */
