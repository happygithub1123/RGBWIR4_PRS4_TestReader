#ifndef __TCS3400_CONTROL_H__
#define __TCS3400_CONTROL_H__

#include "Public_cfg.h"

void Tcs3400Control_Init(void); /* 初始化四路TCS3400及独立配置状态。 */
bit Tcs3400Control_Start(uint8 target_code, uint8 function_code, uint16 parameter); /* 启动TCS命令。 */
void Tcs3400Control_Poll(void); /* 轮询目标通道采样。 */
ProjectAsyncResult_t Tcs3400Control_TakeResult(float *value); /* 获取并消费命令结果。 */
bit Tcs3400Control_IsBusy(void); /* 查询TCS控制器忙状态。 */

#endif
