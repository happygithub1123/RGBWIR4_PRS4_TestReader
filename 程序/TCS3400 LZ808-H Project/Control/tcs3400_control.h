#ifndef __TCS3400_CONTROL_H__
#define __TCS3400_CONTROL_H__ // 防止本项目TCS3400控制层接口被重复包含

#include "Public_cfg.h"

void Tcs3400Control_Init(void); // 初始化本项目四路TCS3400及稳定采样状态
bit Tcs3400Control_Start(uint8 target_code, uint8 function_code, uint16 parameter); // 启动本项目TCS命令
void Tcs3400Control_Poll(void); // 轮询本项目RGBC采样和IR后的Clear恢复
ProjectAsyncResult_t Tcs3400Control_TakeResult(float *value); // 获取并消费本项目环境光结果
bit Tcs3400Control_IsBusy(void); // 查询本项目TCS控制器忙状态

#endif
