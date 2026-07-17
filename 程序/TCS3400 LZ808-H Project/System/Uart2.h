#ifndef __UART2_H__
#define __UART2_H__

#include "Public_cfg.h"

void Uart2_Init(void);                                    /* 初始化UART2为115200bps、8N1。 */
bit Uart2_ReadByte(uint8 *dat);                           /* 非阻塞读取1字节。 */
void Uart2_ClearRx(void);                                 /* 清空接收缓冲和溢出标志。 */
bit Uart2_TakeRxOverflow(void);                           /* 获取并清除接收溢出标志。 */
bit Uart2_SendByte(uint8 dat);                            /* 发送1字节。 */
bit Uart2_SendBuffer(const uint8 *buffer, uint8 length); /* 发送缓冲区并等待完成。 */

#endif
