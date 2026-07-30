#ifndef __APP_COMM_H__
#define __APP_COMM_H__ // 防止本项目MTF应用通信接口被重复包含

#include "Public_cfg.h"

void AppComm_Init(void); // 初始化本项目MTF请求接收和一问一答状态
void AppComm_Poll(void); // 轮询本项目MTF收帧、传感器命令和Float响应

#endif
