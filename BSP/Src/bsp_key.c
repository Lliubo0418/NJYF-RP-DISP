/* 按键 GPIO 引脚仍在 CubeMX 生成的 MX_GPIO_Init() 中初始化，
 * 本文件只负责 EXTI 中断中的消抖采样与状态机。
 */
#include "bsp_key.h"
#include "gpio.h"   /* KEY3_Pin / KEY3_GPIO_Port 等 CubeMX 生成宏 */
#include "delay.h"  /* delay_ms */

typedef struct {
    uint8_t state;     /* 当前电平（按下=1） */
    uint8_t pressed;   /* 一次性“按下事件”标志 */
} KeyStatusTypeDef;

static KeyStatusTypeDef key_status[4] = {0};

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    delay_ms(KEY_DEBOUNCE_MS);

    switch (GPIO_Pin)
    {
        case KEY3_Pin:
            key_status[0].state   = (HAL_GPIO_ReadPin(KEY3_GPIO_Port, KEY3_Pin) == GPIO_PIN_RESET) ? 1 : 0;
            key_status[0].pressed = key_status[0].state;
            __HAL_GPIO_EXTI_CLEAR_IT(KEY3_Pin);
            break;

        case KEY4_Pin:
            key_status[1].state   = (HAL_GPIO_ReadPin(KEY4_GPIO_Port, KEY4_Pin) == GPIO_PIN_RESET) ? 1 : 0;
            key_status[1].pressed = key_status[1].state;
            __HAL_GPIO_EXTI_CLEAR_IT(KEY4_Pin);
            break;

        case KEY5_Pin:
            key_status[2].state   = (HAL_GPIO_ReadPin(KEY5_GPIO_Port, KEY5_Pin) == GPIO_PIN_RESET) ? 1 : 0;
            key_status[2].pressed = key_status[2].state;
            __HAL_GPIO_EXTI_CLEAR_IT(KEY5_Pin);
            break;

        case KEY6_Pin:
            key_status[3].state   = (HAL_GPIO_ReadPin(KEY6_GPIO_Port, KEY6_Pin) == GPIO_PIN_RESET) ? 1 : 0;
            key_status[3].pressed = key_status[3].state;
            __HAL_GPIO_EXTI_CLEAR_IT(KEY6_Pin);
            break;

        default:
            break;
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
