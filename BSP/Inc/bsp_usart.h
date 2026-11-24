#ifndef __BSP_USART_H
#define __BSP_USART_H

#include "stm32f1xx_hal.h"
#include <stdint.h>

typedef enum {
    BSP_USART_INSTANCE_1 = 0,
    BSP_USART_INSTANCE_MAX
} BSP_USART_Instance_t;

typedef void (*BSP_USART_RxCallback_t)(BSP_USART_Instance_t instance, uint8_t data);


HAL_StatusTypeDef BSP_USART_Init(void);

/* 阻塞发送 / 接收 */
HAL_StatusTypeDef BSP_USART_Send(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size, uint32_t Timeout);
HAL_StatusTypeDef BSP_USART_Receive(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size, uint32_t Timeout);

/* 中断方式发送 / 接收 */
HAL_StatusTypeDef BSP_USART_Transmit_IT(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size);
HAL_StatusTypeDef BSP_USART_Receive_IT(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size);

/* 注册逐字节接收回调 */
void BSP_USART_RegisterRxCallback(BSP_USART_Instance_t instance, BSP_USART_RxCallback_t callback);

HAL_StatusTypeDef BSP_USART_AbortReceive_IT(BSP_USART_Instance_t instance);

/* 启动逐字节中断接收 */
HAL_StatusTypeDef BSP_USART_StartRx(BSP_USART_Instance_t instance);

UART_HandleTypeDef* BSP_USART_GetHandle(BSP_USART_Instance_t instance);

#endif
