/* BSP layer: USART 板级封装 (STM32F1 显示板)
 * 仅封装 CubeMX 生成的 huart1（USART1），对接 STM32L496 雷达主板的自定义协议。
 * 接口与 RD_L496 的 BSP/Src/bsp_usart.c 对齐。
 *
 * 注意：本文件只提供“传输能力”封装；具体的帧解析状态机（AA 55 ... CRC）
 * 应放在 APP 层（如 APP/Src/app_disp.c），通过 BSP_USART_RegisterRxCallback
 * 注册逐字节回调来实现，不要在此处写协议。
 *
 * ★接收链路的两条硬约束（2026-09-22 修 UART 溢出根因时确立）：
 *   1) HAL_UART_RxCpltCallback 里【绝不能做耗时操作】。HAL 在调用本回调前
 *      就已关闭 RXNEIE/PEIE/EIE，而重新武装写在回调尾部 ⇒ 回调耗时 = 接收
 *      失聪窗口；窗口超过 2 字节时间（115200 8N1 = 173.6µs）必然触发 ORE。
 *      因此重活（如历史归档、min/max 扫描）一律搬出中断，交给主循环。
 *   2) 【必须】实现 HAL_UART_ErrorCallback 重新武装接收。否则一旦 ORE，
 *      HAL 会关掉接收中断且无人恢复 ⇒ 接收永久停摆（见该函数注释）。
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

/* 接收完成回调：把字节交给已注册的协议解析回调，并【最后】重新武装 IT 接收。
 * ★性能红线：本回调全程在接收中断里执行，其耗时就是"接收失聪窗口"
 *   （HAL 在调用本回调前已关闭 RXNEIE/PEIE/EIE）。115200 8N1 下该窗口必须
 *   远小于 2 字节时间 = 173.6µs，否则触发 ORE。协议解析回调（Disp_OnRxByte）
 *   只做逐字节组帧，不得在其中做 O(n) 浮点运算等重活。 */
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

/* 接收错误回调（覆盖 HAL 的 __weak 空实现，必须实现）。
 *
 * ★为什么必须实现 —— "显示板数据跑一会儿就不再更新"的第二个根因：
 *   HAL_UART_IRQHandler() 检测到 ORE 等错误后会走"阻塞错误"分支，调用
 *   UART_EndRxTransfer()：关闭 RXNEIE/PEIE/EIE 并把 RxState 置 READY。
 *   此后 USART 再也不产生接收中断；若用户不实现本回调重新武装，接收将
 *   【永久静默】—— 现象是读数停更、但主循环/按键/菜单/心跳灯一切正常，
 *   只能靠复位恢复（极易被误判为"死机"）。
 *   即便导致 ORE 的那个根因（中断内 O(n) 扫描，见 oled_ui.c 的
 *   s_histRangeDirty 注释）已修好，也必须保留本回调作为兜底：现场一个
 *   干扰字节或上位机一次突发都可能再次触发 ORE，没有它就会锁死。
 *
 * 处理：按 F1 参考手册的清错序列"先读 SR 再读 DR"清掉 ORE 挂起位
 *   （__HAL_UART_CLEAR_OREFLAG 展开正是这个序列），然后重新武装逐字节接收。
 *   走到这里时 UART_EndRxTransfer() 已把 RxState 置为 READY，可直接重启。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    for (uint8_t i = 0; i < BSP_USART_INSTANCE_MAX; i++)
    {
        if (g_Huart[i] == huart)
        {
            __HAL_UART_CLEAR_OREFLAG(huart);   /* 清 ORE：读 SR 再读 DR */

            /* RxState 此刻为 READY，重新武装接收即可恢复。
             * 返回非 OK（理论上不该发生）时本轮不重试，避免在中断里死等；
             * 下一次错误回调会再次尝试恢复。 */
            (void)HAL_UART_Receive_IT(g_Huart[i], &g_RxByte[i], 1);
            break;
        }
    }
}
