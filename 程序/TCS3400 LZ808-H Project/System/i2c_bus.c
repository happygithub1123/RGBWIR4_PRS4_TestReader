/**
 * @file i2c_bus.c
 * @brief 四路通用软件 I2C 总线驱动实现
 *
 * @details
 * 本模块使用 GPIO 模拟四路独立 I2C 总线，提供 START、STOP、
 * ACK、NACK、单字节收发，以及传感器寄存器组合读写功能。
 */

#include "i2c_bus.h"

/*==============================================================================
 * 软件 I2C 引脚定义
 *============================================================================*/


sbit SENSOR_I2C_SCL1 = P2^5;  /**< 第1路软件 I2C 时钟引脚：P2.5 */
sbit SENSOR_I2C_SDA1 = P2^6;  /**< 第1路软件 I2C 数据引脚：P2.6 */

sbit SENSOR_I2C_SCL2 = P4^5;  /**< 第2路软件 I2C 时钟引脚：P4.5 */
sbit SENSOR_I2C_SDA2 = P4^6;  /**< 第2路软件 I2C 数据引脚：P4.6 */

sbit SENSOR_I2C_SCL3 = P0^1;  /**< 第3路软件 I2C 时钟引脚：P0.1 */
sbit SENSOR_I2C_SDA3 = P0^2;  /**< 第3路软件 I2C 数据引脚：P0.2 */

sbit SENSOR_I2C_SCL4 = P0^4;  /**< 第4路软件 I2C 时钟引脚：P0.4 */
sbit SENSOR_I2C_SDA4 = P5^2;  /**< 第4路软件 I2C 数据引脚：P5.2 */


/*==============================================================================
 * 模块内部函数声明
 *============================================================================*/

static void Sensor_I2C_SetSCL(Sensor_I2C_Bus_t bus, bit val);
static void Sensor_I2C_SetSDA(Sensor_I2C_Bus_t bus, bit val);
static bit Sensor_I2C_GetSDA(Sensor_I2C_Bus_t bus);
static void Sensor_I2C_DelayN(uint8 n);

/*==============================================================================
 * 模块内部函数实现
 *============================================================================*/

/**
 * @brief 设置指定软件 I2C 总线的 SCL 电平
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] val 目标电平，0=低电平，1=释放为高电平
 */
static void Sensor_I2C_SetSCL(Sensor_I2C_Bus_t bus, bit val)
{
    switch (bus)
    {
        case SENSOR_I2C_BUS_1:
            SENSOR_I2C_SCL1 = val;
            break;

        case SENSOR_I2C_BUS_2:
            SENSOR_I2C_SCL2 = val;
            break;

        case SENSOR_I2C_BUS_3:
            SENSOR_I2C_SCL3 = val;
            break;

        case SENSOR_I2C_BUS_4:
            SENSOR_I2C_SCL4 = val;
            break;
    }
}

/**
 * @brief 设置指定软件 I2C 总线的 SDA 电平
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] val 目标电平，0=拉低，1=释放为高电平
 */
static void Sensor_I2C_SetSDA(Sensor_I2C_Bus_t bus, bit val)
{
    switch (bus)
    {
        case SENSOR_I2C_BUS_1:
            SENSOR_I2C_SDA1 = val;
            break;

        case SENSOR_I2C_BUS_2:
            SENSOR_I2C_SDA2 = val;
            break;

        case SENSOR_I2C_BUS_3:
            SENSOR_I2C_SDA3 = val;
            break;

        case SENSOR_I2C_BUS_4:
            SENSOR_I2C_SDA4 = val;
            break;
    }
}

/**
 * @brief 读取指定软件 I2C 总线的 SDA 当前电平
 * @param[in] bus 目标软件 I2C 总线
 * @return SDA 当前电平，0=低电平，1=高电平
 * @note 总线编号无效时默认返回高电平。
 */
static bit Sensor_I2C_GetSDA(Sensor_I2C_Bus_t bus)
{
    switch (bus)
    {
        case SENSOR_I2C_BUS_1:
            return SENSOR_I2C_SDA1;

        case SENSOR_I2C_BUS_2:
            return SENSOR_I2C_SDA2;

        case SENSOR_I2C_BUS_3:
            return SENSOR_I2C_SDA3;

        case SENSOR_I2C_BUS_4:
            return SENSOR_I2C_SDA4;
    }

    return 1;
}

/**
 * @brief 执行指定次数的软件 I2C 空操作延时
 * @param[in] n 空操作循环次数
 */
static void Sensor_I2C_DelayN(uint8 n)
{
    while (n--)
    {
        SENSOR_I2C_NOP();
    }
}

/*==============================================================================
 * 对外开放函数实现
 *============================================================================*/

/**
 * @brief 初始化并释放全部四路软件 I2C 总线
 * @note 将所有 SDA 和 SCL 引脚置高，使总线进入空闲状态。
 */
void Sensor_I2C_Init(void)
{
    SENSOR_I2C_SDA1 = 1;
    SENSOR_I2C_SCL1 = 1;

    SENSOR_I2C_SDA2 = 1;
    SENSOR_I2C_SCL2 = 1;

    SENSOR_I2C_SDA3 = 1;
    SENSOR_I2C_SCL3 = 1;

    SENSOR_I2C_SDA4 = 1;
    SENSOR_I2C_SCL4 = 1;
}

/**
 * @brief 在指定软件 I2C 总线上产生 START 信号
 * @param[in] bus 目标软件 I2C 总线
 * @note 当 SCL 为高电平时，使 SDA 从高电平变为低电平。
 */
void Sensor_I2C_Start(Sensor_I2C_Bus_t bus)
{
    Sensor_I2C_SetSDA(bus, 1);
    Sensor_I2C_SetSCL(bus, 1);

    Sensor_I2C_DelayN(SENSOR_I2C_T_SU_STA);

    Sensor_I2C_SetSDA(bus, 0);

    Sensor_I2C_DelayN(SENSOR_I2C_T_HD_STA);

    Sensor_I2C_SetSCL(bus, 0);
}

/**
 * @brief 在指定软件 I2C 总线上产生 STOP 信号
 * @param[in] bus 目标软件 I2C 总线
 * @note 当 SCL 为高电平时，使 SDA 从低电平变为高电平。
 */
void Sensor_I2C_Stop(Sensor_I2C_Bus_t bus)
{
    Sensor_I2C_SetSDA(bus, 0);

    Sensor_I2C_DelayN(SENSOR_I2C_T_HD_DAT);

    Sensor_I2C_SetSCL(bus, 1);

    Sensor_I2C_DelayN(SENSOR_I2C_T_SU_STO);

    Sensor_I2C_SetSDA(bus, 1);

    Sensor_I2C_DelayN(SENSOR_I2C_T_BUF);
}

/**
 * @brief 检查从机是否返回 ACK
 * @param[in] bus 目标软件 I2C 总线
 * @return 0=收到 ACK，1=未收到 ACK
 * @note 本函数的返回规则为 0 表示成功应答，1 表示无应答。
 */
bit Sensor_I2C_WaitAck(Sensor_I2C_Bus_t bus)
{
    /* 释放 SDA，使从机能够输出应答电平 */
    Sensor_I2C_SetSDA(bus, 1);

    Sensor_I2C_DelayN(SENSOR_I2C_T_HD_DAT);

    Sensor_I2C_SetSCL(bus, 1);

    Sensor_I2C_DelayN(SENSOR_I2C_T_HIGH);

    /* SDA 为高电平表示从机未应答 */
    if (Sensor_I2C_GetSDA(bus))
    {
        Sensor_I2C_SetSCL(bus, 0);

        Sensor_I2C_DelayN(SENSOR_I2C_T_LOW);

        return 1;
    }

    /* SDA 为低电平表示收到 ACK */
    Sensor_I2C_SetSCL(bus, 0);

    Sensor_I2C_DelayN(SENSOR_I2C_T_LOW);

    return 0;
}

/**
 * @brief 主机向从机发送 ACK
 * @param[in] bus 目标软件 I2C 总线
 * @note 通常在主机还需要继续读取后续字节时调用。
 */
void Sensor_I2C_Ack(Sensor_I2C_Bus_t bus)
{
    Sensor_I2C_SetSDA(bus, 0);

    Sensor_I2C_DelayN(SENSOR_I2C_T_SU_DAT);

    Sensor_I2C_SetSCL(bus, 1);

    Sensor_I2C_DelayN(SENSOR_I2C_T_HIGH);

    Sensor_I2C_SetSCL(bus, 0);

    Sensor_I2C_DelayN(SENSOR_I2C_T_LOW);

    /* ACK 发送完成后重新释放 SDA */
    Sensor_I2C_SetSDA(bus, 1);
}

/**
 * @brief 主机向从机发送 NACK
 * @param[in] bus 目标软件 I2C 总线
 * @note 通常在主机读取最后一个字节后调用。
 */
void Sensor_I2C_NAck(Sensor_I2C_Bus_t bus)
{
    Sensor_I2C_SetSDA(bus, 1);

    Sensor_I2C_DelayN(SENSOR_I2C_T_SU_DAT);

    Sensor_I2C_SetSCL(bus, 1);

    Sensor_I2C_DelayN(SENSOR_I2C_T_HIGH);

    Sensor_I2C_SetSCL(bus, 0);

    Sensor_I2C_DelayN(SENSOR_I2C_T_LOW);
}

/**
 * @brief 通过指定软件 I2C 总线发送一个字节
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] dat 待发送的 8 位数据
 * @note 数据按最高位优先顺序发送，发送结束后释放 SDA。
 */
void Sensor_I2C_SendByte(Sensor_I2C_Bus_t bus, uint8 dat)
{
    uint8 i;

    for (i = 0; i < 8; i++)
    {
        /* 先输出当前最高位 */
        Sensor_I2C_SetSDA(bus, (dat & 0x80) ? 1 : 0);

        Sensor_I2C_DelayN(SENSOR_I2C_T_SU_DAT);

        Sensor_I2C_SetSCL(bus, 1);

        Sensor_I2C_DelayN(SENSOR_I2C_T_HIGH);

        Sensor_I2C_SetSCL(bus, 0);

        Sensor_I2C_DelayN(SENSOR_I2C_T_LOW);

        dat <<= 1;
    }

    /* 释放 SDA，便于从机在第9个时钟输出 ACK */
    Sensor_I2C_SetSDA(bus, 1);
}

/**
 * @brief 通过指定软件 I2C 总线读取一个字节
 * @param[in] bus 目标软件 I2C 总线
 * @return 读取到的 8 位数据
 * @note 数据按最高位优先顺序接收，本函数不自动发送 ACK 或 NACK。
 */
uint8 Sensor_I2C_ReadByte(Sensor_I2C_Bus_t bus)
{
    uint8 i;
    uint8 dat;

    dat = 0;

    /* 释放 SDA，使从机可以驱动数据线 */
    Sensor_I2C_SetSDA(bus, 1);

    for (i = 0; i < 8; i++)
    {
        Sensor_I2C_SetSCL(bus, 0);

        Sensor_I2C_DelayN(SENSOR_I2C_T_LOW);

        Sensor_I2C_SetSCL(bus, 1);

        /* 在 SCL 高电平中间位置采样 SDA */
        Sensor_I2C_DelayN(SENSOR_I2C_T_HIGH / 2);

        dat <<= 1;

        if (Sensor_I2C_GetSDA(bus))
        {
            dat |= 0x01;
        }

        Sensor_I2C_DelayN(
            SENSOR_I2C_T_HIGH -
            SENSOR_I2C_T_HIGH / 2);
    }

    Sensor_I2C_SetSCL(bus, 0);

    Sensor_I2C_DelayN(SENSOR_I2C_T_LOW);

    return dat;
}

/**
 * @brief 向指定从机写入一个 8 位寄存器
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] dev 从机 7 位 I2C 地址
 * @param[in] reg 目标寄存器地址
 * @param[in] dat 待写入的寄存器值
 * @return 1=写入成功，0=连续两次写入均失败
 * @note 发生 NACK 时会停止当前通信并自动重试一次。
 */
bit Sensor_I2C_WriteReg(Sensor_I2C_Bus_t bus,
                        uint8 dev,
                        uint8 reg,
                        uint8 dat)
{
    uint8 retry;

    for (retry = 0; retry <= 1; retry++)
    {
        Sensor_I2C_Start(bus);

        /* 发送从机写地址 */
        Sensor_I2C_SendByte(bus, dev << 1);

        if (Sensor_I2C_WaitAck(bus))
        {
            Sensor_I2C_Stop(bus);
            continue;
        }

        /* 发送目标寄存器地址 */
        Sensor_I2C_SendByte(bus, reg);

        if (Sensor_I2C_WaitAck(bus))
        {
            Sensor_I2C_Stop(bus);
            continue;
        }

        /* 发送待写入数据 */
        Sensor_I2C_SendByte(bus, dat);

        if (Sensor_I2C_WaitAck(bus))
        {
            Sensor_I2C_Stop(bus);
            continue;
        }

        Sensor_I2C_Stop(bus);

        return 1;
    }

    Sensor_I2C_Stop(bus);

    return 0;
}

/**
 * @brief 读取指定从机的一个 8 位寄存器
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] dev 从机 7 位 I2C 地址
 * @param[in] reg 目标寄存器地址
 * @return 读取到的寄存器值
 * @note 本兼容接口不检查 ACK，无法区分通信失败和真实寄存器值。
 */
uint8 Sensor_I2C_ReadReg(Sensor_I2C_Bus_t bus,
                         uint8 dev,
                         uint8 reg)
{
    uint8 dat;

    /* 写入待读取的寄存器地址 */
    Sensor_I2C_Start(bus);

    Sensor_I2C_SendByte(bus, dev << 1);
    Sensor_I2C_WaitAck(bus);

    Sensor_I2C_SendByte(bus, reg);
    Sensor_I2C_WaitAck(bus);

    /* 重复起始并切换为读方向 */
    Sensor_I2C_Start(bus);

    Sensor_I2C_SendByte(bus, (dev << 1) | 1);
    Sensor_I2C_WaitAck(bus);

    dat = Sensor_I2C_ReadByte(bus);

    /* 单字节读取结束，主机发送 NACK */
    Sensor_I2C_NAck(bus);

    Sensor_I2C_Stop(bus);

    return dat;
}

/**
 * @brief 读取指定从机的一个 8 位寄存器并返回通信状态
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] dev 从机 7 位 I2C 地址
 * @param[in] reg 目标寄存器地址
 * @param[out] value 返回读取到的寄存器值
 * @return 1=读取成功，0=参数错误或通信失败
 */
bit Sensor_I2C_ReadRegValue(Sensor_I2C_Bus_t bus,
                            uint8 dev,
                            uint8 reg,
                            uint8 *value)
{
    if (value == 0)
    {
        return 0;
    }

    return Sensor_I2C_ReadRegs(bus,
                               dev,
                               reg,
                               value,
                               1);
}

/**
 * @brief 从指定起始寄存器连续读取多个字节
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] dev 从机 7 位 I2C 地址
 * @param[in] reg 起始寄存器地址
 * @param[out] buf 返回连续读取的数据
 * @param[in] len 需要读取的字节数
 * @return 1=读取成功，0=参数错误或任意通信阶段收到 NACK
 * @note 除最后一个字节发送 NACK 外，其余字节均发送 ACK。
 */
bit Sensor_I2C_ReadRegs(Sensor_I2C_Bus_t bus,
                        uint8 dev,
                        uint8 reg,
                        uint8 *buf,
                        uint8 len)
{
    uint8 i;

    if ((buf == 0) || (len == 0))
    {
        return 0;
    }

    /* 发送从机写地址 */
    Sensor_I2C_Start(bus);

    Sensor_I2C_SendByte(bus, dev << 1);

    if (Sensor_I2C_WaitAck(bus))
    {
        Sensor_I2C_Stop(bus);
        return 0;
    }

    /* 发送连续读取的起始寄存器地址 */
    Sensor_I2C_SendByte(bus, reg);

    if (Sensor_I2C_WaitAck(bus))
    {
        Sensor_I2C_Stop(bus);
        return 0;
    }

    /* 重复起始并切换到读方向 */
    Sensor_I2C_Start(bus);

    Sensor_I2C_SendByte(bus, (dev << 1) | 1);

    if (Sensor_I2C_WaitAck(bus))
    {
        Sensor_I2C_Stop(bus);
        return 0;
    }

    /* 连续读取指定数量的数据 */
    for (i = 0; i < len; i++)
    {
        buf[i] = Sensor_I2C_ReadByte(bus);

        if (i == (len - 1))
        {
            /* 最后一个字节读取完成后发送 NACK */
            Sensor_I2C_NAck(bus);
        }
        else
        {
            /* 还需要继续读取时发送 ACK */
            Sensor_I2C_Ack(bus);
        }
    }

    Sensor_I2C_Stop(bus);

    return 1;
}

/**
 * @brief 从指定起始寄存器连续写入多个字节
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] dev 从机 7 位 I2C 地址
 * @param[in] reg 起始寄存器地址
 * @param[in] buf 待写入的数据缓冲区
 * @param[in] len 需要写入的字节数
 * @return 1=写入成功，0=参数错误或任意通信阶段收到 NACK
 */
bit Sensor_I2C_WriteRegs(Sensor_I2C_Bus_t bus,
                         uint8 dev,
                         uint8 reg,
                         uint8 *buf,
                         uint8 len)
{
    uint8 i;

    if ((buf == 0) || (len == 0))
    {
        return 0;
    }

    Sensor_I2C_Start(bus);

    /* 发送从机写地址 */
    Sensor_I2C_SendByte(bus, dev << 1);

    if (Sensor_I2C_WaitAck(bus))
    {
        Sensor_I2C_Stop(bus);
        return 0;
    }

    /* 发送连续写入的起始寄存器地址 */
    Sensor_I2C_SendByte(bus, reg);

    if (Sensor_I2C_WaitAck(bus))
    {
        Sensor_I2C_Stop(bus);
        return 0;
    }

    /* 连续写入缓冲区中的数据 */
    for (i = 0; i < len; i++)
    {
        Sensor_I2C_SendByte(bus, buf[i]);

        if (Sensor_I2C_WaitAck(bus))
        {
            Sensor_I2C_Stop(bus);
            return 0;
        }
    }

    Sensor_I2C_Stop(bus);

    return 1;
}

