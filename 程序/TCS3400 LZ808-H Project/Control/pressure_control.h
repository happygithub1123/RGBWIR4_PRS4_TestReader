#ifndef __PRESSURE_CONTROL_H__
#define __PRESSURE_CONTROL_H__

#include "Public_cfg.h"

void PressureControl_Init(void); /* 初始化压力控制器。 */
bit PressureControl_StartRead(uint8 slave); /* 启动压力读取。 */
bit PressureControl_StartZero(uint8 slave); /* 启动压力清零。 */
void PressureControl_Poll(void); /* 轮询压力事务。 */
ProjectAsyncResult_t PressureControl_TakeResult(float *value); /* 获取并消费压力结果。 */
bit PressureControl_IsBusy(void); /* 查询压力控制器忙状态。 */

#endif
