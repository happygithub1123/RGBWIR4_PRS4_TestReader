/**
 * @file modbus_uart2.h
 * @brief Modbus UART2 驱动对外接口定义
 *
 * @details
 * 本模块依赖 UART2 外设层，负责：
 *  - 485 方向控制
 *  - Modbus RTU 原始帧发送
 *  - 接收帧组包
 *  - 帧间超时判断
 *  - CRC16 计算与基础校验
 */

#ifndef __MODBUS_UART2_H__
#define __MODBUS_UART2_H__

#include "Public_cfg.h"

void ModbusUart2_Init(void);                                   /**< 初始化 Modbus UART2 驱动 */
void ModbusUart2_Poll(void);                                   /**< 轮询接收字节并完成 Modbus 帧组包 */
bit ModbusUart2_SendRequest(const uint8 *buf, uint16 len);     /**< 发送一帧 Modbus RTU 原始请求 */
bit ModbusUart2_GetFrame(uint8 *buf, uint16 *len);             /**< 获取一帧已完成接收的 Modbus RTU 原始帧 */
uint16 ModbusUart2_CalcCrc(const uint8 *buf, uint16 len);      /**< 计算 Modbus CRC16 */

#endif /* __MODBUS_UART2_H__ */
