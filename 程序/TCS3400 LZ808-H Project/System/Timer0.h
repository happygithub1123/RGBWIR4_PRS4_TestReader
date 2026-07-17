#ifndef __TIMER0_H__
#define __TIMER0_H__

#include "Public_cfg.h"

/* 初始化Timer0，提供1ms系统节拍，并驱动P5.4运行指示灯 */
void TM0_Init(void);

/* 返回系统运行毫秒数，用于非阻塞任务调度 */
uint32 Timer0_GetMs(void);

#endif