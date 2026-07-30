#ifndef __SYSTEM_H__
#define __SYSTEM_H__ // 防止本项目系统初始化接口被重复包含

#include "Public_cfg.h"

bit Sys_Init(void); // 初始化本项目系统资源并返回22.1184MHz外部晶振是否可用

#endif
