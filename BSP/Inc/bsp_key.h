#ifndef __BSP_KEY_H
#define __BSP_KEY_H

#include <stdint.h>


#define KEY_DEBOUNCE_MS 10

/* 按键索引：0=KEY3 返回、1=KEY4 上/增、2=KEY5 循环、3=KEY6 确认 */
uint8_t Key_GetPressed(uint8_t key_index);
uint8_t Key_GetState(uint8_t key_index);
void    Key_Release(uint8_t key_index);
void    Key_Task(void);

#endif
