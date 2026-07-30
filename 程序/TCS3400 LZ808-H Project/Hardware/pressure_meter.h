#ifndef __PRESSURE_METER_H__
#define __PRESSURE_METER_H__ // 防止本项目压力仪表接口被重复包含

#include "Public_cfg.h"

typedef enum
{
    PRESSURE_METER_READ_VALUE = 0, // 本项目中读取LZ808-H当前压力原始值
    PRESSURE_METER_READ_DECIMAL,   // 本项目中读取LZ808-H压力显示小数位
    PRESSURE_METER_ZERO            // 本项目中写线圈执行LZ808-H清零
} PressureMeterOperation_t; // 本项目中选择压力仪表的固定操作类型

typedef enum
{
    PRESSURE_METER_RESULT_NONE = 0, // 本项目中表示仪表响应仍未完成
    PRESSURE_METER_RESULT_SUCCESS,  // 本项目中表示压力操作响应校验成功
    PRESSURE_METER_RESULT_ERROR     // 本项目中表示压力操作通信或回显错误
} PressureMeterResult_t; // 本项目中向压力控制层交付仪表操作结果

void PressureMeter_Init(void); // 初始化本项目压力仪表通信层
bit PressureMeter_Start(PressureMeterOperation_t operation, uint8 slave); // 启动本项目仪表操作
void PressureMeter_Poll(void); // 轮询并解析本项目仪表响应
PressureMeterResult_t PressureMeter_TakeResult(int16 *value); // 获取并消费本项目仪表结果

#endif
