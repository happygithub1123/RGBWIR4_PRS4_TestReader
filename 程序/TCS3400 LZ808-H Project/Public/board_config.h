#ifndef __BOARD_CONFIG_H__
#define __BOARD_CONFIG_H__ // 防止本项目板级参数被重复定义

#define PROJECT_FOSC_HZ                   22118400UL // 本项目外部晶振频率，用于核对串口和定时器时基

#define TCS3400_SENSOR_COUNT              4u    // 本项目主板连接的TCS3400传感器总路数
#define TCS3400_DEFAULT_ATIME_REGISTER    0xF6u // 四路TCS3400上电默认写入的27.8ms积分寄存器值
#define TCS3400_DEFAULT_INTEGRATION_PARAMETER 28u // 上位机未设置积分时间时采用的028协议档位
#define TCS3400_DEFAULT_FRAME_WAIT_MS     40u   // 默认27.8ms积分档等待一帧数据完成的工程时间
#define TCS3400_DEFAULT_GAIN_REGISTER     0x01u // 四路TCS3400上电默认写入的4x增益寄存器值
#define TCS3400_DEFAULT_GAIN_VALUE        4u    // 设置成功后向本项目上层记录和返回的默认4x增益

#define TCS3400_FRAME_MARGIN_MS           10u // TCS3400积分超时在档位时间之外保留的通信余量
#define TCS3400_DATA_POLL_INTERVAL_MS     2u  // 状态机查询TCS3400 AVALID的最小轮询间隔
#define MODBUS_RESPONSE_TIMEOUT_MS        200u // 每次压力仪表Modbus请求等待响应的最长时间，恢复实机原稳定窗口
#define MODBUS_SLAVE_ADDRESS_MIN          1u   // 本项目允许填写的最小压力仪表地址
#define MODBUS_SLAVE_ADDRESS_MAX          247u // 本项目允许填写的最大压力仪表地址

/*
 * 四路软件 I2C：
 * BUS1 P0.4=SCL、P5.2=SDA；BUS2 P0.1=SCL、P0.2=SDA；
 * BUS3 P4.5=SCL、P4.6=SDA；BUS4 P2.5=SCL、P2.6=SDA。
 * UART1 P3.0=RXD、P3.1=TXD；UART2 P1.0=RXD2、P1.1=TXD2；
 * RS485 /RE与DE并接并通过上拉连接P0.7，R与D各有上拉；
 * P0.7低电平接收、高电平发送；Y1~Y4控制=P3.2、P3.3、P3.4、P5.0；
 * MCU控制脚高电平时ULN2003将对应Y端拉低；运行指示灯=P5.4。
 */

#endif
