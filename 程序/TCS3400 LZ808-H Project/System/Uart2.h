#ifndef __UART2_H__
#define __UART2_H__ // 防止本项目压力RS485串口UART2接口被重复包含

#include "Public_cfg.h"

void Uart2_Init(void); // 初始化本项目压力RS485使用的115200bps、8N1串口
bit Uart2_ReadByte(uint8 *dat); // 非阻塞读取一个压力Modbus响应字节
void Uart2_ClearRx(void); // 清空压力Modbus接收缓冲和溢出状态
bit Uart2_TakeRxOverflow(void); // 获取并清除压力响应接收溢出标志
bit Uart2_SendByte(uint8 dat); // 发送一个压力Modbus请求字节
bit Uart2_SendBuffer(const uint8 *buffer, uint8 length); // 按顺序发送完整压力Modbus请求并等待完成

#endif
