#ifndef __MODBUS_RTU_H__
#define __MODBUS_RTU_H__ // 防止本项目压力Modbus接口被重复包含

#include "Public_cfg.h"

typedef enum
{
    MODBUS_RTU_RESULT_NONE = 0, // 本项目中表示压力响应仍未完成
    MODBUS_RTU_RESULT_SUCCESS,  // 本项目中表示完整响应已经通过校验
    MODBUS_RTU_RESULT_ERROR     // 本项目中表示压力事务发生通信或格式错误
} ModbusRtuResult_t; // 本项目中向压力仪表层交付Modbus事务结果

void ModbusRtu_Init(void); // 初始化本项目Modbus RTU主机
bit ModbusRtu_StartReadHolding(uint8 slave, uint16 address, uint16 count); // 启动本项目保持寄存器读取
bit ModbusRtu_StartWriteSingleCoil(uint8 slave, uint16 address, uint16 value); // 启动本项目清零线圈写入
void ModbusRtu_Poll(void); // 轮询本项目压力响应和超时
bit ModbusRtu_IsBusy(void); // 查询本项目Modbus事务忙状态
ModbusRtuResult_t ModbusRtu_TakeResponse(uint8 *buffer, uint8 buffer_size,
                                         uint8 *length); // 在容量范围内获取并消费本项目完整响应

#endif
