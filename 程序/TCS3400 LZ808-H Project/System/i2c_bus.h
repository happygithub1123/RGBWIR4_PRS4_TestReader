/**
 * @file i2c_bus.h
 * @brief 四路通用软件 I2C 总线对外接口定义
 *
 * @details
 * 定义四路软件 I2C 总线编号、GPIO 时序参数、基础总线操作，
 * 以及单寄存器和连续寄存器读写接口。
 */

#ifndef __I2C_BUS_H__
#define __I2C_BUS_H__

#include "Public_cfg.h"

/*==============================================================================
 * 公共类型定义
 *============================================================================*/

/**
 * @brief 传感器软件 I2C 总线编号
 */
typedef enum
{
    SENSOR_I2C_BUS_1 = 0,  /**< 第1路：P2.5=SCL，P2.6=SDA */
    SENSOR_I2C_BUS_2,      /**< 第2路：P4.5=SCL，P4.6=SDA */
    SENSOR_I2C_BUS_3,      /**< 第3路：P0.1=SCL，P0.2=SDA */
    SENSOR_I2C_BUS_4,      /**< 第4路：P0.4=SCL，P5.2=SDA */
    SENSOR_I2C_BUS_COUNT   /**< 软件 I2C 总线总数 */
} Sensor_I2C_Bus_t;

/** 兼容现有 TCS3400 驱动使用的旧总线类型名称 */
typedef Sensor_I2C_Bus_t I2C_Bus_t;

/*==============================================================================
 * 兼容旧总线名称
 *============================================================================*/

#define SOFT_I2C_BUS_1  SENSOR_I2C_BUS_1  /**< 兼容旧第1路软件 I2C 名称 */
#define SOFT_I2C_BUS_2  SENSOR_I2C_BUS_2  /**< 兼容旧第2路软件 I2C 名称 */
#define SOFT_I2C_BUS_3  SENSOR_I2C_BUS_3  /**< 兼容旧第3路软件 I2C 名称 */
#define SOFT_I2C_BUS_4  SENSOR_I2C_BUS_4  /**< 兼容旧第4路软件 I2C 名称 */

/*==============================================================================
 * 软件 I2C 时序参数
 *============================================================================*/

/**
 * @brief 执行一次软件 I2C 延时空操作
 */
#define SENSOR_I2C_NOP()  _nop_()

/*
 * 以下参数为延时循环计数值。
 * 实际时间还会受到函数调用、循环、GPIO 操作及编译优化影响。
 *
 * 当前参数目标：
 *  - SCL 频率约 50 kHz；
 *  - 适用于较长连接线和测试治具环境；
 *  - 系统时钟为 22.1184 MHz、1T 模式。
 */

/* START、STOP 和总线空闲时序 */
#define SENSOR_I2C_T_SU_STA  180  /**< START 建立时间延时计数 */
#define SENSOR_I2C_T_HD_STA  144  /**< START 保持时间延时计数 */
#define SENSOR_I2C_T_SU_STO  144  /**< STOP 建立时间延时计数 */
#define SENSOR_I2C_T_BUF     180  /**< 两次通信之间的总线空闲延时计数 */

/* 数据建立和保持时序 */
#define SENSOR_I2C_T_SU_DAT   36  /**< SDA 数据建立时间延时计数 */
#define SENSOR_I2C_T_HD_DAT   36  /**< SDA 数据保持时间延时计数 */

/* SCL 高、低电平时序 */
#define SENSOR_I2C_T_HIGH    144  /**< SCL 高电平延时计数 */
#define SENSOR_I2C_T_LOW     180  /**< SCL 低电平延时计数 */

/*==============================================================================
 * 兼容旧时序宏名称
 *============================================================================*/

#define I2C_NOP()       SENSOR_I2C_NOP()       /**< 兼容旧空操作宏 */
#define I2C_T_SU_STA    SENSOR_I2C_T_SU_STA    /**< 兼容旧 START 建立时间宏 */
#define I2C_T_HD_STA    SENSOR_I2C_T_HD_STA    /**< 兼容旧 START 保持时间宏 */
#define I2C_T_SU_STO    SENSOR_I2C_T_SU_STO    /**< 兼容旧 STOP 建立时间宏 */
#define I2C_T_BUF       SENSOR_I2C_T_BUF       /**< 兼容旧总线空闲时间宏 */
#define I2C_T_SU_DAT    SENSOR_I2C_T_SU_DAT    /**< 兼容旧数据建立时间宏 */
#define I2C_T_HD_DAT    SENSOR_I2C_T_HD_DAT    /**< 兼容旧数据保持时间宏 */
#define I2C_T_HIGH      SENSOR_I2C_T_HIGH      /**< 兼容旧 SCL 高电平时间宏 */
#define I2C_T_LOW       SENSOR_I2C_T_LOW       /**< 兼容旧 SCL 低电平时间宏 */

/*==============================================================================
 * 对外开放函数声明
 *============================================================================*/

void Sensor_I2C_Init(void);                                                                   /**< 初始化并释放全部四路软件 I2C 总线 */
void Sensor_I2C_Start(Sensor_I2C_Bus_t bus);                                                   /**< 在指定总线上产生 START 信号 */
void Sensor_I2C_Stop(Sensor_I2C_Bus_t bus);                                                    /**< 在指定总线上产生 STOP 信号 */
bit Sensor_I2C_WaitAck(Sensor_I2C_Bus_t bus);                                                  /**< 检查从机应答，0=ACK，1=NACK */
void Sensor_I2C_Ack(Sensor_I2C_Bus_t bus);                                                     /**< 主机向从机发送 ACK */
void Sensor_I2C_NAck(Sensor_I2C_Bus_t bus);                                                    /**< 主机向从机发送 NACK */
void Sensor_I2C_SendByte(Sensor_I2C_Bus_t bus, uint8 dat);                                     /**< 通过指定总线发送一个字节 */
uint8 Sensor_I2C_ReadByte(Sensor_I2C_Bus_t bus);                                               /**< 通过指定总线读取一个字节 */
bit Sensor_I2C_WriteReg(Sensor_I2C_Bus_t bus, uint8 dev, uint8 reg, uint8 dat);                /**< 向从机写入一个寄存器 */
uint8 Sensor_I2C_ReadReg(Sensor_I2C_Bus_t bus, uint8 dev, uint8 reg);                           /**< 读取一个寄存器，不返回通信状态 */
bit Sensor_I2C_ReadRegValue(Sensor_I2C_Bus_t bus, uint8 dev, uint8 reg, uint8 *value);          /**< 读取一个寄存器并返回通信状态 */
bit Sensor_I2C_ReadRegs(Sensor_I2C_Bus_t bus, uint8 dev, uint8 reg, uint8 *buf, uint8 len);     /**< 连续读取多个寄存器 */
bit Sensor_I2C_WriteRegs(Sensor_I2C_Bus_t bus, uint8 dev, uint8 reg, uint8 *buf, uint8 len);    /**< 连续写入多个寄存器 */

#endif /* __I2C_BUS_H__ */

