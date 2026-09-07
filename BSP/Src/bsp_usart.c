/* BSP layer: USART 板级封装 (STM32F1 显示板)
 * 仅封装 CubeMX 生成的 huart1（USART1），对接 STM32L496 雷达主板的自定义协议。
 * 接口与 RD_L496 的 BSP/Src/bsp_usart.c 对齐。
 *
 * 注意：本文件只提供“传输能力”封装；具体的帧解析状态机（AA 55 ... CRC）
 * 应放在 APP 层（如 APP/Src/app_disp.c），通过 BSP_USART_RegisterRxCallback
 * 注册逐字节回调来实现，不要在此处写协议。
 */
#include "bsp_usart.h"
#include "usart.h"   /* extern UART_HandleTypeDef huart1; */

/* 实例 -> HAL 句柄 映射 */
static UART_HandleTypeDef* const g_Huart[BSP_USART_INSTANCE_MAX] = {
    &huart1,   /* INSTANCE_1 = USART1 */
};

/* 逐字节接收缓冲：配合 Receive_IT 使用，回调里自动重新武装 */
static uint8_t g_RxByte[BSP_USART_INSTANCE_MAX] = {0};
static BSP_USART_RxCallback_t g_RxCb[BSP_USART_INSTANCE_MAX] = {0};

HAL_StatusTypeDef BSP_USART_Init(void)
{
    for (uint8_t i = 0; i < BSP_USART_INSTANCE_MAX; i++)
    {
        g_RxCb[i]   = 0;
        g_RxByte[i] = 0;
    }
    return HAL_OK;   /* huart1 已由 CubeMX MX_USART1_UART_Init() 初始化 */
}

HAL_StatusTypeDef BSP_USART_Send(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_Transmit(g_Huart[instance], (uint8_t *)pData, Size, Timeout);
}

HAL_StatusTypeDef BSP_USART_Receive(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_Receive(g_Huart[instance], pData, Size, Timeout);
}

HAL_StatusTypeDef BSP_USART_Transmit_IT(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_Transmit_IT(g_Huart[instance], (uint8_t *)pData, Size);
}

HAL_StatusTypeDef BSP_USART_Receive_IT(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_Receive_IT(g_Huart[instance], pData, Size);
}

void BSP_USART_RegisterRxCallback(BSP_USART_Instance_t instance, BSP_USART_RxCallback_t callback)
{
    if (instance < BSP_USART_INSTANCE_MAX)
    {
        g_RxCb[instance] = callback;
    }
}

HAL_StatusTypeDef BSP_USART_AbortReceive_IT(BSP_USART_Instance_t instance)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_AbortReceive_IT(g_Huart[instance]);
}

HAL_StatusTypeDef BSP_USART_StartRx(BSP_USART_Instance_t instance)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    /* 用内部固定缓冲，与 HAL_UART_RxCpltCallback 中的重新武装保持一致 */
    return HAL_UART_Receive_IT(g_Huart[instance], &g_RxByte[instance], 1);
}

UART_HandleTypeDef* BSP_USART_GetHandle(BSP_USART_Instance_t instance)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return 0;
    return g_Huart[instance];
}

/* 接收完成回调：把字节交给已注册的协议解析回调，并重新武装 IT 接收 */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < BSP_USART_INSTANCE_MAX; i++)
    {
        if (g_Huart[i] == huart)
        {
            uint8_t b = g_RxByte[i];
            if (g_RxCb[i])
            {
                g_RxCb[i]((BSP_USART_Instance_t)i, b);
            }
            HAL_UART_Receive_IT(g_Huart[i], &g_RxByte[i], 1);   /* 重新武装 */
            break;
        }
    }
}
