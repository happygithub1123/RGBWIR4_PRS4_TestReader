# RGBWIR4_PRS4_TestReader

四路 TCS3400 RGBWIR 环境光测试与压力传感器通讯框架工程。

本项目面向 STC8H8K64U 单片机测试治具，当前核心功能是通过 UART1 接收上位机命令，读取 4 路 TCS3400 的 R/G/B/W/Clear/IR 通道数据，并以固定二进制 Float 帧返回。压力传感器相关的 UART2、RS485、Modbus 通讯层已经预留框架，后续可按具体压力传感器协议继续补全。

## 功能概览

- 4 路 TCS3400 独立软件 I2C 总线读取
- 支持 R、G、B、W/Clear、IR 通道读取
- 支持 TCS3400 ID 读取、增益设置、当前增益查询
- UART1 上位机一问一答协议
- 非阻塞串口接收、命令解析和采样状态机
- UART2/RS485/Modbus 压力传感器通讯框架
- X/Y 输入输出联动控制框架
- Keil C51 工程文件与构建产物保留

## 硬件与通讯参数

| 项目 | 参数 |
| --- | --- |
| MCU | STC8H8K64U |
| 环境光传感器 | TCS3400 x 4 |
| 上位机串口 | UART1 |
| UART1 引脚 | RXD=P3.0, TXD=P3.1 |
| 波特率 | 115200 bps |
| 数据格式 | 8N1 |
| 命令结束符 | `\r\n` |
| 压力传感器预留接口 | UART2 / RS485 / Modbus |

## 四路 TCS3400 总线映射

| 传感器编号 | 软件 I2C 总线 | SCL | SDA |
| --- | --- | --- | --- |
| 1 | `SENSOR_I2C_BUS_1` | P2.5 | P2.6 |
| 2 | `SENSOR_I2C_BUS_2` | P4.5 | P4.6 |
| 3 | `SENSOR_I2C_BUS_3` | P0.1 | P0.2 |
| 4 | `SENSOR_I2C_BUS_4` | P0.4 | P5.2 |

TCS3400 的 7-bit I2C 地址固定为 `0x39`。

## 上位机协议

命令正文固定 13 个 ASCII 字符，后接 `\r\n`：

```text
MTF:AA-FF-PPP\r\n
```

字段说明：

| 字段 | 含义 |
| --- | --- |
| `MTF:` | 固定命令头 |
| `AA` | 两位通道编码，十位为传感器编号，个位为通道或特殊命令组 |
| `FF` | 通道读取时为占位；特殊命令时为 `97/98/99` |
| `PPP` | 通道读取时为积分时间；设置增益时为增益值 |
| `\r\n` | 命令结束符 |

`AA` 编码规则：

```text
sensor  = AA / 10
channel = AA % 10
```

| `AA` 个位 | 含义 |
| --- | --- |
| `0` | 特殊命令组 |
| `1` | R 通道 |
| `2` | G 通道 |
| `3` | B 通道 |
| `4` | W / Clear 通道 |
| `5` | IR 通道 |

## 命令示例

| 命令 | 功能 |
| --- | --- |
| `MTF:11-FF-178\r\n` | 读取第 1 路 R 通道，积分时间 178 ms |
| `MTF:12-AA-178\r\n` | 读取第 1 路 G 通道，积分时间 178 ms |
| `MTF:14-88-178\r\n` | 读取第 1 路 W/Clear 通道，积分时间 178 ms |
| `MTF:25-00-300\r\n` | 读取第 2 路 IR 通道，积分时间 300 ms |
| `MTF:10-97-PPP\r\n` | 读取第 1 路 TCS3400 ID |
| `MTF:30-98-016\r\n` | 设置第 3 路 TCS3400 增益为 16x |
| `MTF:40-99-PPP\r\n` | 查询第 4 路 TCS3400 当前增益 |

通道读取积分时间建议范围为 `003~712 ms`，常用默认值为 `178 ms`。

## 返回帧

正常返回固定 7 字节二进制帧：

```text
3D B0 B1 B2 B3 0D 0A
```

- `3D`：ASCII 字符 `=`
- `B0~B3`：IEEE 754 单精度 Float，小端序
- `0D 0A`：`\r\n`

异常统一返回：

```text
3D FF FF FF FF 0D 0A
```

## 目录结构

```text
.
├── 程序/TCS3400 LZ808-H Project/
│   ├── App/          # TCS3400 应用层与压力传感器应用层
│   ├── Control/      # X/Y 输入输出控制
│   ├── Hardware/     # TCS3400 驱动与 Modbus UART2 适配层
│   ├── System/       # GPIO、UART、Timer、软件 I2C 等系统层
│   ├── User/         # main.c
│   ├── Listings/     # Keil 编译列表文件
│   ├── Objects/      # Keil 编译输出
│   └── Project.uvproj
├── TCS3400和LZ808-H资料参考/   # 器件资料
├── STC8H8K64U_Codex_Register_Reference.md
├── 压力传感器及环境光的通信协议设计与实现.md
├── 环境光测试.md
├── 软件流程图.md
└── 软件逻辑.md
```

## 主要源码模块

| 模块 | 说明 |
| --- | --- |
| `User/main.c` | 初始化 GPIO、UART、Timer、TCS3400 和压力传感器任务 |
| `App/tcs3400_app.*` | 上位机协议解析、回复管理、TCS3400 采样状态机 |
| `Hardware/tcs3400_driver.*` | TCS3400 寄存器读写、积分时间、增益和原始数据读取 |
| `System/i2c_bus.*` | 4 路软件 I2C 总线 |
| `System/Uart.*` | UART1 收发与环形缓冲 |
| `System/Uart2.*` | UART2 收发与环形缓冲 |
| `Hardware/modbus_uart2.*` | UART2/RS485/Modbus 通讯框架 |
| `App/pressure_sensor_app.*` | 压力传感器业务层预留入口 |
| `Control/action_control.*` | X 输入检测与 Y 输出控制 |

## 编译说明

1. 使用 Keil C51 打开 `程序/TCS3400 LZ808-H Project/Project.uvproj`。
2. 确认 STC8H8K64U 相关头文件、启动文件和编译器环境配置正确。
3. 构建工程，生成 HEX 文件。
4. 烧录后通过 UART1 按 `115200 8N1` 连接上位机测试。

## 测试建议

1. 发送 `MTF:10-97-PPP\r\n`，验证 TCS3400 ID 返回。
2. 发送 `MTF:11-FF-178\r\n` 到 `MTF:15-FF-178\r\n`，验证第 1 路 RGBWIR 五个通道。
3. 发送 `MTF:24-00-178\r\n`、`MTF:45-88-178\r\n`，验证多路传感器读取。
4. 发送 `MTF:10-98-064\r\n` 后再发送 `MTF:10-99-PPP\r\n`，验证增益设置和查询。
5. 测试错误命令、错误 `AA`、错误特殊命令 `FF`、参数非数字等异常输入，应返回统一异常帧。

## 当前状态

- TCS3400 四路环境光测试协议为当前主要功能。
- 压力传感器接口已搭建 UART2/RS485/Modbus 框架，尚未绑定具体寄存器协议。
- 仓库中保留了 Keil `Listings/`、`Objects/` 等构建产物，便于追溯当前固件构建状态。
