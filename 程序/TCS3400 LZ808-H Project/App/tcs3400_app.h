/**
 * @file tcs3400_app.h
 * @brief TCS3400 应用层对外接口定义
 *
 * @details
 * 提供上位机命令处理、四路 TCS3400 初始化和应用层轮询接口。
 */

#ifndef __TCS3400_APP_H__
#define __TCS3400_APP_H__

#include "Public_cfg.h"
#include "Uart.h"
#include "Timer0.h"
#include "tcs3400_driver.h"

/*==============================================================================
 * 对外开放函数声明
 *============================================================================*/

void TCS3400_App_ProcessLine(char *line);  /**< 处理一行不包含 CRLF 的完整上位机命令 */
void TCS3400_App_Init(void);               /**< 初始化全部 4 路 TCS3400 传感器 */
void TCS3400_App_Poll(void);               /**< 处理串口命令并驱动传感器采样状态机 */

#endif
