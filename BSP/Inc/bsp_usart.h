#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

/* 串口实例：显示板仅 USART1 与主板通信，接口与 RD_L496 的 bsp_usart.h 对齐 */
typedef enum {
    BSP_USART_INSTANCE_1 = 0,
    BSP_USART_INSTANCE_MAX
} BSP_USART_Instance_t;

/* 每字节接收完成回调（协议状态机逐字节解析用） */
typedef void (*BSP_USART_RxCallback_t)(BSP_USART_Instance_t instance, uint8_t data);

/* 初始化 BSP 串口层（回调表等）；huart1 由 CubeMX MX_USART1_UART_Init 完成 */
HAL_StatusTypeDef BSP_USART_Init(void);

/* 阻塞发送 / 接收（⚠ 不得在中断上下文调用，见规范 §6.1 ISR 禁止清单） */
HAL_StatusTypeDef BSP_USART_Send(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef BSP_USART_Receive(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size, uint32_t Timeout);

/* 中断方式发送 / 接收 */
HAL_StatusTypeDef BSP_USART_Transmit_IT(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef BSP_USART_Receive_IT(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size);

/* 注册逐字节接收回调；回调运行在接收中断里，只能做 O(1) */
void BSP_USART_RegisterRxCallback(BSP_USART_Instance_t instance, BSP_USART_RxCallback_t callback);

HAL_StatusTypeDef BSP_USART_AbortReceive_IT(BSP_USART_Instance_t instance);

/* 启动逐字节中断接收（用内部固定缓冲 g_RxByte，HAL 回调里自动重新武装）；Disp_Init() 调用 */
HAL_StatusTypeDef BSP_USART_StartRx(BSP_USART_Instance_t instance);

UART_HandleTypeDef* BSP_USART_GetHandle(BSP_USART_Instance_t instance);

#endif
