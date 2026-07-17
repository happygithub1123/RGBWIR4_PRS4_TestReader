#ifndef __UART1_H__
#define __UART1_H__

#include "Public_cfg.h"

void Uart1_Init(void);                                  /* 初始化UART1为115200bps、8N1。 */
bit Uart1_ReadByte(uint8 *dat);                         /* 非阻塞读取1字节。 */
void Uart1_ClearRx(void);                               /* 清空接收缓冲和溢出标志。 */
bit Uart1_TakeRxOverflow(void);                         /* 获取并清除接收溢出标志。 */
bit Uart1_SendByte(uint8 dat);                          /* 发送1字节。 */
bit Uart1_SendBuffer(const uint8 *buffer, uint8 length); /* 发送缓冲区。 */

#endif
