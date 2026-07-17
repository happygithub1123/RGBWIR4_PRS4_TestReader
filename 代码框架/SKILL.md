# TCS3400 环境光测试代码框架

本目录用于记录当前环境光测试固件的代码分层和模块职责，方便后续沿用现有框架扩展四路 TCS3400 或新增同类传感器。

## 当前框架

- `User/main.c`：只负责硬件初始化和主循环调度，不直接解析上位机协议，也不直接读传感器。
- `Control/action_control.c/.h`：负责 X1~X4 输入与 Y1~Y4 输出的联动控制。
- `App/tcs3400_app.c/.h`：负责 `MTF:AA-FF-PPP` 协议解析、命令分发、等待回复管理和非阻塞采样状态机。
- `Hardware/tcs3400_driver.c/.h`：负责 TCS3400 寄存器访问、积分时间换算、增益设置和原始数据读取。
- `System/i2c_bus.c/.h`：当前统一的四路软件 I2C 底层，函数统一为 `Sensor_I2C_*`。
- `System/Uart.c/.h`、`System/Timer0.c/.h`、`System/GPIO.c/.h`、`System/Public_cfg.c/.h`：分别提供 UART1、系统节拍、GPIO 和公共类型支持。
- `App/pressure_sensor_app.c/.h`、`System/Uart2.c/.h`、`Hardware/modbus_uart2.c/.h`：压力传感器通讯框架占位，当前未接入具体压力传感器。

## 设计约束

- `main.c` 保持轻量，只做初始化和轮询。
- UART1 中断只缓存字节，不在中断里解析协议。
- TCS3400 协议解析和采样都放在 `App/tcs3400_app.*` 完成。
- 4 路 TCS3400 共享同一套协议框架，通过 `AA` 十位选择物理总线，通过 `AA` 个位选择 R/G/B/Clear/IR 或特殊命令组。
- 底层 I2C 不复制 4 份驱动，统一使用 `System/i2c_bus.*`。
- UART2/485/Modbus 和压力传感器业务保持独立框架，不参与 UART1/TCS3400 上位机协议。

## 当前调用链

```text
main()
  -> GPIO / UART1 / Timer0 初始化
  -> TCS3400_App_Init()
      -> Sensor_I2C_Init()
      -> 4 路 TCS3400 初始化
  -> Pressure_Sensor_App_Init()
      -> ModbusUart2_Init()
  -> while(1)
      -> XY_Control()
      -> TCS3400_App_Poll()
          -> App_UartTask()
          -> App_PendingTask()
      -> Pressure_Sensor_App_Poll()
```

## 后续扩展建议

- 新增 TCS3400 类命令时优先放在 `App/tcs3400_app.*`。
- 新增压力传感器型号时优先放在 `App/pressure_sensor_app.*`，不要影响 UART1/TCS3400 链路。
- 新增底层通讯能力时保持 System/Hardware/App 分层，避免把协议解析塞进 `main.c`。
