/**
 * @file Uart2.h
 * @brief UART2 串口通信驱动对外接口定义
 *
 * @details
 * 本模块只负责 UART2 外设初始化、中断接收、发送接口和环形缓冲区，
 * 不包含 Modbus 协议、CRC、站号或 485 方向控制逻辑。
 */

#ifndef __UART2_H__
#define __UART2_H__

#include "Public_cfg.h"

void Uart2_Init(void);                                    /**< 初始化 UART2，配置为 115200bps、8N1 */
bit Uart2_SendByte(uint8 dat);                            /**< 通过 UART2 阻塞发送一个字节 */
bit Uart2_SendBuffer(const uint8 *buf, uint16 len);       /**< 通过 UART2 发送一段原始缓冲区 */
bit Uart2_ReadByte(uint8 *dat);                           /**< 从 UART2 接收缓冲区读取一个字节 */

#endif /* __UART2_H__ */
