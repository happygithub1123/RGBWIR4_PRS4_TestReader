#ifndef __TRIGGER_OUTPUT_H__
#define __TRIGGER_OUTPUT_H__ // 防止本项目压力越限Y输出接口被重复包含

#include "Public_cfg.h"

void TriggerOutput_Init(void); // 初始化本项目Y1至Y4压力越限输出为释放状态
void TriggerOutput_Set(uint8 channel, uint8 active); // 按压力比较结果控制指定Y通道

#endif
