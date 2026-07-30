#ifndef __RUN_LED_H__
#define __RUN_LED_H__ // 防止本项目运行指示灯接口被重复包含

#include "Public_cfg.h"

void RunLed_Init(void); // 初始化本项目主板运行指示灯和计时起点
void RunLed_Poll(void); // 每1秒翻转指示灯以显示主循环仍在运行

#endif
