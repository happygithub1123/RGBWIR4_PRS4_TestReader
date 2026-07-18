#ifndef __SENSOR_CONTROL_H__
#define __SENSOR_CONTROL_H__

#include "Public_cfg.h"

void SensorControl_Init(void); /* 初始化TCS3400和压力控制器。 */
bit SensorControl_Start(uint8 command_code, uint16 parameter); /* 按AB指令码启动操作。 */
void SensorControl_Poll(void); /* 轮询当前传感器操作。 */
ProjectAsyncResult_t SensorControl_TakeResult(float *value); /* 获取并消费操作结果。 */
bit SensorControl_IsBusy(void); /* 查询统一控制器忙状态。 */

#endif
