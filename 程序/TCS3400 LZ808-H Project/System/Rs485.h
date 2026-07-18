#ifndef __RS485_H__
#define __RS485_H__

#include "Public_cfg.h"

void Rs485_Init(void);                                    /* 初始化RS485为接收状态。 */
bit Rs485_SendBuffer(const uint8 *buffer, uint8 length);  /* 发送一帧RS485数据。 */
bit Rs485_ReadByte(uint8 *dat);                           /* 非阻塞读取接收字节。 */
void Rs485_ClearRx(void);                                 /* 清空RS485接收缓冲。 */
bit Rs485_TakeRxOverflow(void);                           /* 获取并清除接收溢出标志。 */

#endif
