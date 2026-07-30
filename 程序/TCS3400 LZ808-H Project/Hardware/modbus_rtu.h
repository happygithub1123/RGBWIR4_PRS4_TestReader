#ifndef __MODBUS_RTU_H__
#define __MODBUS_RTU_H__

#include "Public_cfg.h"

typedef enum
{
    MODBUS_RTU_RESULT_NONE = 0,
    MODBUS_RTU_RESULT_SUCCESS,
    MODBUS_RTU_RESULT_ERROR
} ModbusRtuResult_t;

void ModbusRtu_Init(void); /* 初始化Modbus RTU主机。 */
bit ModbusRtu_StartReadHolding(uint8 slave, uint16 address); /* 启动单个保持寄存器读取。 */
bit ModbusRtu_StartWriteSingleCoil(uint8 slave, uint16 address, uint16 value); /* 启动单线圈写入。 */
void ModbusRtu_Poll(void); /* 轮询接收、重发和超时。 */
bit ModbusRtu_IsBusy(void); /* 查询事务忙状态。 */
ModbusRtuResult_t ModbusRtu_TakeResult(uint16 *value); /* 获取并消费已校验结果。 */
uint16 ModbusRtu_CalcCrc(const uint8 *buffer, uint8 length); /* 计算Modbus CRC16。 */

#endif
