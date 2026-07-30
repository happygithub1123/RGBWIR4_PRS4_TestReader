#ifndef __XTALSELECT_H__
#define __XTALSELECT_H__ // 防止本项目外部晶振切换接口被重复包含

#include "Public_cfg.h"

bit XtalSelect_Init(void); // 启动22.1184MHz外部晶振，稳定后切换并返回本项目时钟状态

#endif
