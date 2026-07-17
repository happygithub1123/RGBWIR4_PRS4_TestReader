/**
 * @file Uart.h
 * @brief UART1 串口通信驱动对外接口定义
 *
 * @details
 * 提供 UART1 初始化、阻塞发送、环形缓冲区接收、
 * printf 重定向和常用数值打印接口。
 */

#ifndef __UART_H__
#define __UART_H__

#include "Public_cfg.h"

/*==============================================================================
 * 对外开放函数声明
 *============================================================================*/

void Uart1_Init(void);                              /**< 初始化 UART1，配置为 115200bps、8N1 */
void Uart_SendByte(unsigned char dat);              /**< 阻塞发送一个字节 */
void Uart_SendStr(const unsigned char *Str);        /**< 发送以空字符结尾的字符串 */
bit Uart_ReadByte(uint8 *dat);                      /**< 非阻塞读取一个接收字节，1=成功，0=无数据 */
char putchar(char c);                               /**< 将 printf 字符输出重定向到 UART1 */
void Uart_PrintUint16(unsigned int num);            /**< 打印 uint16 十进制文本 */
void Uart_PrintHex8(unsigned char dat);             /**< 打印两位大写十六进制文本，不带 0x */

#endif /* __UART_H__ */
