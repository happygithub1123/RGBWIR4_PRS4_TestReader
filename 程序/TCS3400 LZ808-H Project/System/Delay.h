#ifndef __DELAY_H__
#define __DELAY_H__ // 防止本项目上电硬件稳定延时接口被重复包含

#include "Public_cfg.h"

void Delay_ms(uint16 delay_ms); // 本项目仅在上电阶段等待外设稳定

#endif
