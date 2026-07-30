#ifndef __RS485_H__
#define __RS485_H__ // 防止本项目RS485收发接口被重复包含

#include "Public_cfg.h"

void Rs485_Init(void); // 初始化本项目压力总线并保持RS485接收状态
bit Rs485_SendBuffer(const uint8 *buffer, uint8 length); // 切换方向并发送一帧压力Modbus请求
bit Rs485_ReadByte(uint8 *dat); // 非阻塞读取一字节压力仪表响应
void Rs485_ClearRx(void); // 清空压力RS485接收缓冲和旧响应
bit Rs485_TakeRxOverflow(void); // 获取并清除压力响应接收溢出标志

#endif
