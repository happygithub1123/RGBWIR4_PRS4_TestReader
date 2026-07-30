#ifndef __PRESSURE_CONTROL_H__
#define __PRESSURE_CONTROL_H__ // 防止本项目压力控制层接口被重复包含

#include "Public_cfg.h"

void PressureControl_Init(void); // 初始化本项目压力控制器和Y1至Y4限值
bit PressureControl_SetLowerLimit(uint8 channel, uint16 limit_value); // 设置本项目指定通道下限
bit PressureControl_SetUpperLimit(uint8 channel, uint16 limit_value); // 设置本项目指定通道上限
bit PressureControl_StartRead(uint8 slave); // 启动本项目普通压力读取
bit PressureControl_StartCompareRead(uint8 channel, uint8 slave); // 启动本项目读取并比较
bit PressureControl_StartZero(uint8 slave); // 启动本项目压力清零
void PressureControl_Poll(void); // 轮询本项目压力事务
ProjectAsyncResult_t PressureControl_TakeResult(float *value); // 获取并消费本项目压力结果
bit PressureControl_IsBusy(void); // 查询本项目压力控制器忙状态

#endif
