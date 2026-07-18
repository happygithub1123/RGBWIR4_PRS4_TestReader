#ifndef __PRESSURE_METER_H__
#define __PRESSURE_METER_H__

#include "Public_cfg.h"

typedef enum
{
    PRESSURE_METER_READ_VALUE = 0,
    PRESSURE_METER_READ_DECIMAL,
    PRESSURE_METER_ZERO
} PressureMeterOperation_t;

typedef enum
{
    PRESSURE_METER_RESULT_NONE = 0,
    PRESSURE_METER_RESULT_SUCCESS,
    PRESSURE_METER_RESULT_ERROR
} PressureMeterResult_t;

void PressureMeter_Init(void); /* 初始化压力仪表通信层。 */
bit PressureMeter_Start(PressureMeterOperation_t operation, uint8 slave); /* 启动仪表操作。 */
void PressureMeter_Poll(void); /* 轮询仪表响应。 */
PressureMeterResult_t PressureMeter_TakeResult(int16 *value); /* 获取并消费仪表结果。 */

#endif
