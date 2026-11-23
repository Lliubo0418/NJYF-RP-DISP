<<<<<<< HEAD
#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include <stdint.h>

/* 板载按键驱动 (STM32F1 显示板)
 * 原实现位于 Core/Src/gpio.c 的 USER CODE BEGIN 2 区，
 * 现按 RD_L496 的分层规范抽到 BSP 层。
 *
 * 按键索引与 main.c 中 Key_GetPressed(n) 的 n 对应：
 *   0 -> KEY3 返回键
 *   1 -> KEY4 上键 / 增加
 *   2 -> KEY5 循环键
 *   3 -> KEY6 确认键
 */
#define KEY_DEBOUNCE_MS 10

uint8_t Key_GetPressed(uint8_t key_index);
uint8_t Key_GetState(uint8_t key_index);
void    Key_Release(uint8_t key_index);

#endif /* __BSP_KEY_H */
=======
#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include <stdint.h>

/* 板载按键驱动 (STM32F1 显示板)：EXTI 登记 + 主循环非阻塞消抖 */

/* 消抖窗口(ms)：机械轻触键弹跳 5~20ms，调小易一次按出多个事件 */
#define KEY_DEBOUNCE_MS 10

/* 按键索引：0=KEY3 返回、1=KEY4 上/增、2=KEY5 循环、3=KEY6 确认 */
uint8_t Key_GetPressed(uint8_t key_index);
uint8_t Key_GetState(uint8_t key_index);
void    Key_Release(uint8_t key_index);
void    Key_Task(void);   /* 主循环调用：非阻塞消抖 + 状态采样 */

#endif
>>>>>>> origin/develop
