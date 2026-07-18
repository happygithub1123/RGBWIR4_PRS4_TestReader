#ifndef __TIMER0_H__
#define __TIMER0_H__

#include "Public_cfg.h"

void TM0_Init(void);                                      /* 初始化1ms系统时基。 */
uint32 Timer0_GetMs(void);                                /* 获取系统毫秒计数。 */
bit Timer0_HasElapsed(uint32 start_ms, uint16 period_ms); /* 判断指定时间是否到达。 */

#endif
