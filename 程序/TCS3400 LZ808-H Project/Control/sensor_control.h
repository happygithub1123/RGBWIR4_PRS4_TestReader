#ifndef __SENSOR_CONTROL_H__
#define __SENSOR_CONTROL_H__ // 防止本项目统一传感器控制接口被重复包含

#include "Public_cfg.h"

void SensorControl_Init(void); // 初始化本项目环境光和压力两个控制器
bit SensorControl_Start(uint8 command_code, uint16 parameter); // 按Pin1和工作值2分发传感器操作
void SensorControl_Poll(void); // 轮询当前环境光或压力异步操作
ProjectAsyncResult_t SensorControl_TakeResult(float *value); // 获取并消费当前传感器命令结果
bit SensorControl_IsBusy(void); // 查询本项目是否已有传感器命令占用控制器

#endif
