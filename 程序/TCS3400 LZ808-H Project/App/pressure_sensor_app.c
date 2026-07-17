/**
 * @file pressure_sensor_app.c
 * @brief 压力传感器通讯应用层实现
 *
 * @details
 * 本模块面向压力传感器业务：
 *  - 组织后续要发出的 Modbus 指令
 *  - 接收并缓存压力传感器返回帧
 *  - 预留不同压力传感器差异化解析入口
 *
 * 当前阶段只搭建框架，不实现具体寄存器协议。
 */

#include "pressure_sensor_app.h"
#include "modbus_uart2.h"

/*==============================================================================
 * 宏定义
 *============================================================================*/

#define PRESSURE_SENSOR_FRAME_MAX_SIZE  64

/*==============================================================================
 * 模块内部变量
 *============================================================================*/

static uint8 g_pressure_frame_buf[PRESSURE_SENSOR_FRAME_MAX_SIZE];
static uint16 g_pressure_frame_len = 0;

/*==============================================================================
 * 对外开放函数实现
 *============================================================================*/

void Pressure_Sensor_App_Init(void)
{
    g_pressure_frame_len = 0;
    ModbusUart2_Init();
}

void Pressure_Sensor_App_Poll(void)
{
    ModbusUart2_Poll();

    if (ModbusUart2_GetFrame(g_pressure_frame_buf, &g_pressure_frame_len))
    {
        Pressure_Sensor_App_ProcessFrame(g_pressure_frame_buf, g_pressure_frame_len);
    }
}

bit Pressure_Sensor_App_RequestSample(void)
{
    /*
     * 预留压力传感器采样请求接口。
     * 具体 Modbus 功能码、站号、寄存器地址待后续型号确定后实现。
     */
    return 0;
}

void Pressure_Sensor_App_ProcessFrame(const uint8 *buf, uint16 len)
{
    /*
     * 预留压力传感器返回帧解析入口。
     * 当前阶段只搭应用层框架，不处理具体寄存器数据。
     */
    if ((buf == 0) || (len == 0))
    {
        return;
    }
}
