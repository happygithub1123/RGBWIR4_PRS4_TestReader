/**
 * @file tcs3400_driver.c
 * @brief TCS3400 颜色传感器底层驱动实现
 *
 * @details
 * 本模块负责 TCS3400 的寄存器访问、初始化、积分时间配置、
 * 增益配置、数据有效状态检测，以及 RGBC 和 IR 原始数据读取。
 */

#include "tcs3400_driver.h"
#include "i2c_bus.h"

/*==============================================================================
 * 模块内部宏定义
 *============================================================================*/

#define TCS3400_AUTOINC  0x20                              /**< 寄存器地址自动递增命令位，当前代码暂未使用 */
#define U16_LE(lo, hi)    (((uint16)(hi) << 8) | (lo))     /**< 将低字节和高字节组合成 16 位小端数值 */

/*==============================================================================
 * 模块内部变量
 *============================================================================*/

/** 各路 TCS3400 当前配置对应的帧等待时间，单位 ms */
static uint16 g_tcs3400_frame_wait_ms[4] = {183, 183, 183, 183};

/*==============================================================================
 * 模块内部函数声明
 *============================================================================*/

static bit TCS_Write8(I2C_Bus_t bus, uint8 reg, uint8 val);
static uint8 TCS_Read8(I2C_Bus_t bus, uint8 reg);
static bit TCS_Read8Value(I2C_Bus_t bus, uint8 reg, uint8 *value);
static bit TCS_Read16Value(I2C_Bus_t bus, uint8 reg_low, uint16 *value);
static uint16 TCS_Read16(I2C_Bus_t bus, uint8 reg_low);
static uint16 TCS3400_CalcCycles(uint16 time_ms);
static bit TCS3400_WaitValid(I2C_Bus_t bus);

/*==============================================================================
 * 模块内部函数实现
 *============================================================================*/

/**
 * @brief 向 TCS3400 写入一个 8 位寄存器
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] reg 目标寄存器地址
 * @param[in] val 待写入的寄存器值
 * @return 1=I2C 写入成功，0=I2C 写入失败
 */
static bit TCS_Write8(I2C_Bus_t bus, uint8 reg, uint8 val)
{
    return Sensor_I2C_WriteReg(bus,
                               TCS3400_I2C_ADDR_7BIT,
                               reg,
                               val);
}

/**
 * @brief 读取 TCS3400 的一个 8 位寄存器
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] reg 目标寄存器地址
 * @return 读取到的寄存器值
 * @note 该兼容接口不返回 I2C ACK 状态。
 */
static uint8 TCS_Read8(I2C_Bus_t bus, uint8 reg)
{
    return Sensor_I2C_ReadReg(bus,
                              TCS3400_I2C_ADDR_7BIT,
                              reg);
}

/**
 * @brief 读取 TCS3400 的一个 8 位寄存器并检查通信状态
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] reg 目标寄存器地址
 * @param[out] value 返回读取到的寄存器值
 * @return 1=I2C 读取成功，0=I2C 读取失败
 */
static bit TCS_Read8Value(I2C_Bus_t bus,
                          uint8 reg,
                          uint8 *value)
{
    return Sensor_I2C_ReadRegValue(bus,
                                   TCS3400_I2C_ADDR_7BIT,
                                   reg,
                                   value);
}

/**
 * @brief 从连续的低、高字节寄存器读取一个 16 位数值
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] reg_low 低字节寄存器地址
 * @param[out] value 返回组合后的 16 位数值
 * @return 1=输出指针有效并完成读取，0=输出指针为空
 * @note 当前实现通过 TCS_Read8() 分两次读取，不能反馈 I2C ACK 错误。
 */
static bit TCS_Read16Value(I2C_Bus_t bus,
                           uint8 reg_low,
                           uint16 *value)
{
    uint8 d0;
    uint8 d1;

    if (value == 0)
    {
        return 0;
    }

    d0 = TCS_Read8(bus, reg_low);
    d1 = TCS_Read8(bus, reg_low + 1);

    *value = ((uint16)d1 << 8) | d0;

    return 1;
}

/**
 * @brief 从连续的低、高字节寄存器读取一个 16 位数值
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] reg_low 低字节寄存器地址
 * @return 读取并组合后的 16 位数值
 * @note 当前接口不反馈 I2C ACK 错误。
 */
static uint16 TCS_Read16(I2C_Bus_t bus, uint8 reg_low)
{
    uint8 low;
    uint8 high;

    low = TCS_Read8(bus, reg_low);
    high = TCS_Read8(bus, reg_low + 1);

    return ((uint16)high << 8) | low;
}

/**
 * @brief 将积分时间换算为 TCS3400 ADC 积分周期数
 * @param[in] time_ms 目标积分时间，单位 ms
 * @return ADC 积分周期数，范围为 1～256
 * @note 输入积分时间会被限制在 3～712 ms。
 */
static uint16 TCS3400_CalcCycles(uint16 time_ms)
{
    uint32 temp;
    uint16 cycles;

    if (time_ms < 3)
    {
        time_ms = 3;
    }

    if (time_ms > 712)
    {
        time_ms = 712;
    }

    /* 常用积分时间使用固定周期数，避免换算误差 */
    if (time_ms == 28)
    {
        return 10;
    }

    if (time_ms == 103)
    {
        return 37;
    }

    if (time_ms == 178)
    {
        return 64;
    }

    if (time_ms == 712)
    {
        return 256;
    }

    /*
     * 每个积分周期约为 2.78 ms。
     * 加 277 后进行整数除法，相当于向上取整。
     */
    temp = ((uint32)time_ms * 100UL) + 277UL;
    cycles = (uint16)(temp / 278UL);

    if (cycles < 1)
    {
        cycles = 1;
    }

    if (cycles > 256)
    {
        cycles = 256;
    }

    return cycles;
}

/**
 * @brief 等待旧数据失效并等待一帧新数据变为有效
 * @param[in] bus 目标软件 I2C 总线
 * @return 1=检测到新的有效数据，0=通信失败或等待超时
 * @note 本函数包含 Delay_ms(2) 阻塞延时。
 */
static bit TCS3400_WaitValid(I2C_Bus_t bus)
{
    uint16 t;
    uint16 timeout;
    uint8 status;

    timeout = (g_tcs3400_frame_wait_ms[(uint8)bus] / 2) + 20;

    /*
     * 第一阶段：
     * 等待上一帧的 AVALID 清零，避免读取旧数据。
     */
    t = timeout;

    while (t--)
    {
        if (!TCS_Read8Value(bus, TCS3400_STATUS, &status))
        {
            return 0;
        }

        if ((status & TCS3400_STATUS_AVALID) == 0)
        {
            break;
        }

        Delay_ms(2);
    }

    /*
     * 第二阶段：
     * 等待新一帧的 AVALID 置位。
     */
    t = timeout;

    while (t--)
    {
        if (!TCS_Read8Value(bus, TCS3400_STATUS, &status))
        {
            return 0;
        }

        if (status & TCS3400_STATUS_AVALID)
        {
            return 1;
        }

        Delay_ms(2);
    }

    return 0;
}

/*==============================================================================
 * 对外开放函数实现
 *============================================================================*/

/**
 * @brief 初始化指定总线上的 TCS3400
 * @param[in] bus 目标软件 I2C 总线
 * @return 1=初始化成功，0=任意寄存器写入失败
 */
bit TCS3400_Init(I2C_Bus_t bus)
{
    /* 开启芯片电源 */
    if (!TCS_Write8(bus,
                    TCS3400_ENABLE,
                    TCS3400_EN_PON))
    {
        return 0;
    }

    /* 等待芯片上电稳定 */
    Delay_ms(3);

    /* 设置默认积分时间为 178 ms */
    if (!TCS3400_SetIntegrationTime(bus, 178))
    {
        return 0;
    }

    /* 设置默认模拟增益 */
    if (!TCS3400_SetGain(bus, TCS3400_DEFAULT_GAIN))
    {
        return 0;
    }

    /* 开启芯片电源和 RGBC ADC */
    if (!TCS_Write8(bus,
                    TCS3400_ENABLE,
                    TCS3400_EN_PON | TCS3400_EN_AEN))
    {
        return 0;
    }

    return 1;
}

/**
 * @brief 将积分时间换算为 ATIME 寄存器值
 * @param[in] time_ms 目标积分时间，单位 ms
 * @return ATIME 寄存器值
 */
uint8 TCS3400_CalcATIME(uint16 time_ms)
{
    uint16 cycles;

    cycles = TCS3400_CalcCycles(time_ms);

    return (uint8)(256 - cycles);
}

/**
 * @brief 设置 TCS3400 的积分时间
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] time_ms 目标积分时间，单位 ms
 * @return 1=设置成功，0=ATIME 寄存器写入失败
 * @note 同时更新该总线对应的应用等待时间，并额外增加 5 ms 裕量。
 */
bit TCS3400_SetIntegrationTime(I2C_Bus_t bus,
                               uint16 time_ms)
{
    uint16 cycles;
    uint16 wait_ms;

    cycles = TCS3400_CalcCycles(time_ms);

    if (!TCS3400_SetATIME(bus, (uint8)(256 - cycles)))
    {
        return 0;
    }

    wait_ms =
        (uint16)((((uint32)cycles * 278UL) + 99UL) / 100UL);

    g_tcs3400_frame_wait_ms[(uint8)bus] = wait_ms + 5;

    return 1;
}

/**
 * @brief 获取指定总线当前配置对应的帧等待时间
 * @param[in] bus 目标软件 I2C 总线
 * @return 帧等待时间，单位 ms
 */
uint16 TCS3400_GetFrameWaitMs(I2C_Bus_t bus)
{
    return g_tcs3400_frame_wait_ms[(uint8)bus];
}

/**
 * @brief 直接设置 ATIME 寄存器
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] atime ATIME 寄存器值
 * @return 1=写入成功，0=写入失败
 */
bit TCS3400_SetATIME(I2C_Bus_t bus, uint8 atime)
{
    return TCS_Write8(bus, TCS3400_ATIME, atime);
}

/**
 * @brief 设置 TCS3400 模拟增益
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] gain 增益编码，仅低两位有效
 * @return 1=写入成功，0=写入失败
 */
bit TCS3400_SetGain(I2C_Bus_t bus, uint8 gain)
{
    return TCS_Write8(bus,
                      TCS3400_CONTROL,
                      (gain & 0x03));
}

/**
 * @brief 设置 TCS3400 等待时间寄存器
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] wtime WTIME 寄存器值
 * @return 1=写入成功，0=写入失败
 */
bit TCS3400_SetWTIME(I2C_Bus_t bus, uint8 wtime)
{
    return TCS_Write8(bus, TCS3400_WTIME, wtime);
}

/**
 * @brief 读取 TCS3400 芯片 ID
 * @param[in] bus 目标软件 I2C 总线
 * @param[out] id 返回芯片 ID 寄存器值
 * @return 1=读取成功，0=输出指针为空或通信失败
 */
bit TCS3400_ReadID(I2C_Bus_t bus, uint8 *id)
{
    if (id == 0)
    {
        return 0;
    }

    return Sensor_I2C_ReadRegs(bus,
                               TCS3400_I2C_ADDR_7BIT,
                               TCS3400_ID,
                               id,
                               1);
}

/**
 * @brief 读取 TCS3400 的指定 8 位寄存器
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] reg 目标寄存器地址
 * @param[out] value 返回读取到的寄存器值
 * @return 1=读取成功，0=输出指针为空或通信失败
 */
bit TCS3400_ReadRegValue(I2C_Bus_t bus,
                         uint8 reg,
                         uint8 *value)
{
    if (value == 0)
    {
        return 0;
    }

    return Sensor_I2C_ReadRegValue(bus,
                                   TCS3400_I2C_ADDR_7BIT,
                                   reg,
                                   value);
}

/**
 * @brief 读取 STATUS 寄存器中的 AVALID 状态
 * @param[in] bus 目标软件 I2C 总线
 * @param[out] valid 返回数据有效状态，1=有效，0=无效
 * @return 1=STATUS 寄存器读取成功，0=输出指针为空或通信失败
 */
bit TCS3400_GetDataValid(I2C_Bus_t bus, uint8 *valid)
{
    uint8 status;

    if (valid == 0)
    {
        return 0;
    }

    if (!TCS_Read8Value(bus, TCS3400_STATUS, &status))
    {
        return 0;
    }

    *valid =
        (status & TCS3400_STATUS_AVALID) ? 1 : 0;

    return 1;
}

/**
 * @brief 强制重新启动一次 RGBC ADC 积分
 * @param[in] bus 目标软件 I2C 总线
 * @return 1=两次 ENABLE 寄存器写入成功，0=任意一次写入失败
 * @note 通过先关闭 AEN、再重新开启 AEN 实现。
 */
bit TCS3400_RestartIntegration(I2C_Bus_t bus)
{
    /* 保持 PON，关闭 AEN */
    if (!TCS_Write8(bus,
                    TCS3400_ENABLE,
                    TCS3400_EN_PON))
    {
        return 0;
    }

    /* 保持 PON，重新开启 AEN */
    if (!TCS_Write8(bus,
                    TCS3400_ENABLE,
                    TCS3400_EN_PON | TCS3400_EN_AEN))
    {
        return 0;
    }

    return 1;
}

/**
 * @brief 不等待 AVALID，直接读取 RGBC 原始数据
 * @param[in] bus 目标软件 I2C 总线
 * @param[out] r 返回 Red 通道原始计数
 * @param[out] g 返回 Green 通道原始计数
 * @param[out] b 返回 Blue 通道原始计数
 * @param[out] c 返回 Clear 通道原始计数
 * @return 1=输出指针有效并完成读取，0=存在空指针或读取失败
 * @note 当前底层 16 位读取接口不能反馈单字节 I2C ACK 错误。
 */
bit TCS3400_ReadRGBC_NoWait(I2C_Bus_t bus,
                            uint16 *r,
                            uint16 *g,
                            uint16 *b,
                            uint16 *c)
{
    if ((r == 0) ||
        (g == 0) ||
        (b == 0) ||
        (c == 0))
    {
        return 0;
    }

    if (!TCS_Read16Value(bus, TCS3400_CDATAL, c))
    {
        return 0;
    }

    if (!TCS_Read16Value(bus, TCS3400_RDATAL, r))
    {
        return 0;
    }

    if (!TCS_Read16Value(bus, TCS3400_GDATAL, g))
    {
        return 0;
    }

    if (!TCS_Read16Value(bus, TCS3400_BDATAL, b))
    {
        return 0;
    }

    return 1;
}

/**
 * @brief 不等待 AVALID，直接读取 Clear 数据端口
 * @param[in] bus 目标软件 I2C 总线
 * @param[out] value 返回 Clear 或 IR 映射后的 16 位原始计数
 * @return 1=读取成功，0=输出指针为空或通信失败
 */
bit TCS3400_ReadClearPort_NoWait(I2C_Bus_t bus,
                                 uint16 *value)
{
    uint8 lo;
    uint8 hi;

    if (value == 0)
    {
        return 0;
    }

    if (!TCS_Read8Value(bus, TCS3400_CDATAL, &lo))
    {
        return 0;
    }

    if (!TCS_Read8Value(bus, TCS3400_CDATAH, &hi))
    {
        return 0;
    }

    *value = ((uint16)hi << 8) | lo;

    return 1;
}

/**
 * @brief 等待一帧新数据后读取 RGBC 原始计数
 * @param[in] bus 目标软件 I2C 总线
 * @param[out] r 返回 Red 通道原始计数
 * @param[out] g 返回 Green 通道原始计数
 * @param[out] b 返回 Blue 通道原始计数
 * @param[out] c 返回 Clear 通道原始计数
 * @return 1=等待及读取成功，0=等待超时、通信失败或参数错误
 * @note 本函数会阻塞等待新数据。
 */
bit TCS3400_ReadRGBC(I2C_Bus_t bus,
                     uint16 *r,
                     uint16 *g,
                     uint16 *b,
                     uint16 *c)
{
    if (!TCS3400_WaitValid(bus))
    {
        return 0;
    }

    return TCS3400_ReadRGBC_NoWait(bus, r, g, b, c);
}

/**
 * @brief 设置 Clear 数据端口的 IR 映射状态
 * @param[in] bus 目标软件 I2C 总线
 * @param[in] en 1=开启 IR 映射，0=关闭 IR 映射
 * @return 1=设置成功，0=寄存器写入失败
 * @note 设置完成后固定延时 3 ms，用于寄存器模式切换稳定。
 */
bit TCS3400_SetIRMapEnable(I2C_Bus_t bus, bit en)
{
    uint8 value;

    value =
        en ? TCS3400_IR_ENABLE : TCS3400_IR_DISABLE;

    if (!TCS_Write8(bus, TCS3400_IR, value))
    {
        return 0;
    }

    /* 等待寄存器模式切换稳定，不代表等待一帧积分 */
    Delay_ms(3);

    return 1;
}

/**
 * @brief 读取 IR 通道原始计数
 * @param[in] bus 目标软件 I2C 总线
 * @return IR 原始计数；等待失败时返回 0
 * @note 本函数会开启 IR 映射、阻塞等待新帧，然后恢复普通 Clear 模式。
 */
uint16 TCS3400_ReadIR(I2C_Bus_t bus)
{
    uint16 ir;

    TCS3400_SetIRMapEnable(bus, 1);

    if (!TCS3400_WaitValid(bus))
    {
        TCS3400_SetIRMapEnable(bus, 0);
        return 0;
    }

    ir = TCS_Read16(bus, TCS3400_CDATAL);

    TCS3400_SetIRMapEnable(bus, 0);

    /* 等待关闭 IR 映射后的普通 Clear 数据恢复 */
    TCS3400_WaitValid(bus);

    return ir;
}

/**
 * @brief 读取 IR 映射状态下的 RGB 和 IR 原始数据
 * @param[in] bus 目标软件 I2C 总线
 * @param[out] r 返回 Red 通道原始计数
 * @param[out] g 返回 Green 通道原始计数
 * @param[out] b 返回 Blue 通道原始计数
 * @param[out] ir 返回 IR 通道原始计数
 * @return 1=读取及恢复成功，0=参数错误、等待超时或读取失败
 * @note 本函数会开启 IR 映射，读取后关闭 IR 映射并等待普通模式恢复。
 */
bit TCS3400_ReadRGBIR(I2C_Bus_t bus,
                      uint16 *r,
                      uint16 *g,
                      uint16 *b,
                      uint16 *ir)
{
    uint8 read_ok;

    read_ok = 1;

    if ((r == 0) ||
        (g == 0) ||
        (b == 0) ||
        (ir == 0))
    {
        return 0;
    }

    if (!TCS3400_SetIRMapEnable(bus, 1))
    {
        return 0;
    }

    if (!TCS3400_WaitValid(bus))
    {
        TCS3400_SetIRMapEnable(bus, 0);
        return 0;
    }

    /*
     * IR 映射模式下：
     * Clear 数据端口对应 IR 原始计数，
     * R、G、B 通道仍从各自寄存器读取。
     */
    if (!TCS_Read16Value(bus, TCS3400_CDATAL, ir))
    {
        read_ok = 0;
    }

    if (read_ok && !TCS_Read16Value(bus, TCS3400_RDATAL, r))
    {
        read_ok = 0;
    }

    if (read_ok && !TCS_Read16Value(bus, TCS3400_GDATAL, g))
    {
        read_ok = 0;
    }

    if (read_ok && !TCS_Read16Value(bus, TCS3400_BDATAL, b))
    {
        read_ok = 0;
    }

    TCS3400_SetIRMapEnable(bus, 0);

    if (!read_ok)
    {
        return 0;
    }

    /* 等待关闭 IR 映射后的普通模式恢复 */
    if (!TCS3400_WaitValid(bus))
    {
        return 0;
    }

    return 1;
}       

/**
 * @brief 读取 Red 通道原始计数
 * @param[in] bus 目标软件 I2C 总线
 * @return Red 通道原始计数；读取失败时返回初始值 0
 */
uint16 TCS3400_ReadR(I2C_Bus_t bus)
{
    uint16 r = 0;
    uint16 g = 0;
    uint16 b = 0;
    uint16 c = 0;

    TCS3400_ReadRGBC(bus, &r, &g, &b, &c);

    return r;
}

/**
 * @brief 读取 Green 通道原始计数
 * @param[in] bus 目标软件 I2C 总线
 * @return Green 通道原始计数；读取失败时返回初始值 0
 */
uint16 TCS3400_ReadG(I2C_Bus_t bus)
{
    uint16 r = 0;
    uint16 g = 0;
    uint16 b = 0;
    uint16 c = 0;

    TCS3400_ReadRGBC(bus, &r, &g, &b, &c);

    return g;
}

/**
 * @brief 读取 Blue 通道原始计数
 * @param[in] bus 目标软件 I2C 总线
 * @return Blue 通道原始计数；读取失败时返回初始值 0
 */
uint16 TCS3400_ReadB(I2C_Bus_t bus)
{
    uint16 r = 0;
    uint16 g = 0;
    uint16 b = 0;
    uint16 c = 0;

    TCS3400_ReadRGBC(bus, &r, &g, &b, &c);

    return b;
}

/**
 * @brief 读取 Clear 通道原始计数
 * @param[in] bus 目标软件 I2C 总线
 * @return Clear 通道原始计数；读取失败时返回初始值 0
 */
uint16 TCS3400_ReadC(I2C_Bus_t bus)
{
    uint16 r = 0;
    uint16 g = 0;
    uint16 b = 0;
    uint16 c = 0;

    TCS3400_ReadRGBC(bus, &r, &g, &b, &c);

    return c;
}

/**
 * @brief 仅读取 IR 通道原始计数
 * @param[in] bus 目标软件 I2C 总线
 * @return IR 通道原始计数；读取失败时返回初始值 0
 */
uint16 TCS3400_ReadIR_Only(I2C_Bus_t bus)
{
    uint16 r = 0;
    uint16 g = 0;
    uint16 b = 0;
    uint16 ir = 0;

    TCS3400_ReadRGBIR(bus, &r, &g, &b, &ir);

    return ir;
}

/**
 * @brief 读取 RGBC 和 IR 原始数据
 * @param[in] bus 目标软件 I2C 总线
 * @param[out] r 返回 Red 通道原始计数
 * @param[out] g 返回 Green 通道原始计数
 * @param[out] b 返回 Blue 通道原始计数
 * @param[out] c 返回 Clear 通道原始计数
 * @param[out] ir 返回 IR 通道原始计数
 * @return 1=两次读取均成功，0=任意一次读取失败
 * @note 第二次 TCS3400_ReadRGBIR() 会重新写入 r、g、b。
 */
bit TCS3400_ReadRaw(I2C_Bus_t bus,
                    uint16 *r,
                    uint16 *g,
                    uint16 *b,
                    uint16 *c,
                    uint16 *ir)
{
    if (!TCS3400_ReadRGBC(bus, r, g, b, c))
    {
        return 0;
    }

    if (!TCS3400_ReadRGBIR(bus, r, g, b, ir))
    {
        return 0;
    }

    return 1;
}

