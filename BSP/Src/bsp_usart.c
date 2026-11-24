#include "bsp_usart.h"
#include "usart.h"

static UART_HandleTypeDef* const g_Huart[BSP_USART_INSTANCE_MAX] = {
    &huart1,
};

static uint8_t g_RxByte[BSP_USART_INSTANCE_MAX] = {0};
static BSP_USART_RxCallback_t g_RxCb[BSP_USART_INSTANCE_MAX] = {0};

/**
 * @brief  初始化 BSP 串口层
 * @retval HAL_OK
 */
HAL_StatusTypeDef BSP_USART_Init(void)
{
    for (uint8_t i = 0; i < BSP_USART_INSTANCE_MAX; i++)
    {
        g_RxCb[i]   = 0;
        g_RxByte[i] = 0;
    }
    return HAL_OK;
}

/**
 * @brief  阻塞发送
 * @param  instance 串口实例
 * @param  pData    待发送数据
 * @param  Size     字节数
 * @param  Timeout  超时
 * @retval HAL 状态码
 */
HAL_StatusTypeDef BSP_USART_Send(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_Transmit(g_Huart[instance], (uint8_t *)pData, Size, Timeout);
}

/**
 * @brief  阻塞接收
 * @param  instance 串口实例
 * @param  pData    接收缓冲
 * @param  Size     期望字节数
 * @param  Timeout  超时
 * @retval HAL 状态码
 */
HAL_StatusTypeDef BSP_USART_Receive(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size, uint32_t Timeout)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_Receive(g_Huart[instance], pData, Size, Timeout);
}

/**
 * @brief  中断方式发送
 * @param  instance 串口实例
 * @param  pData    待发送数据
 * @param  Size     字节数
 * @retval HAL 状态码
 */
HAL_StatusTypeDef BSP_USART_Transmit_IT(BSP_USART_Instance_t instance, const uint8_t *pData, uint16_t Size)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_Transmit_IT(g_Huart[instance], (uint8_t *)pData, Size);
}

/**
 * @brief  中断方式接收指定字节数
 * @param  instance 串口实例
 * @param  pData    接收缓冲
 * @param  Size     期望字节数
 * @retval HAL 状态码
 */
HAL_StatusTypeDef BSP_USART_Receive_IT(BSP_USART_Instance_t instance, uint8_t *pData, uint16_t Size)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_Receive_IT(g_Huart[instance], pData, Size);
}

/**
 * @brief  注册逐字节接收回调
 * @param  instance 串口实例
 * @param  callback 回调函数
 * @retval 无
 */
void BSP_USART_RegisterRxCallback(BSP_USART_Instance_t instance, BSP_USART_RxCallback_t callback)
{
    if (instance < BSP_USART_INSTANCE_MAX)
    {
        g_RxCb[instance] = callback;
    }
}

/**
 * @brief  中止中断方式接收
 * @param  instance 串口实例
 * @retval HAL 状态码
 */
HAL_StatusTypeDef BSP_USART_AbortReceive_IT(BSP_USART_Instance_t instance)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;
    return HAL_UART_AbortReceive_IT(g_Huart[instance]);
}

/**
 * @brief  启动逐字节中断接收
 * @param  instance 串口实例
 * @retval HAL 状态码
 */
HAL_StatusTypeDef BSP_USART_StartRx(BSP_USART_Instance_t instance)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return HAL_ERROR;

    return HAL_UART_Receive_IT(g_Huart[instance], &g_RxByte[instance], 1);
}

/**
 * @brief  取串口句柄
 * @param  instance 串口实例
 * @retval 句柄指针；实例非法返回 0
 */
UART_HandleTypeDef* BSP_USART_GetHandle(BSP_USART_Instance_t instance)
{
    if (instance >= BSP_USART_INSTANCE_MAX) return 0;
    return g_Huart[instance];
}

/**
 * @brief  接收完成回调
 * @param  huart 触发中断的串口句柄
 * @retval 无
 */
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
            HAL_UART_Receive_IT(g_Huart[i], &g_RxByte[i], 1);
            break;
        }
    }
}

/**
 * @brief  接收错误回调
 * @param  huart 触发中断的串口句柄
 * @retval 无
 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < BSP_USART_INSTANCE_MAX; i++)
    {
        if (g_Huart[i] == huart)
        {
            __HAL_UART_CLEAR_OREFLAG(huart);   /* 清 ORE 挂起位：先读 SR 再读 DR */

            (void)HAL_UART_Receive_IT(g_Huart[i], &g_RxByte[i], 1);
            break;
        }
    }
}
