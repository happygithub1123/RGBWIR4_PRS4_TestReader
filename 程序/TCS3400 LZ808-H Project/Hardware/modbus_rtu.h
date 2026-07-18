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
bit ModbusRtu_StartReadHolding(uint8 slave, uint16 address, uint16 count); /* 启动保持寄存器读取。 */
bit ModbusRtu_StartWriteSingleCoil(uint8 slave, uint16 address, uint16 value); /* 启动单线圈写入。 */
void ModbusRtu_Poll(void); /* 轮询接收和超时。 */
bit ModbusRtu_IsBusy(void); /* 查询事务忙状态。 */
ModbusRtuResult_t ModbusRtu_TakeResponse(uint8 *buffer, uint8 *length); /* 获取并消费响应。 */
uint16 ModbusRtu_CalcCrc(const uint8 *buffer, uint8 length); /* 计算Modbus CRC16。 */

#endif
