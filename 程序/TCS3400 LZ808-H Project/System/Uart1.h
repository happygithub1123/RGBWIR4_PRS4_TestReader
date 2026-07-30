#ifndef __UART1_H__
#define __UART1_H__ // 防止本项目MTF串口UART1接口被重复包含

#include "Public_cfg.h"

void Uart1_Init(void); // 初始化本项目MTF通信使用的115200bps、8N1串口
bit Uart1_ReadByte(uint8 *dat); // 非阻塞读取一个MTF请求字节
void Uart1_ClearRx(void); // 清空MTF请求接收缓冲和溢出状态
bit Uart1_TakeRxOverflow(void); // 获取并清除MTF请求接收溢出标志
bit Uart1_SendByte(uint8 dat); // 发送一个MTF Float响应字节
bit Uart1_SendBuffer(const uint8 *buffer, uint8 length); // 按顺序发送完整MTF Float响应

#endif
