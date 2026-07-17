/**
 * @file tcs3400_app.c
 * @brief TCS3400 应用层协议解析、命令分发与非阻塞采样状态机实现
 *
 * @details
 * 本模块负责解析 MTF:AA-FF-PPP 格式上位机命令，管理 4 路 TCS3400，
 * 按需读取 R、G、B、Clear、IR、ID，并根据积分时间，通过串口
 * 以小端 IEEE 754 单精度浮点数返回响应帧。
 */

#include "tcs3400_app.h"
#include "tcs3400_driver.h"

/*==============================================================================
 * 宏定义
 *============================================================================*/

#define APP_LINE_MAX                  20  /**< 单行命令接收缓冲区长度 */
#define APP_CMD_LEN                   13  /**< 命令正文长度：MTF:AA-FF-PPP */
#define APP_CH_MAX                     4  /**< TCS3400 传感器通道数量 */
#define APP_SENSOR_RGBC                0  /**< 普通 RGBC 读取模式 */
#define APP_SENSOR_IR_WAIT             1  /**< IR 映射开启后的等待模式 */
#define APP_REPLY_EXTRA_TIMEOUT_MS   300  /**< 命令回复额外超时时间，单位 ms */

/*==============================================================================
 * 类型定义
 *============================================================================*/

/**
 * @brief 上位机命令对应的 TCS3400 数据通道
 */
typedef enum
{
    APP_CHANNEL_NONE = 0, /**< 无效通道 */
    APP_CHANNEL_R,        /**< 红色通道 */
    APP_CHANNEL_G,        /**< 绿色通道 */
    APP_CHANNEL_B,        /**< 蓝色通道 */
    APP_CHANNEL_CLEAR,    /**< Clear 通道 */
    APP_CHANNEL_IR        /**< 红外通道 */
} AppChannel_t;

/**
 * @brief 传感器采样状态
 */
typedef enum
{
    APP_SAMPLE_IDLE = 0,            /**< 空闲状态 */
    APP_SAMPLE_WAIT_RGBC,           /**< 等待普通 RGBC 积分帧 */
    APP_SAMPLE_WAIT_IR,             /**< 等待 IR 映射积分帧 */
    APP_SAMPLE_WAIT_CLEAR_RECOVERY  /**< 等待关闭 IR 映射后的 Clear 恢复帧 */
} AppSampleState_t;

/*==============================================================================
 * 模块内部变量
 *============================================================================*/

/** 传感器初始化完成标志 */
static bit g_sensor_inited = 0;

/** 下一帧采样到期时间戳，单位 ms */
static uint32 g_sample_due_ms = 0;

/** 各路传感器当前记录的积分时间，单位 ms */
static uint16 g_sample_time_ms[APP_CH_MAX] = {178, 178, 178, 178};

/** 当前是否存在等待回复的命令 */
static bit g_reply_pending = 0;

/** 当前等待回复命令对应的 I2C 总线 */
static I2C_Bus_t g_pending_bus = SENSOR_I2C_BUS_1;

/** 当前等待回复命令对应的数据通道 */
static AppChannel_t g_pending_channel = APP_CHANNEL_NONE;

/** 当前采样流程状态 */
static AppSampleState_t g_sample_state = APP_SAMPLE_IDLE;

/** IR 采样临时缓存 */
static uint16 g_pending_ir_value = 0;

/** 当前采样流程超时截止时刻，单位 ms */
static uint32 g_sample_deadline_ms = 0;

/*==============================================================================
 * 模块内部函数声明
 *============================================================================*/

static bit App_TimeReached(uint32 now, uint32 target);
static AppChannel_t App_CodeToChannel(uint8 channel_id);
static bit App_IsDigit(char c);
static uint16 App_ParseTimeMs(char *line);
static uint32 App_U16ToFloatBits(uint16 value);
static void App_SendFloatBits(uint32 bits);
static void App_SendFloatFrame(uint16 value);
static void App_SendErr(void);
static bit App_CheckFrameFormat(char *line);
static bit App_CheckParamDigits(char *line);
static uint8 App_ParseCode(char high, char low);
static bit App_ParamIs(char *line, char p0, char p1, char p2);
static I2C_Bus_t App_AA2Bus(uint8 aa);
static bit App_ReadID(I2C_Bus_t bus, uint8 *id);
static bit App_ParseGainParam(char *line,
                              uint8 *gain_reg,
                              uint16 *gain_value);
static uint16 App_GainRegToValue(uint8 value);
static uint16 App_ATimeToMs(uint8 atime);
static void App_SetIntegrationTime(I2C_Bus_t bus, uint16 time_ms);
static bit App_StartCommandSample(I2C_Bus_t bus,
                                  AppChannel_t channel,
                                  uint16 time_ms);
static void App_ClearPendingState(void);
static void App_AbortPendingSample(void);
static void App_PendingTask(void);
static void App_UartTask(void);

/*==============================================================================
 * 模块内部函数实现
 *============================================================================*/

/**
 * @brief 判断指定时刻是否已经到达
 * @param[in] now 当前系统时间戳，单位 ms
 * @param[in] target 目标时间戳，单位 ms
 * @return 1=已经到达或超过目标时间，0=尚未到达
 * @note 使用有符号差值判断，可兼容无符号毫秒计数自然回绕
 */
static bit App_TimeReached(uint32 now, uint32 target)
{
    return ((long)(now - target) >= 0) ? 1 : 0;
}

/**
 * @brief 将 FF 功能码转换为应用层数据通道
 * @param[in] ff 功能码，取值 1~5
 * @return 对应的 AppChannel_t，参数无效时返回 APP_CHANNEL_NONE
 */
static AppChannel_t App_CodeToChannel(uint8 channel_id)
{
    switch (channel_id)
    {
        case 1:  return APP_CHANNEL_R;
        case 2:  return APP_CHANNEL_G;
        case 3:  return APP_CHANNEL_B;
        case 4:  return APP_CHANNEL_CLEAR;
        case 5:  return APP_CHANNEL_IR;
        default: return APP_CHANNEL_NONE;
    }
}

/**
 * @brief 判断字符是否为十进制数字
 * @param[in] c 待检查字符
 * @return 1=字符位于 '0'~'9'，0=不是十进制数字
 */
static bit App_IsDigit(char c)
{
    return (c >= '0' && c <= '9');
}

/**
 * @brief 解析命令中的三位 PPP 积分时间参数
 * @param[in] line 以 '\0' 结尾的完整命令字符串
 * @return 解析得到的积分时间，单位 ms
 * @note 调用前应已通过 App_CheckParamDigits() 检查 PPP 字段。
 */
static uint16 App_ParseTimeMs(char *line)
{
    uint16 time_ms;

    time_ms = (uint16)(line[10] - '0') * 100;
    time_ms += (uint16)(line[11] - '0') * 10;
    time_ms += (uint16)(line[12] - '0');

    return time_ms;
}

/**
 * @brief 将 16 位无符号整数转换为 IEEE 754 单精度浮点位模式
 * @param[in] value 待转换的 16 位无符号整数
 * @return 数值等价的 IEEE 754 单精度浮点 32 位位模式
 * @note 输入范围为 0~65535，可被单精度浮点精确表示。
 */
static uint32 App_U16ToFloatBits(uint16 value)
{
    uint8 exp;
    uint16 base;
    uint16 rest;
    uint32 bits;

    if (value == 0)
    {
        return 0;
    }

    exp = 0;
    base = 1;

    while ((base <= 0x4000) &&
           ((uint16)(base << 1) <= value))
    {
        base <<= 1;
        exp++;
    }

    rest = value - base;

    /* 组合指数部分 */
    bits = ((uint32)(exp + 127) << 23);

    /* 组合尾数部分 */
    bits |= ((uint32)rest << (23 - exp));

    return bits;
}

/**
 * @brief 发送 IEEE 754 单精度浮点数二进制响应帧
 * @param[in] bits IEEE 754 单精度浮点 32 位位模式
 * @note 帧格式为 '=' + 4 字节小端浮点数 + CRLF。
 */
static void App_SendFloatBits(uint32 bits)
{
    Uart_SendByte('=');
    Uart_SendByte((uint8)(bits & 0xFF));
    Uart_SendByte((uint8)((bits >> 8) & 0xFF));
    Uart_SendByte((uint8)((bits >> 16) & 0xFF));
    Uart_SendByte((uint8)((bits >> 24) & 0xFF));
    Uart_SendStr("\r\n");
}

/**
 * @brief 将 16 位数值转换为浮点格式并发送二进制响应帧
 * @param[in] value 待返回的 16 位无符号数值
 */
static void App_SendFloatFrame(uint16 value)
{
    App_SendFloatBits(App_U16ToFloatBits(value));
}

/**
 * @brief 发送统一异常响应帧
 * @note 异常帧为 3D FF FF FF FF 0D 0A。
 */
static void App_SendErr(void)
{
    App_SendFloatBits(0xFFFFFFFFUL);
}

/**
 * @brief 检查命令固定格式、AA/FF 数字字段及命令长度
 * @param[in] line 以 '\0' 结尾的命令字符串
 * @return 1=固定格式正确，0=格式错误
 * @note 不检查末尾 PPP 三位参数内容，需要时由 App_CheckParamDigits() 检查。
 */
static bit App_CheckFrameFormat(char *line)
{
    uint8 i;

    if (line == 0) return 0;
    for (i = 0; i < APP_CMD_LEN; i++)
    {
        if (line[i] == 0) return 0;
    }
    if (line[APP_CMD_LEN] != 0) return 0;

    if (line[0] != 'M') return 0;
    if (line[1] != 'T') return 0;
    if (line[2] != 'F') return 0;
    if (line[3] != ':') return 0;
    if (!App_IsDigit(line[4])) return 0;
    if (!App_IsDigit(line[5])) return 0;
    if (line[6] != '-') return 0;
    if (line[9] != '-') return 0;
    return 1;
}

/**
 * @brief 检查命令 PPP 参数字段是否全部为数字
 * @param[in] line 以 '\0' 结尾的完整命令字符串
 * @return 1=PPP 三位均为数字，0=至少一位不是数字
 */
static bit App_CheckParamDigits(char *line)
{
    if (!App_IsDigit(line[10])) return 0;
    if (!App_IsDigit(line[11])) return 0;
    if (!App_IsDigit(line[12])) return 0;
    return 1;
}

/**
 * @brief 将两个 ASCII 十进制数字字符转换为两位数值
 * @param[in] high 十位数字字符
 * @param[in] low 个位数字字符
 * @return 转换后的数值，范围为 0~99
 */
static uint8 App_ParseCode(char high, char low)
{
    return (uint8)((high - '0') * 10 + (low - '0'));
}

/**
 * @brief 判断 PPP 参数是否等于指定三个字符
 * @param[in] line 以 '\0' 结尾的完整命令字符串
 * @param[in] p0 PPP 参数百位字符
 * @param[in] p1 PPP 参数十位字符
 * @param[in] p2 PPP 参数个位字符
 * @return 1=PPP 参数完全匹配，0=不匹配
 */
static bit App_ParamIs(char *line, char p0, char p1, char p2)
{
    return ((line[10] == p0) && (line[11] == p1) && (line[12] == p2));
}

/**
 * @brief 将 AA 传感器通道号转换为 I2C 总线枚举
 * @param[in] aa 传感器通道号，范围为 1~4
 * @return 对应的 I2C_Bus_t 枚举值
 * @note 调用前需先检查 aa 是否位于 1~APP_CH_MAX。
 */
static I2C_Bus_t App_AA2Bus(uint8 aa)
{
    return (I2C_Bus_t)(aa - 1);
}

/**
 * @brief 读取指定总线上的 TCS3400 ID
 * @param[in] bus 目标 I2C 总线
 * @param[out] id 返回读取到的 ID 寄存器值
 * @return 1=读取成功，0=参数无效或通信失败
 */
static bit App_ReadID(I2C_Bus_t bus, uint8 *id)
{
    if (id == 0)
    {
        return 0;
    }

    return TCS3400_ReadID(bus, id);
}

/**
 * @brief 解析增益设置命令中的 PPP 参数
 * @param[in] line 以 '\0' 结尾的完整命令字符串
 * @param[out] gain_reg 返回 TCS3400 CONTROL 寄存器增益编码
 * @param[out] gain_value 返回实际增益倍数 1、4、16 或 64
 * @return 1=参数有效且解析成功，0=参数无效或输出指针为空
 */
static bit App_ParseGainParam(char *line, uint8 *gain_reg, uint16 *gain_value)
{
    if ((gain_reg == 0) || (gain_value == 0))
    {
        return 0;
    }

    if (App_ParamIs(line, '0', '0', '1'))
    {
        *gain_reg = TCS3400_GAIN_1X;
        *gain_value = 1;
        return 1;
    }

    if (App_ParamIs(line, '0', '0', '4'))
    {
        *gain_reg = TCS3400_GAIN_4X;
        *gain_value = 4;
        return 1;
    }

    if (App_ParamIs(line, '0', '1', '6'))
    {
        *gain_reg = TCS3400_GAIN_16X;
        *gain_value = 16;
        return 1;
    }

    if (App_ParamIs(line, '0', '6', '4'))
    {
        *gain_reg = TCS3400_GAIN_64X;
        *gain_value = 64;
        return 1;
    }

    return 0;
}

/**
 * @brief 将 CONTROL 寄存器增益编码转换为实际增益倍数
 * @param[in] value CONTROL 寄存器值
 * @return 实际增益倍数 1、4、16 或 64，编码无效时返回 0
 */
static uint16 App_GainRegToValue(uint8 value)
{
    switch (value & 0x03)
    {
        case TCS3400_GAIN_1X:  return 1;
        case TCS3400_GAIN_4X:  return 4;
        case TCS3400_GAIN_16X: return 16;
        case TCS3400_GAIN_64X: return 64;
        default:               return 0;
    }
}

/**
 * @brief 将 ATIME 寄存器值换算为积分时间
 * @param[in] atime ATIME 寄存器值
 * @return 换算后的积分时间，单位 ms
 * @note 按每积分周期约 2.78 ms 计算并四舍五入到整数毫秒。
 */
static uint16 App_ATimeToMs(uint8 atime)
{
    uint16 cycles;

    /* ATIME=0x00 时对应 256 个积分周期 */
    cycles = (uint16)(256 - atime);
    if (cycles == 0)
    {
        cycles = 256;
    }

    /* integration_ms = cycles * 2.78 ms */
    return (uint16)((((uint32)cycles * 278UL) + 99UL) / 100UL);
}

/**
 * @brief 设置指定通道的积分时间并更新应用层记录
 * @param[in] bus 目标 I2C 总线
 * @param[in] time_ms 目标积分时间，单位 ms
 * @note 输入值会被限制在 3~712 ms，当前函数本身未被完整命令处理流程调用。
 */
static void App_SetIntegrationTime(I2C_Bus_t bus, uint16 time_ms)
{
    if (time_ms < 3) time_ms = 3;
    if (time_ms > 712) time_ms = 712;

    TCS3400_SetIntegrationTime(bus, time_ms);
    g_sample_time_ms[(uint8)bus] = time_ms;
}

/**
 * @brief 启动一条由上位机命令触发的非阻塞采样流程
 * @param[in] bus 目标 I2C 总线
 * @param[in] channel 待读取的数据通道
 * @param[in] time_ms 本次积分时间，单位 ms
 * @return 1=采样启动成功，0=参数错误或配置失败
 * @note 输入积分时间会被限制在 3~712 ms。
 */
static bit App_StartCommandSample(I2C_Bus_t bus,
                                  AppChannel_t channel,
                                  uint16 time_ms)
{
    uint32 now;

    if (time_ms < 3) time_ms = 3;
    if (time_ms > 712) time_ms = 712;

    if (!TCS3400_SetIntegrationTime(bus, time_ms))
        return 0;

    if (channel == APP_CHANNEL_IR)
    {
        if (!TCS3400_SetIRMapEnable(bus, 1))
            return 0;
    }
    else
    {
        if (!TCS3400_SetIRMapEnable(bus, 0))
            return 0;
    }

    if (!TCS3400_RestartIntegration(bus))
        return 0;

    /* RestartIntegration 会再次清除 IR 映射，需防止 ENABLE 写入覆盖 IR 位 */
    if (channel == APP_CHANNEL_IR)
    {
        if (!TCS3400_SetIRMapEnable(bus, 1))
            return 0;
    }

    now = Timer0_GetMs();

    g_pending_bus = bus;
    g_pending_channel = channel;
    g_reply_pending = 1;

    g_sample_due_ms = now + TCS3400_GetFrameWaitMs(bus);
    g_sample_deadline_ms = g_sample_due_ms + APP_REPLY_EXTRA_TIMEOUT_MS;

    if (channel == APP_CHANNEL_IR)
        g_sample_state = APP_SAMPLE_WAIT_IR;
    else
        g_sample_state = APP_SAMPLE_WAIT_RGBC;

    return 1;
}

/**
 * @brief 清除当前等待中的采样流程状态
 */
static void App_ClearPendingState(void)
{
    g_reply_pending = 0;
    g_pending_channel = APP_CHANNEL_NONE;
    g_sample_state = APP_SAMPLE_IDLE;
}

/**
 * @brief 中止当前采样流程并恢复普通 Clear 模式
 * @note 若采样流程处于 IR 相关状态时，会关闭 IR 映射并重新启动积分。
 */
static void App_AbortPendingSample(void)
{
    if ((g_sample_state == APP_SAMPLE_WAIT_IR) ||
        (g_sample_state == APP_SAMPLE_WAIT_CLEAR_RECOVERY))
    {
        TCS3400_SetIRMapEnable(g_pending_bus, 0);
        TCS3400_RestartIntegration(g_pending_bus);
    }
    App_ClearPendingState();
}

/**
 * @brief 处理等待回复的采样流程状态机
 * @note 负责轮询 AVALID、通信错误、命令超时，并发送正常或异常响应。
 */
static void App_PendingTask(void)
{
    uint32 now;
    uint8 valid;
    uint16 r, g, b, c;
    uint16 value;

    if (!g_reply_pending)
        return;
    if (!g_sensor_inited)
        return;

    now = Timer0_GetMs();

    if (App_TimeReached(now, g_sample_deadline_ms))
    {
        App_SendErr();
        App_AbortPendingSample();
        return;
    }

    if (!App_TimeReached(now, g_sample_due_ms))
        return;

    /*
     * IR 已经在上一状态成功读取，
     * 当前状态只等待普通 Clear 恢复时间。
     * 等够一帧后直接返回缓存 IR，不再检查第二帧 AVALID。
     */
    if (g_sample_state == APP_SAMPLE_WAIT_CLEAR_RECOVERY)
    {
        App_SendFloatFrame(g_pending_ir_value);
        App_ClearPendingState();
        return;
    }

    /* 普通 RGBC 帧和 IR 第一帧都需要检查 AVALID */
    if (!TCS3400_GetDataValid(g_pending_bus, &valid))
    {
        App_SendErr();
        App_AbortPendingSample();
        return;
    }

    if (!valid)
    {
        g_sample_due_ms = now + 5;
        return;
    }

    switch (g_sample_state)
    {
        case APP_SAMPLE_WAIT_RGBC:
            if (!TCS3400_ReadRGBC_NoWait(g_pending_bus, &r, &g, &b, &c))
            {
                App_SendErr();
                App_AbortPendingSample();
                return;
            }

            switch (g_pending_channel)
            {
                case APP_CHANNEL_R:     value = r; break;
                case APP_CHANNEL_G:     value = g; break;
                case APP_CHANNEL_B:     value = b; break;
                case APP_CHANNEL_CLEAR: value = c; break;
                default:
                    App_SendErr();
                    App_AbortPendingSample();
                    return;
            }

            App_SendFloatFrame(value);
            App_ClearPendingState();
            break;

        case APP_SAMPLE_WAIT_IR:
            /* IR 映射开启时，CDATAL/CDATAH 返回 IR 原始数据 */
            if (!TCS3400_ReadClearPort_NoWait(g_pending_bus, &g_pending_ir_value))
            {
                App_SendErr();
                App_AbortPendingSample();
                return;
            }

            /* 关闭 IR 映射，恢复普通 Clear */
            if (!TCS3400_SetIRMapEnable(g_pending_bus, 0))
            {
                App_SendErr();
                App_AbortPendingSample();
                return;
            }

            /* 启动一帧普通 Clear 恢复积分 */
            if (!TCS3400_RestartIntegration(g_pending_bus))
            {
                App_SendErr();
                App_AbortPendingSample();
                return;
            }

            now = Timer0_GetMs();
            g_sample_due_ms = now + TCS3400_GetFrameWaitMs(g_pending_bus);
            g_sample_deadline_ms = g_sample_due_ms + APP_REPLY_EXTRA_TIMEOUT_MS;
            g_sample_state = APP_SAMPLE_WAIT_CLEAR_RECOVERY;
            break;

        default:
            App_SendErr();
            App_AbortPendingSample();
            break;
    }
}

/**
 * @brief 从 UART 接收缓冲区取字节并组装完整命令
 * @note 同时支持真实 CRLF 和字面量字符串 "\\r\\n" 作为行结束符
 */
static void App_UartTask(void)
{
    static char line[APP_LINE_MAX];
    static uint8 index = 0;
    static bit overflow = 0;
    static bit got_cr = 0;
    static bit format_error = 0;

    uint8 ch;

    while (Uart_ReadByte(&ch))
    {
        /* 识别字面量字符串 \\r\\n */
        if ((ch == 'n') && (index >= 3))
        {
            if ((line[index - 3] == '\\') &&
                (line[index - 2] == 'r') &&
                (line[index - 1] == '\\'))
            {
                index -= 3;
                if (format_error || overflow || index == 0)
                    App_SendErr();
                else
                {
                    line[index] = 0;
                    TCS3400_App_ProcessLine(line);
                }
                index = 0; overflow = 0; got_cr = 0; format_error = 0;
                continue;
            }
        }

        if (ch == '\r')
        {
            got_cr = 1;
            continue;
        }

        if (ch == '\n')
        {
            if (!got_cr)
            {
                App_SendErr();
                index = 0; overflow = 0; format_error = 0;
                continue;
            }

            if (format_error || overflow || index == 0)
                App_SendErr();
            else
            {
                line[index] = 0;
                TCS3400_App_ProcessLine(line);
            }

            index = 0; overflow = 0; got_cr = 0; format_error = 0;
            continue;
        }

        if (got_cr) { got_cr = 0; format_error = 1; }
        if (overflow) continue;

        if (index < (APP_LINE_MAX - 1))
            line[index++] = (char)ch;
        else
        {
            overflow = 1;
            index = 0;
        }
    }
}

/*==============================================================================
 * 对外开放函数实现
 *============================================================================*/

/**
 * @brief 处理一行完整的上位机命令
 * @param[in] line 以 '\0' 结尾的命令字符串，内容不包含 CRLF
 * @note 命令格式为 MTF:AA-FF-PPP，结果通过 UART 发送。
 */
void TCS3400_App_ProcessLine(char *line)
{
    I2C_Bus_t bus;
    uint8 aa;
    uint8 ff;
    uint8 sensor;
    uint8 channel_code;
    uint8 id;
    uint8 reg_value;
    uint8 gain_reg;
    uint16 gain_value;
    uint16 time_ms;
    AppChannel_t channel;

    if (!App_CheckFrameFormat(line))
    {
        App_SendErr();
        return;
    }

    aa = App_ParseCode(line[4], line[5]);
    sensor = aa / 10;
    channel_code = aa % 10;

    if (g_reply_pending)
    {
        App_SendErr();
        return;
    }

    if ((sensor < 1) || (sensor > APP_CH_MAX) || (channel_code > 5))
    {
        App_SendErr();
        return;
    }

    bus = App_AA2Bus(sensor);

    if (!g_sensor_inited)
    {
        TCS3400_App_Init();
    }

    /* AA 个位 1~5：读取 R、G、B、Clear 或 IR；FF 仅作占位 */
    if ((channel_code >= 1) && (channel_code <= 5))
    {
        if (!App_CheckParamDigits(line))
        {
            App_SendErr();
            return;
        }

        if (!App_ReadID(bus, &id))
        {
            App_SendErr();
            return;
        }

        channel = App_CodeToChannel(channel_code);
        if (channel == APP_CHANNEL_NONE)
        {
            App_SendErr();
            return;
        }

        time_ms = App_ParseTimeMs(line);

        if (!App_StartCommandSample(bus, channel, time_ms))
            App_SendErr();

        return;
    }

    if (!App_IsDigit(line[7]) || !App_IsDigit(line[8]))
    {
        App_SendErr();
        return;
    }

    ff = App_ParseCode(line[7], line[8]);

    /* AA 个位 0，FF=97：读取传感器 ID */
    if (ff == 97)
    {
        if (App_ReadID(bus, &id))
            App_SendFloatFrame((uint16)id);
        else
            App_SendErr();
        return;
    }

    /* FF=98：设置并回读当前增益 */
    if (ff == 98)
    {
        if (!App_ParseGainParam(line, &gain_reg, &gain_value))
        {
            App_SendErr();
            return;
        }

        if (!TCS3400_SetGain(bus, gain_reg))
        {
            App_SendErr();
            return;
        }

        if (!TCS3400_ReadRegValue(bus, TCS3400_CONTROL, &reg_value))
        {
            App_SendErr();
            return;
        }

        gain_value = App_GainRegToValue(reg_value);
        if (gain_value == 0)
        {
            App_SendErr();
            return;
        }

        App_SendFloatFrame(gain_value);
        return;
    }

    /* FF=99：读取当前增益 */
    if (ff == 99)
    {
        if (TCS3400_ReadRegValue(bus, TCS3400_CONTROL, &reg_value))
            App_SendFloatFrame(App_GainRegToValue(reg_value));
        else
            App_SendErr();
        return;
    }

    /* 未定义的 FF 功能码 */
    App_SendErr();
}

/**
 * @brief 初始化全部 4 路 TCS3400 传感器
 * @note 初始化通用 I2C、各路传感器以及应用层相关状态。
 */
void TCS3400_App_Init(void)
{
    uint8 i;

    Sensor_I2C_Init();

    for (i = 0; i < APP_CH_MAX; i++)
    {
        g_sample_time_ms[i] = 178;
        TCS3400_Init((I2C_Bus_t)i);
    }

    g_sample_state = APP_SAMPLE_IDLE;
    g_pending_channel = APP_CHANNEL_NONE;
    g_reply_pending = 0;
    g_sample_due_ms = Timer0_GetMs() + TCS3400_GetFrameWaitMs(SOFT_I2C_BUS_1);

    g_sensor_inited = 1;
}

/**
 * @brief 执行 TCS3400 应用层轮询任务
 * @note 需要在 main() 主循环中持续调用。
 */
void TCS3400_App_Poll(void)
{
    App_UartTask();

    if (g_reply_pending)
        App_PendingTask();
}
