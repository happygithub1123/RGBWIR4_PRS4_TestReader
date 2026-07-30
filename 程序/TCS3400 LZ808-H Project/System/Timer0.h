#ifndef __TIMER0_H__
#define __TIMER0_H__ // 防止本项目1ms系统时基接口被重复包含

#include "Public_cfg.h"

void TM0_Init(void); // 初始化本项目所有非阻塞状态机共用的1ms时基
uint32 Timer0_GetMs(void); // 原子读取本项目当前系统毫秒计数
bit Timer0_HasElapsed(uint32 start_ms, uint16 period_ms); // 判断环境光、压力或运行灯等待时间是否到达

#endif
