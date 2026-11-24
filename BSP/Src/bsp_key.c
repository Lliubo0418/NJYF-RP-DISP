#include "bsp_key.h"
#include "gpio.h"
#include "stm32f1xx_hal.h"

typedef struct {
    uint8_t  state;
    uint8_t  pressed;
    uint8_t  pending;
    uint32_t trigger_tick;
} KeyStatusTypeDef;

static KeyStatusTypeDef key_status[4] = {0};

/**
 * @brief  把 EXTI 引脚号映射为 key_status 索引
 * @param  GPIO_Pin 引脚掩码
 * @retval 0~3；非按键引脚返回 -1
 */
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

/**
 * @brief  EXTI 回调
 * @param  GPIO_Pin 触发中断的引脚掩码
 * @retval 无
 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    int8_t idx = Key_IndexByPin(GPIO_Pin);
    if (idx >= 0)
    {
        key_status[idx].pending      = 1U;
        key_status[idx].trigger_tick = HAL_GetTick();
    }
    __HAL_GPIO_EXTI_CLEAR_IT(GPIO_Pin);
}

/**
 * @brief  主循环轮询
 * @retval 无
 */
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
            key_status[i].pressed = st;
            key_status[i].pending = 0U;
        }
    }
}

/**
 * @brief  一次性"按下"事件
 * @param  key_index 按键索引 0~3
 * @retval 1=本帧有按下事件，0=无
 */
uint8_t Key_GetPressed(uint8_t key_index)
{
    if (key_index >= 4) return 0;
    if (key_status[key_index].pressed)
    {
        key_status[key_index].pressed = 0;
        return 1;
    }
    return 0;
}

/**
 * @brief  查询按键当前稳定电平
 * @param  key_index 按键索引 0~3
 * @retval 1=按下，0=松开或索引越界
 */
uint8_t Key_GetState(uint8_t key_index)
{
    if (key_index >= 4) return 0;
    return key_status[key_index].state;
}

/**
 * @brief  强制清除按键稳定电平
 * @param  key_index 按键索引 0~3
 * @retval 无
 */
void Key_Release(uint8_t key_index)
{
    if (key_index >= 4) return;
    key_status[key_index].state = 0;
}
