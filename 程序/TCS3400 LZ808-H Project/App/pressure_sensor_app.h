/**
 * @file pressure_sensor_app.h
 * @brief 压力传感器通讯应用层对外接口定义
 *
 * @details
 * 本模块负责压力传感器相关 Modbus 指令的组织、解析与发送流程，
 * 并调用 Modbus UART2 驱动层完成通讯。
 */

#ifndef __PRESSURE_SENSOR_APP_H__
#define __PRESSURE_SENSOR_APP_H__

#include "Public_cfg.h"

void Pressure_Sensor_App_Init(void);                                  /**< 初始化压力传感器通讯应用层 */
void Pressure_Sensor_App_Poll(void);                                  /**< 压力传感器应用层轮询任务 */
bit Pressure_Sensor_App_RequestSample(void);                          /**< 预留采样请求接口 */
void Pressure_Sensor_App_ProcessFrame(const uint8 *buf, uint16 len);  /**< 处理一帧压力传感器返回数据 */

#endif /* __PRESSURE_SENSOR_APP_H__ */
