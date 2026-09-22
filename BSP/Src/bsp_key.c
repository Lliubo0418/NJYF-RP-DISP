/* 按键 GPIO 引脚仍在 CubeMX 生成的 MX_GPIO_Init() 中初始化，
 * 本文件负责 EXTI 中断登记与主循环非阻塞消抖。
 *
 * ★EXTI 触发边沿契约（2026-09-21 核对）：
 *   state 的唯一写入路径是"EXTI 触发 → 消抖窗口后回读电平"，因此 state 想从 1
 *   回到 0，就要求【松开】时也产生一次 EXTI。
 *   => gpio.c 的按键必须配成双边沿 GPIO_MODE_IT_RISING_FALLING。
 *   若改成仅下降沿：按下仍能正常触发 Key_GetPressed()（当前业务不受影响），
 *      但松开不产生 EXTI，state 会长期卡在 1，Key_GetState() 失去"实时电平"语义。
 *   核对记录：rp_disp.ioc 原本写成 GPIO_MODE_IT_FALLING，与 gpio.c 的
 *      RISING_FALLING 不一致，已同步为双边沿。日后在 CubeMX 里重新生成代码前，
 *      请确认本契约仍成立（对应 rp_disp.ioc 的 PB8~PB12.GPIO_ModeDefaultEXTI）。
 *
 * 非阻塞消抖策略：
 *   - EXTI 回调只记录触发的引脚编号与时间戳，不做任何延时或采样；
 *   - Key_Task() 在主循环中轮询，距最近一次 EXTI 触发超过 KEY_DEBOUNCE_MS 后，
 *     重新读取 GPIO 电平确认稳定状态，并置位 pressed 事件标志。
 */
#include "bsp_key.h"
#include "gpio.h"   /* KEY3_Pin / KEY3_GPIO_Port 等 CubeMX 生成宏 */
#include "stm32f1xx_hal.h"   /* HAL_GetTick */

typedef struct {
    uint8_t  state;       /* 当前稳定电平（按下=1） */
    uint8_t  pressed;     /* 一次性“按下事件”标志 */
    uint8_t  pending;     /* EXTI 已触发，等待消抖确认 */
    uint32_t trigger_tick;/* EXTI 触发时的 SysTick 时间戳 */
} KeyStatusTypeDef;

static KeyStatusTypeDef key_status[4] = {0};

/* EXTI 触发的引脚 -> key_status 索引映射 */
static int8_t Key_IndexByPin(uint16_t GPIO_Pin)
{
    switch (GPIO_Pin)
    {
        case KEY3_Pin: return 0;
        case KEY4_Pin: return 1;
        case KEY5_Pin: return 2;
        case KEY6_Pin: return 3;
        default:       return -1;
    }
}

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    int8_t idx = Key_IndexByPin(GPIO_Pin);
    if (idx >= 0)
    {
        key_status[idx].pending      = 1U;
        key_status[idx].trigger_tick = HAL_GetTick();
    }
    /* 清除对应 EXTI 挂起位 */
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_Pin);
}

/* 主循环调用：对处于 pending 状态的按键，待消抖窗口过后采样确认。 */
void Key_Task(void)
{
    uint32_t now = HAL_GetTick();
    for (uint8_t i = 0; i < 4; i++)
    {
        if (key_status[i].pending &&
            (now - key_status[i].trigger_tick) >= KEY_DEBOUNCE_MS)
        {
            uint16_t pin = 0;
            GPIO_TypeDef *port = 0;
            switch (i)
            {
                case 0: pin = KEY3_Pin; port = KEY3_GPIO_Port; break;
                case 1: pin = KEY4_Pin; port = KEY4_GPIO_Port; break;
                case 2: pin = KEY5_Pin; port = KEY5_GPIO_Port; break;
                case 3: pin = KEY6_Pin; port = KEY6_GPIO_Port; break;
                default: break;
            }
            uint8_t st = (HAL_GPIO_ReadPin(port, pin) == GPIO_PIN_RESET) ? 1U : 0U;
            key_status[i].state   = st;
            key_status[i].pressed = st;   /* 仅在确认按下时产生事件 */
            key_status[i].pending = 0U;
        }
    }
}

uint8_t Key_GetPressed(uint8_t key_index)
{
    if (key_index >= 4) return 0;
    if (key_status[key_index].pressed)
    {
        key_status[key_index].pressed = 0;   /* 读后清零，单次触发 */
        return 1;
    }
    return 0;
}

uint8_t Key_GetState(uint8_t key_index)
{
    if (key_index >= 4) return 0;
    return key_status[key_index].state;
}

void Key_Release(uint8_t key_index)
{
    if (key_index >= 4) return;
    key_status[key_index].state = 0;
}
