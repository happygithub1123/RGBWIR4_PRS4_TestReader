# STC8H8K64U 核心寄存器速查手册（Codex 精简版）

> 当前工程统一约定：UART1/UART2 环形接收缓存为 32 字节，实际可用 31 字节；主板严格一问一答时容量满足当前项目需求。


> 来源：STC8H 系列技术手册，资料日期 2025-07-14。  
> 目标芯片：**STC8H8K64U**，重点面向 Keil C51 / 8051 C 工程。  
> 本文只保留代码开发时常查的寄存器、位定义、配置顺序、计算公式和注意事项；删除产品宣传、封装尺寸、烧录器操作及大量重复范例。

---

## 1. 使用范围与默认约定

本文默认：

- MCU：STC8H8K64U。
- 编译器：Keil C51。
- 常用类型：`uint8`、`uint16`、`uint32`。
- 用户常用系统时钟：`22.1184MHz`。
- 写驱动时优先使用芯片官方头文件中的寄存器名称。
- 不同芯片子系列、封装和芯片版本的寄存器或引脚可能不同，移植前必须重新核对型号。

Codex 修改代码时应遵守：

1. 不凭普通 8051 经验猜测 STC8H 寄存器。
2. 修改寄存器时尽量使用按位操作，避免覆盖同一寄存器中的其他功能位。
3. 中断标志一般需要软件清零，不能默认硬件自动清除。
4. XFR 寄存器访问前确认 `P_SW2.EAXFR=1`。
5. GPIO 复用前先检查 `P_SW1`、`P_SW2` 或对应的 XFR 引脚选择寄存器。
6. 定时器、串口和 PWM 配置时，先停止模块，再写参数，最后清标志并启动。

---

## 2. SFR 与 XFR 访问规则

### 2.1 SFR

SFR 是标准 8051 特殊功能寄存器，通常直接通过官方头文件访问：

```c
P1M0 = 0x01;
P1M1 = 0x00;
```

只有地址能被 8 整除的部分 SFR 支持 `sbit` 位寻址。不要对任意 SFR 位强行使用 `sbit`。

### 2.2 XFR

STC8H 扩展寄存器位于 XDATA 地址空间，主要范围为：

```text
0xFA00 ~ 0xFFFF
```

访问 XFR 前必须使能：

```c
P_SW2 |= 0x80;       /* EAXFR=1，允许访问扩展寄存器。 */
```

典型声明方式：

```c
#define CLKSEL  (*(volatile unsigned char xdata *)0xFE00)
#define CLKDIV  (*(volatile unsigned char xdata *)0xFE01)
```

注意：

- XFR 与外部 XRAM 的高地址区域重叠。
- 未使用外部 64KB XRAM 时，通常可以一直保持 `EAXFR=1`。
- 使用外部 XRAM 且需要访问 `0xFA00~0xFFFF` 时，必须临时切换 `EAXFR`。

---

## 3. STC8H8K64U SFR 地址总表

| 地址 | 寄存器 | 地址 | 寄存器 | 地址 | 寄存器 | 地址 | 寄存器 |
|---:|---|---:|---|---:|---|---:|---|
| `80H` | P0 | `81H` | SP | `82H` | DPL | `83H` | DPH |
| `84H` | S4CON | `85H` | S4BUF | `87H` | PCON | `88H` | TCON |
| `89H` | TMOD | `8AH` | TL0 | `8BH` | TL1 | `8CH` | TH0 |
| `8DH` | TH1 | `8EH` | AUXR | `8FH` | INTCLKO | `90H` | P1 |
| `91H` | P1M1 | `92H` | P1M0 | `93H` | P0M1 | `94H` | P0M0 |
| `95H` | P2M1 | `96H` | P2M0 | `98H` | SCON | `99H` | SBUF |
| `9AH` | S2CON | `9BH` | S2BUF | `9DH` | IRCBAND | `9FH` | IRTRIM |
| `A0H` | P2 | `A1H` | BUS_SPEED | `A2H` | P_SW1 | `A8H` | IE |
| `A9H` | SADDR | `AAH` | WKTCL | `ABH` | WKTCH | `ACH` | S3CON |
| `ADH` | S3BUF | `AEH` | TA | `AFH` | IE2 | `B0H` | P3 |
| `B1H` | P3M1 | `B2H` | P3M0 | `B3H` | P4M1 | `B4H` | P4M0 |
| `B5H` | IP2 | `B6H` | IP2H | `B7H` | IPH | `B8H` | IP |
| `B9H` | SADEN | `BAH` | P_SW2 | `BCH` | ADC_CONTR | `BDH` | ADC_RES |
| `BEH` | ADC_RESL | `C0H` | P4 | `C1H` | WDT_CONTR | `C2H` | IAP_DATA |
| `C3H` | IAP_ADDRH | `C4H` | IAP_ADDRL | `C5H` | IAP_CMD | `C6H` | IAP_TRIG |
| `C7H` | IAP_CONTR | `C8H` | P5 | `C9H` | P5M1 | `CAH` | P5M0 |
| `CBH` | P6M1 | `CCH` | P6M0 | `CDH` | SPSTAT | `CEH` | SPCTL |
| `CFH` | SPDAT | `D0H` | PSW | `D1H` | T4T3M | `D2H` | T4H |
| `D3H` | T4L | `D4H` | T3H | `D5H` | T3L | `D6H` | T2H |
| `D7H` | T2L | `DCH` | USBCLK | `DEH` | ADCCFG | `DFH` | IP3 |
| `E0H` | ACC | `E1H` | P7M1 | `E2H` | P7M0 | `E3H` | DPS |
| `E4H` | DPL1 | `E5H` | DPH1 | `E6H` | CMPCR1 | `E7H` | CMPCR2 |
| `E8H` | P6 | `ECH` | USBDAT | `EEH` | IP3H | `EFH` | AUXINTIF |
| `F0H` | B | `F4H` | USBCON | `F5H` | IAP_TPS | `F8H` | P7 |
| `FBH` | USBADR | `FFH` | RSTCFG |  |  |  |  |

---

## 4. 常用 XFR 地址索引

### 4.1 时钟与复位

| 地址 | 寄存器 | 作用 |
|---:|---|---|
| `FE00H` | CLKSEL | 主时钟源选择 |
| `FE01H` | CLKDIV | 系统时钟分频 |
| `FE02H` | HIRCCR | 内部高速 IRC 控制 |
| `FE03H` | XOSCCR | 外部晶振/外部时钟控制 |
| `FE04H` | IRC32KCR | 内部低速 32K IRC 控制 |
| `FE05H` | MCLKOCR | 主时钟输出控制 |
| `FE06H` | IRCDB | IRC 稳定等待时间 |
| `FE07H` | IRC48MCR | 48MHz IRC 控制 |
| `FE98H` | SPFUNC | 特殊功能控制 |
| `FE99H` | RSTFLAG | 复位来源标志 |

### 4.2 GPIO 扩展控制

| 地址范围 | 寄存器 | 作用 |
|---:|---|---|
| `FE10H~FE17H` | P0PU~P7PU | 内部上拉使能 |
| `FE18H~FE1FH` | P0NCS~P7NCS | 施密特输入控制 |
| `FE20H~FE27H` | P0SR~P7SR | 输出沿速度控制 |
| `FE28H~FE2FH` | P0DR~P7DR | 输出驱动能力控制 |
| `FE30H` | P0IE | P0 数字输入使能 |
| `FE31H` | P1IE | P1 数字输入使能 |

> STC8H8K64U 不同版本的数字输入使能寄存器支持范围可能不同。以实际芯片版本和官方头文件为准。

### 4.3 I2C

| 地址 | 寄存器 | 作用 |
|---:|---|---|
| `FE80H` | I2CCFG | I2C 使能、主从模式、速度 |
| `FE81H` | I2CMSCR | 主机命令控制 |
| `FE82H` | I2CMSST | 主机状态 |
| `FE83H` | I2CSLCR | 从机控制 |
| `FE84H` | I2CSLST | 从机状态 |
| `FE85H` | I2CSLADR | 从机地址 |
| `FE86H` | I2CTXD | 发送数据 |
| `FE87H` | I2CRXD | 接收数据 |
| `FE88H` | I2CMSAUX | 主机辅助控制 |

### 4.4 定时器与 ADC

| 地址 | 寄存器 | 作用 |
|---:|---|---|
| `FEA2H` | TM2PS | 定时器2预分频 |
| `FEA3H` | TM3PS | 定时器3预分频 |
| `FEA4H` | TM4PS | 定时器4预分频 |
| `FEA8H` | ADCTIM | ADC 采样时序 |
| `FEACH` | T3T4PIN / T3T4PS | T3/T4 引脚选择 |
| `FEADH` | ADCEXCFG | ADC 扩展配置 |
| `FEAEH` | CMPEXCFG | 比较器扩展配置 |

### 4.5 高级 PWM

| 地址范围 | 模块 |
|---:|---|
| `FEC0H~FEDFH` | PWMA，通道1~4 |
| `FEE0H~FEFFH` | PWMB，通道5~8 |
| `FEB0H~FEB3H` | PWMA 外部触发、输出使能、引脚选择、辅助控制 |
| `FEB4H~FEB7H` | PWMB 外部触发、输出使能、引脚选择、辅助控制 |

### 4.6 DMA、MDU、CRC

| 地址范围 | 模块 |
|---:|---|
| `FA00H` 起 | M2M DMA |
| `FA10H` 起 | ADC DMA |
| `FA20H` 起 | SPI DMA |
| `FA30H/FA38H` 起 | UART1 发送/接收 DMA |
| `FA40H/FA48H` 起 | UART2 发送/接收 DMA |
| `FA50H/FA58H` 起 | UART3 发送/接收 DMA |
| `FA60H/FA68H` 起 | UART4 发送/接收 DMA |
| `FA70H` 起 | LCM DMA |
| `FCF0H~FCF7H` | MDU16 硬件乘除法 |
| `FB30H~FB33H` | CRC16-Modbus |

---

## 5. 外设引脚切换

### 5.1 P_SW1（A2H）

```text
B7 B6：S1_S，串口1引脚组
B3 B2：SPI_S，SPI引脚组
```

#### UART1 引脚组

| S1_S | RXD | TXD |
|---:|---|---|
| `00` | P3.0 | P3.1 |
| `01` | P3.6 | P3.7 |
| `10` | P1.6 | P1.7 |
| `11` | P4.3 | P4.4 |

设置示例：

```c
P_SW1 &= ~0xC0;       /* UART1 选择 P3.0/P3.1。 */
```

#### SPI 引脚组

| SPI_S | SS | MOSI | MISO | SCLK |
|---:|---|---|---|---|
| `00` | P1.2/P5.4 | P1.3 | P1.4 | P1.5 |
| `01` | P2.2 | P2.3 | P2.4 | P2.5 |
| `10` | P5.4 | P4.0 | P4.1 | P4.3 |
| `11` | P3.5 | P3.4 | P3.3 | P3.2 |

### 5.2 P_SW2（BAH）

```text
B7    EAXFR：允许访问 XFR
B5:B4 I2C_S：I2C 引脚组
B3    CMPO_S：比较器输出引脚
B2    S4_S：串口4引脚组
B1    S3_S：串口3引脚组
B0    S2_S：串口2引脚组
```

#### UART2/3/4

| 外设 | 选择0 | 选择1 |
|---|---|---|
| UART2 | RXD2=P1.0，TXD2=P1.1 | RXD2=P4.6，TXD2=P4.7 |
| UART3 | RXD3=P0.0，TXD3=P0.1 | RXD3=P5.0，TXD3=P5.1 |
| UART4 | RXD4=P0.2，TXD4=P0.3 | RXD4=P5.2，TXD4=P5.3 |

#### I2C

| I2C_S | SCL | SDA |
|---:|---|---|
| `00` | P1.5 | P1.4 |
| `01` | P2.5 | P2.4 |
| `10` | P7.7 | P7.6 |
| `11` | P3.2 | P3.3 |

#### 比较器输出

| CMPO_S | 输出引脚 |
|---:|---|
| `0` | P3.4 |
| `1` | P4.1 |

### 5.3 T3/T4 引脚切换

`T3T4PIN` 位0：

| 值 | T3 | T3CLKO | T4 | T4CLKO |
|---:|---|---|---|---|
| `0` | P0.4 | P0.5 | P0.6 | P0.7 |
| `1` | P0.0 | P0.1 | P0.2 | P0.3 |

### 5.4 PWM 引脚切换

`PWMA_PS`：

| 通道 | `00` | `01` | `10` | `11` |
|---|---|---|---|---|
| C1/C1N | P1.0/P1.1 | P2.0/P2.1 | P6.0/P6.1 | 保留 |
| C2/C2N | P1.2或P5.4/P1.3 | P2.2/P2.3 | P6.2/P6.3 | 保留 |
| C3/C3N | P1.4/P1.5 | P2.4/P2.5 | P6.4/P6.5 | 保留 |
| C4/C4N | P1.6/P1.7 | P2.6/P2.7 | P6.6/P6.7 | P3.4/P3.3 |

`PWMB_PS`：

| 通道 | `00` | `01` | `10` | `11` |
|---|---|---|---|---|
| C5 | P2.0 | P1.7 | P0.0 | P7.4 |
| C6 | P2.1 | P5.4 | P0.1 | P7.5 |
| C7 | P2.2 | P3.3 | P0.2 | P7.6 |
| C8 | P2.3 | P3.4 | P0.3 | P7.7 |

---

## 6. 时钟系统

### 6.1 关键寄存器

| 寄存器 | 关键位 | 作用 |
|---|---|---|
| CLKSEL | CKMS、HSIOCK、MCK2SEL、MCKSEL | 主时钟源选择 |
| CLKDIV | CLKDIV[7:0] | SYSCLK 分频 |
| HIRCCR | ENHIRC、HIRCST | 高速 IRC 使能/稳定标志 |
| XOSCCR | ENXOSC、XITYPE、GAIN、XCFILTER、XOSCST | 外部晶振控制 |
| MCLKOCR | MCLKO_S、MCLKO_DIV | 时钟输出 |

### 6.2 CLKSEL 主时钟选择

`MCK2SEL` 与 `MCKSEL` 联合决定 MCLK：

| MCK2SEL | MCKSEL | MCLK |
|---:|---:|---|
| `00` | `00` | 内部高速 IRC |
| `00` | `01` | 外部晶振/外部时钟 |
| `00` | `10` | 外部 32K 时钟 |
| `00` | `11` | 内部 32K IRC |
| `01` | 任意 | PLL/2 |
| `10` | 任意 | PLL/4 |

系统时钟：

```text
SYSCLK = MCLK / CLKDIV
```

`CLKDIV=0` 或 `1` 均表示不分频，`2` 表示二分频，以此类推。

### 6.3 外部晶振启用顺序

```c
P_SW2 |= 0x80;        /* 允许访问 XFR。 */
XOSCCR = 0xC0;        /* 使能外部晶振；具体增益/滤波按频率调整。 */
while (!(XOSCCR & 0x01));
CLKSEL = (CLKSEL & ~0x03) | 0x01;
```

切换原则：

1. 先使能目标时钟。
2. 等待稳定标志。
3. 再切换系统时钟源。
4. 必要时设置 `CLKDIV`。
5. 不要先关闭当前正在工作的时钟。

### 6.4 主时钟输出

`MCLKOCR`：

- bit7：输出引脚，`0=P5.4`，`1=P1.6`。
- bit6:0：分频值，`0` 关闭输出，`n` 表示输出 `SYSCLK/n`。

---

## 7. 复位、看门狗与低压复位

### 7.1 WDT_CONTR（C1H）

```text
B7 WDT_FLAG：看门狗溢出标志
B5 EN_WDT：看门狗使能
B4 CLR_WDT：写1喂狗
B3 IDL_WDT：空闲模式下是否运行
B2:B0 WDT_PS：预分频
```

看门狗溢出时间：

```text
T_WDT = 12 × 32768 × 2^(WDT_PS+1) / SYSCLK
```

注意：软件使能看门狗后，通常不能通过软件关闭，需重新上电或复位。

喂狗：

```c
WDT_CONTR |= 0x10;
```

### 7.2 软件复位

`IAP_CONTR`：

```text
B7 IAPEN
B6 SWBS：复位后进入用户区/系统 ISP 区
B5 SWRST：写1触发软件复位
B4 CMD_FAIL：IAP 操作失败标志
```

普通软件复位：

```c
IAP_CONTR = 0x20;
```

### 7.3 RSTCFG（FFH）

```text
B6 ENLVR：低压复位使能
B4 P54RST：P5.4 作为复位脚
B1:B0 LVDS：低压复位门槛
```

| LVDS | 典型门槛 |
|---:|---:|
| `00` | 2.0V |
| `01` | 2.4V |
| `10` | 2.7V |
| `11` | 3.0V |

### 7.4 RSTFLAG（FE99H）

| 位 | 标志 |
|---:|---|
| B4 | LVDRSTF，低压复位 |
| B3 | WDTRSTF，看门狗复位 |
| B2 | SWRSTF，软件复位 |
| B1 | ROMOVF，程序地址溢出 |
| B0 | EXRSTF，外部复位 |

向对应位写 `1` 清除标志。

---

## 8. GPIO

### 8.1 GPIO 模式

每组端口通过 `PxM1` 和 `PxM0` 配置：

| PxM1 | PxM0 | 模式 | 常见用途 |
|---:|---:|---|---|
| 0 | 0 | 准双向口 | 普通 8051 兼容输入输出 |
| 0 | 1 | 推挽输出 | LED、控制信号、高速输出 |
| 1 | 0 | 高阻输入 | 数字输入、ADC 输入 |
| 1 | 1 | 开漏输出 | I2C、线与输出 |

单引脚模式配置示例：

```c
/* P1.0 推挽输出。 */
P1M1 &= ~0x01;
P1M0 |=  0x01;

/* P1.1 高阻输入。 */
P1M1 |=  0x02;
P1M0 &= ~0x02;
```

### 8.2 GPIO 扩展属性

| 寄存器 | 位值 | 含义 |
|---|---:|---|
| PxPU | 1 | 使能约 4kΩ 内部上拉 |
| PxNCS | 0 | 使能施密特输入 |
| PxNCS | 1 | 关闭施密特输入 |
| PxSR | 0 | 快速输出沿 |
| PxSR | 1 | 慢速输出沿，降低 EMI |
| PxDR | 0 | 增强驱动能力 |
| PxDR | 1 | 普通驱动能力 |
| PxIE | 1 | 使能数字输入 |
| PxIE | 0 | 关闭数字输入，适合模拟输入/低功耗 |

ADC 引脚建议：

1. GPIO 配置为高阻输入。
2. 对支持 `PxIE` 的端口关闭数字输入。
3. 不要同时使能内部上拉。

### 8.3 GPIO 使用原则

- 输入脚先读端口锁存器置 `1` 或直接配置高阻输入。
- 驱动继电器、MOS 管和高速逻辑时使用推挽输出。
- I2C 必须使用开漏/高阻方式并配置外部上拉。
- 未使用引脚不要悬空，可配置为输出低或带确定电平的输入。

---

## 9. 中断系统

### 9.1 常用中断号

| 中断源 | Keil 中断号 |
|---|---:|
| INT0 | 0 |
| Timer0 | 1 |
| INT1 | 2 |
| Timer1 | 3 |
| UART1 | 4 |
| ADC | 5 |
| LVD | 6 |
| PCA | 7 |
| UART2 | 8 |
| SPI | 9 |
| INT2 | 10 |
| INT3 | 11 |
| Timer2 | 12 |
| INT4 | 16 |
| UART3 | 17 |
| UART4 | 18 |
| Timer3 | 19 |
| Timer4 | 20 |
| Comparator | 21 |
| I2C | 24 |
| USB | 25 |
| PWMA | 26 |
| PWMB | 27 |

中断号大于 31 时，旧版 Keil C51 可能需要 STC 的中断号扩展工具或中断跳转处理。

### 9.2 中断使能

`IE`：

```text
EA ELVD EADC ES ET1 EX1 ET0 EX0
```

`IE2`：

```text
EUSB/ETKSU ET4 ET3 ES4 ES3 ET2 ESPI ES2
```

`INTCLKO`：

```text
B6 EX4
B5 EX3
B4 EX2
B2 T2CLKO
B1 T1CLKO
B0 T0CLKO
```

### 9.3 中断优先级

优先级寄存器成对使用：

- `IP` + `IPH`
- `IP2` + `IP2H`
- `IP3` + `IP3H`

每个中断源由高低两个优先级位组成：

| 高位 | 低位 | 优先级 |
|---:|---:|---|
| 0 | 0 | 0，最低 |
| 0 | 1 | 1 |
| 1 | 0 | 2 |
| 1 | 1 | 3，最高 |

### 9.4 外部中断与定时器标志

`TCON`：

```text
TF1 TR1 TF0 TR0 IE1 IT1 IE0 IT0
```

- `IT0/IT1=1`：下降沿触发。
- `IT0/IT1=0`：低电平触发。
- `IE0/IE1`：外部中断标志。
- `TF0/TF1`：定时器溢出标志。

`AUXINTIF`：

```text
B6 INT4IF
B5 INT3IF
B4 INT2IF
B2 T4IF
B1 T3IF
B0 T2IF
```

T2/T3/T4 中断服务函数内应软件清除对应标志。

### 9.5 全端口 GPIO 中断

XFR 寄存器组：

| 地址范围 | 作用 |
|---:|---|
| `FD00H~FD07H` | P0INTE~P7INTE，中断使能 |
| `FD10H~FD17H` | P0INTF~P7INTF，中断标志 |
| `FD20H~FD27H` | P0IM0~P7IM0，触发模式低位 |
| `FD30H~FD37H` | P0IM1~P7IM1，触发模式高位 |
| `FD40H~FD47H` | P0WKUE~P7WKUE，唤醒使能 |
| `FD60H` | PINIPL，端口中断优先级低位 |
| `FD61H` | PINIPH，端口中断优先级高位 |

端口中断标志需要软件清零。

---

## 10. 定时器0/1

### 10.1 TCON

```text
TF1 TR1 TF0 TR0 IE1 IT1 IE0 IT0
```

### 10.2 TMOD

```text
B7  T1_GATE
B6  T1_C/T
B5:B4 T1_M1:T1_M0
B3  T0_GATE
B2  T0_C/T
B1:B0 T0_M1:T0_M0
```

定时器模式：

| M1:M0 | 模式 |
|---:|---|
| `00` | 16位自动重装载 |
| `01` | 16位非自动重装载 |
| `10` | 8位自动重装载 |
| `11` | T0 为不可屏蔽 16位自动重装载；T1 停止 |

### 10.3 AUXR 相关位

```text
B7 T0x12：1=1T，0=12T
B6 T1x12：1=1T，0=12T
B5 UART_M0x6
B4 T2R
B3 T2_C/T
B2 T2x12
B1 EXTRAM
B0 S1ST2
```

### 10.4 周期计算

16位模式：

```text
T = (65536 - Reload) × Divider / SYSCLK
```

8位自动重装载：

```text
T = (256 - THx) × Divider / SYSCLK
```

其中：

- 1T 模式：`Divider=1`
- 12T 模式：`Divider=12`

重装值：

```text
Reload = 65536 - SYSCLK × T / Divider
```

### 10.5 Timer0 1ms 示例，22.1184MHz，1T

理论计数：

```text
22,118,400 × 0.001 = 22,118.4
Reload ≈ 65536 - 22118 = 43418 = 0xA99A
```

```c
void TM0_Init(void)
{
    AUXR |= 0x80;      /* T0 选择 1T。 */
    TMOD &= 0xF0;      /* T0 模式0，16位自动重装载。 */
    TL0 = 0x9A;
    TH0 = 0xA9;
    TF0 = 0;
    ET0 = 1;
    TR0 = 1;
}
```

由于重装值取整，存在极小误差；高精度任务应累积校正或使用更合适的时钟。

---

## 11. 定时器2/3/4

### 11.1 Timer2

| 寄存器 | 作用 |
|---|---|
| T2H/T2L | 16位当前值与重装值 |
| AUXR.T2R | 启停 |
| AUXR.T2_C/T | 定时/计数选择 |
| AUXR.T2x12 | 1T/12T |
| TM2PS | 预分频，实际除数=`TM2PS+1` |
| AUXINTIF.T2IF | 溢出标志 |

周期：

```text
T2周期 = (65536 - Reload) × (TM2PS + 1) × Divider / SYSCLK
```

### 11.2 Timer3/4

`T4T3M`：

```text
B7 T4R
B6 T4_C/T
B5 T4x12
B4 T4CLKO
B3 T3R
B2 T3_C/T
B1 T3x12
B0 T3CLKO
```

| 定时器 | 计数寄存器 | 预分频 | 中断标志 |
|---|---|---|---|
| T3 | T3H/T3L | TM3PS | AUXINTIF.T3IF |
| T4 | T4H/T4L | TM4PS | AUXINTIF.T4IF |

公式与 Timer2 相同。

### 11.3 中断模板

```c
void Timer2_Isr(void) interrupt 12
{
    AUXINTIF &= ~0x01; /* 清 T2IF。 */
}
```

---

## 12. UART1~UART4

### 12.1 UART1

`SCON`：

```text
SM0/FE SM1 SM2 REN TB8 RB8 TI RI
```

常用模式1：

```c
SCON = 0x50;           /* 8位数据，可变波特率，允许接收。 */
```

`PCON`：

- bit7 `SMOD`：UART1 波特率倍增。
- bit6 `SMOD0`：置1后可通过 `SCON.7` 读取帧错误 FE。

`AUXR.S1ST2`：

- `0`：UART1 使用 Timer1。
- `1`：UART1 使用 Timer2。

### 12.2 UART2/3/4 控制寄存器

`S2CON`：

```text
S2SM0 - S2SM2 S2REN - - S2TI S2RI
```

`S3CON`：

```text
S3SM0 S3ST3 S3SM2 S3REN - - S3TI S3RI
```

- `S3ST3=0`：Timer2 作为波特率发生器。
- `S3ST3=1`：Timer3 作为波特率发生器。

`S4CON`：

```text
S4SM0 S4ST4 S4SM2 S4REN - - S4TI S4RI
```

- `S4ST4=0`：Timer2。
- `S4ST4=1`：Timer4。

### 12.3 Timer2/Timer1 模式0波特率公式

1T：

```text
Baud = SYSCLK / [4 × (65536 - Reload)]
Reload = 65536 - SYSCLK / (4 × Baud)
```

12T：

```text
Baud = SYSCLK / [4 × 12 × (65536 - Reload)]
```

22.1184MHz、1T 常用值：

| 波特率 | Reload | TH/TL |
|---:|---:|---|
| 115200 | `0xFFD0` | FF / D0 |
| 57600 | `0xFFA0` | FF / A0 |
| 38400 | `0xFF70` | FF / 70 |
| 19200 | `0xFEE0` | FE / E0 |
| 9600 | `0xFDC0` | FD / C0 |

### 12.4 UART1 使用 Timer2，115200bps@22.1184MHz

```c
void Uart1_Init(void)
{
    P_SW1 &= ~0xC0;    /* UART1：P3.0/P3.1。 */
    SCON = 0x50;       /* 模式1，允许接收。 */

    AUXR &= ~0x10;     /* 停止 Timer2。 */
    AUXR |= 0x04;      /* Timer2 选择 1T。 */
    AUXR &= ~0x08;     /* Timer2 作为定时器。 */
    AUXR |= 0x01;      /* UART1 使用 Timer2。 */

    T2L = 0xD0;
    T2H = 0xFF;

    RI = 0;
    TI = 0;
    ES = 1;
    AUXR |= 0x10;      /* 启动 Timer2。 */
}
```

### 12.5 UART 中断模板

```c
void Uart1_Isr(void) interrupt 4
{
    uint8 dat;

    if (RI)
    {
        RI = 0;
        dat = SBUF;
        /* 将 dat 放入接收环形缓冲区。 */
    }

    if (TI)
    {
        TI = 0;
        /* 发送下一个字节或释放 busy 标志。 */
    }
}
```

注意：

- `RI` 和 `TI` 必须分别判断。
- 两个标志都需要软件清零。
- 中断中只做收发搬运，协议解析放在主循环。
- 非阻塞发送应使用发送缓冲区和 busy 状态。

---

## 13. SPI

### 13.1 SPSTAT（CDH）

```text
B7 SPIF：传输完成标志
B6 WCOL：写冲突标志
```

两个标志均通过向对应位写 `1` 清除。

```c
SPSTAT = 0xC0;         /* 清 SPIF 和 WCOL。 */
```

### 13.2 SPCTL（CEH）

```text
B7 SSIG：1=忽略 SS 输入
B6 SPEN：SPI 使能
B5 DORD：0=MSB先发，1=LSB先发
B4 MSTR：1=主机，0=从机
B3 CPOL
B2 CPHA
B1:B0 SPR：时钟分频
```

普通 SPI 分频：

| SPR | SCLK |
|---:|---:|
| `00` | SYSCLK/4 |
| `01` | SYSCLK/8 |
| `10` | SYSCLK/16 |
| `11` | SYSCLK/32 |

### 13.3 SPI 四种模式

| 模式 | CPOL | CPHA |
|---:|---:|---:|
| 0 | 0 | 0 |
| 1 | 0 | 1 |
| 2 | 1 | 0 |
| 3 | 1 | 1 |

### 13.4 主机单字节传输

```c
uint8 Spi_Transfer(uint8 dat)
{
    SPDAT = dat;

    while (!(SPSTAT & 0x80));
    SPSTAT = 0x80;     /* 写1清 SPIF。 */

    return SPDAT;
}
```

实际工程应增加超时，防止硬件异常导致死循环。

---

## 14. I2C 硬件控制器

### 14.1 I2CCFG（FE80H）

```text
B7 ENI2C：I2C 使能
B6 MSSL：1=主机，0=从机
B5:B0 MSSPEED：主机速度
```

主机 SCL：

```text
SCL = FOSC / 2 / (MSSPEED × 2 + 4)
```

### 14.2 I2CMSCR（FE81H）主机命令

| MSCMD | 操作 |
|---:|---|
| `0` | 空闲 |
| `1` | 发送 START |
| `2` | 发送一个字节 |
| `3` | 接收 ACK |
| `4` | 接收一个字节 |
| `5` | 发送 ACK/NAK |
| `6` | 发送 STOP |
| `9` | START + 发送数据 + 接收 ACK |
| `A` | 发送数据 + 接收 ACK |
| `B` | 接收数据 + 回 ACK=0 |
| `C` | 接收数据 + 回 NAK=1 |

### 14.3 I2CMSST（FE82H）

```text
B7 MSBUSY：主机忙
B6 MSIF：主机操作完成标志
B1 MSACKI：收到的 ACK
B0 MSACKO：要发送的 ACK/NAK
```

`MSIF` 需要软件清零。

### 14.4 I2CMSAUX（FE88H）

- bit0 `WDTA=1`：写入 `I2CTXD` 时自动触发命令 `A`。

### 14.5 主机操作基本流程

1. GPIO 选择 I2C 引脚组。
2. 配置外部上拉，端口采用开漏/高阻方式。
3. 设置 `I2CCFG` 的主机模式和速度。
4. 写 `I2CTXD`。
5. 写 `I2CMSCR` 发出命令。
6. 等待 `MSIF=1`。
7. 清 `MSIF`。
8. 检查 `MSACKI`。
9. 最后发送 STOP。

不要只等待 `MSBUSY`，应以操作完成标志和 ACK 状态共同判断。

---

## 15. ADC

### 15.1 ADC_CONTR（BCH）

```text
B7 ADC_POWER：ADC 电源
B6 ADC_START：启动转换
B5 ADC_FLAG：转换完成标志
B4 ADC_EPWMT：PWM 触发控制
B3:B0 ADC_CHS：通道选择
```

ADC 上电后建议等待约 1ms 再开始第一次转换。

### 15.2 STC8H8K64U ADC 通道

| 通道 | 输入 |
|---:|---|
| 0 | P1.0 |
| 1 | P1.1 |
| 2 | P5.4 |
| 3 | P1.3 |
| 4 | P1.4 |
| 5 | P1.5 |
| 6 | P1.6 |
| 7 | P1.7 |
| 8 | P0.0 |
| 9 | P0.1 |
| 10 | P0.2 |
| 11 | P0.3 |
| 12 | P0.4 |
| 13 | P0.5 |
| 14 | P0.6 |
| 15 | 内部约 1.19V 参考源 |

### 15.3 ADCCFG（DEH）

```text
B5 RESFMT：结果格式
B3:B0 SPEED：ADC 时钟分频
```

ADC 时钟：

```text
FADC = SYSCLK / 2 / (SPEED + 1)
```

推荐使用右对齐：

```c
ADCCFG |= 0x20;
```

12位结果：

```c
value = ((uint16)(ADC_RES & 0x0F) << 8) | ADC_RESL;
```

### 15.4 ADCTIM（FEA8H）

```text
B7 CSSETUP：建立时间
B6:B5 CSHOLD：保持时间
B4:B0 SMPDUTY：采样时间
```

一次转换时间约为：

```text
建立周期 + 采样周期 + 保持周期 + 12个 ADC 时钟
```

采样时间不能设置过短，信号源阻抗较高时应增加采样周期。

### 15.5 ADC 读取模板

```c
uint16 Adc_Read(uint8 channel)
{
    uint16 value;

    ADC_CONTR = 0x80 | (channel & 0x0F);
    Delay_ms(1);

    ADC_CONTR |= 0x40;
    while (!(ADC_CONTR & 0x20));
    ADC_CONTR &= ~0x20;

    value = ((uint16)(ADC_RES & 0x0F) << 8) | ADC_RESL;
    return value;
}
```

正式工程应：

- ADC 只初始化一次，不要每次读取都重复上电延时。
- 增加转换超时。
- 读取首个通道后可丢弃一次结果，以减小通道切换残留影响。
- 多次采样后平均或中值滤波。

---

## 16. 比较器

### 16.1 CMPCR1（E6H）

```text
B7 CMPEN：比较器使能
B6 CMPIF：中断标志
B5 PIE：上升沿中断使能
B4 NIE：下降沿中断使能
B3 PIS：上升沿标志
B2 NIS：下降沿标志
B1 CMPOE：比较器输出使能
B0 CMPRES：比较结果
```

`CMPIF` 需要软件清零。只有对应边沿中断使能时，边沿才会置位中断标志。

### 16.2 CMPCR2（E7H）

```text
B7 INVCMPO：输出反相
B6 DISFLT：数字滤波关闭
B5:B0 LCDTY：滤波时间
```

启用滤波时，实际滤波宽度约为：

```text
(LCDTY + 2) 个 SYSCLK 周期
```

### 16.3 CMPEXCFG（FEAEH）

```text
B7:B6 CHYS：迟滞 0/10/20/30mV
B2 CMPNS：负端选择
B1:B0 CMPPS：正端选择
```

- `CMPNS=0`：负端 P3.6。
- `CMPNS=1`：负端内部约 1.19V。
- 正端可选 P3.7、P5.0、P5.1 或 ADC 输入通道。

---

## 17. IAP / EEPROM / Data Flash

### 17.1 寄存器

| 寄存器 | 作用 |
|---|---|
| IAP_DATA | 读写数据 |
| IAP_ADDRH/L | 目标地址 |
| IAP_CMD | 操作命令 |
| IAP_TRIG | 触发序列 |
| IAP_CONTR | 总控制、失败标志 |
| IAP_TPS | IAP 时序参数 |

### 17.2 命令

| IAP_CMD | 操作 |
|---:|---|
| `01H` | 读取字节 |
| `02H` | 编程字节 |
| `03H` | 擦除扇区 |

擦除扇区大小：`512字节`。

### 17.3 IAP_TPS

通常设置为系统时钟的 MHz 整数值：

```c
IAP_TPS = 22;          /* 22.1184MHz。 */
```

### 17.4 触发序列

必须连续写：

```c
IAP_TRIG = 0x5A;
IAP_TRIG = 0xA5;
```

触发期间不应被会操作 IAP 的中断打断。

### 17.5 编程规则

- Flash 编程只能把位从 `1` 写成 `0`。
- 需要从 `0` 变为 `1` 时必须先擦除扇区。
- 擦除前先备份同一扇区中仍需保留的数据。
- 操作完成后关闭 IAP，并清地址、命令和数据寄存器。
- 检查 `IAP_CONTR.CMD_FAIL`。
- 不要在没有确认实际程序区和数据区规划时硬编码 EEPROM 上限。

### 17.6 基础操作模板

```c
static void Iap_Trigger(void)
{
    IAP_TRIG = 0x5A;
    IAP_TRIG = 0xA5;
    _nop_();
}

static void Iap_Idle(void)
{
    IAP_CONTR = 0;
    IAP_CMD = 0;
    IAP_TRIG = 0;
    IAP_ADDRH = 0x80;
    IAP_ADDRL = 0;
}
```

---

## 18. 高级 PWM

STC8H8K64U 有两组 16 位高级 PWM：

- PWMA：通道1~4，可带互补输出。
- PWMB：通道5~8。

### 18.1 PWMA 寄存器组

| 地址 | 寄存器 | 作用 |
|---:|---|---|
| FEC0 | PWMA_CR1 | 主控制 |
| FEC1 | PWMA_CR2 | 辅助控制 |
| FEC2 | PWMA_SMCR | 从模式控制 |
| FEC3 | PWMA_ETR | 外部触发 |
| FEC4 | PWMA_IER | 中断使能 |
| FEC5 | PWMA_SR1 | 状态1 |
| FEC6 | PWMA_SR2 | 状态2 |
| FEC7 | PWMA_EGR | 事件产生 |
| FEC8~FECB | PWMA_CCMR1~4 | 通道模式 |
| FECC~FECD | PWMA_CCER1~2 | 通道输出使能/极性 |
| FECE~FECF | PWMA_CNTRH/L | 计数器 |
| FED0~FED1 | PWMA_PSCRH/L | 预分频 |
| FED2~FED3 | PWMA_ARRH/L | 自动重装载周期 |
| FED4 | PWMA_RCR | 重复计数 |
| FED5~FEDC | PWMA_CCR1~4H/L | 比较值/占空比 |
| FEDD | PWMA_BKR | 刹车与主输出控制 |
| FEDE | PWMA_DTR | 死区时间 |
| FEDF | PWMA_OISR | 输出空闲状态 |

PWMB 地址结构与 PWMA 相同，基地址从 `FEE0H` 开始。

### 18.2 基本计算

边沿对齐 PWM：

```text
PWM频率 = SYSCLK / [(PSC + 1) × (ARR + 1)]
占空比 ≈ CCR / (ARR + 1)
```

中心对齐模式的计数周期通常还需乘以 2，具体取决于所选模式。

### 18.3 安全配置顺序

1. 关闭计数器和通道输出。
2. 设置引脚复用。
3. 写预分频 `PSC`。
4. 写周期 `ARR`。
5. 写比较值 `CCR`。
6. 配置输出比较模式和极性。
7. 配置死区、互补输出和刹车功能。
8. 产生更新事件，使预装载值生效。
9. 清状态标志。
10. 使能通道输出和主输出。
11. 启动计数器。

使用电机或半桥时，必须在使能互补输出前配置死区和安全极性。

---

## 19. USB

核心 SFR：

| 寄存器 | 地址 | 作用 |
|---|---:|---|
| USBCON | F4H | USB 控制 |
| USBCLK | DCH | USB 时钟 |
| USBADR | FBH | 间接寄存器地址 |
| USBDAT | ECH | 间接寄存器数据 |

USB SIE 内部寄存器通常通过 `USBADR/USBDAT` 间接访问。

建议：

- CDC/HID 优先使用 STC 官方 USB 库和官方示例。
- 不要仅依据普通 8051 代码自行重写枚举流程。
- USB 时钟、端点缓冲和中断时序要求严格。
- 修改官方库时只改设备描述符、端点数据处理和应用层，不随意改底层状态机。

---

## 20. DMA

DMA 适合把外设数据批量搬运到 XDATA，减少 CPU 中断负担。

主要通道：

- M2M：存储器到存储器。
- ADC DMA。
- SPI DMA。
- UART1~UART4 发送/接收 DMA。
- LCM DMA。

各 DMA 通道通常包含：

```text
CFG   配置
CR    控制
STA   状态
AMT   传输数量
DONE  完成数量
地址寄存器
```

使用顺序：

1. 禁止 DMA 通道。
2. 设置源地址、目标地址和长度。
3. 清完成/错误标志。
4. 配置触发源和地址递增方式。
5. 使能 DMA。
6. 等待完成中断或轮询状态。
7. 检查实际完成数量。

DMA 使用的缓冲区应放在 XDATA，并避免 CPU 与 DMA 同时修改同一缓冲区。

---

## 21. 双数据指针 DPTR

| 寄存器 | 作用 |
|---|---|
| DPL/DPH | DPTR0 |
| DPL1/DPH1 | DPTR1 |
| DPS | 当前 DPTR 选择和自动切换控制 |

典型用途：

- 一个 DPTR 指向源数据。
- 另一个 DPTR 指向目标缓冲区。
- 提高查表、复制和外部存储器访问效率。

使用汇编优化或库函数时，注意中断和被调用函数可能改变 `DPS/DPTR`，必要时保存并恢复。

---

## 22. MDU16 硬件乘除法

XFR 地址：

| 地址 | 寄存器 |
|---:|---|
| FCF0 | MD3 |
| FCF1 | MD2 |
| FCF2 | MD1 |
| FCF3 | MD0 |
| FCF4 | MD5 |
| FCF5 | MD4 |
| FCF6 | ARCON |
| FCF7 | OPCON |

用于硬件乘法、除法、移位等运算。操作数、结果字节顺序和命令编码必须按具体运算章节配置，不要把普通 `ACC/B` 的 8 位乘除法流程套用到 MDU16。

---

## 23. CRC16-Modbus

| 地址 | 寄存器 | 作用 |
|---:|---|---|
| FB30H | CRC16_CR | CRC 控制 |
| FB31H | CRC16_DIN | 输入一个数据字节 |
| FB32H | CRC16_DOH | 结果高字节 |
| FB33H | CRC16_DOL | 结果低字节 |

`CRC16_CR`：

```text
B7 RST：写1复位 CRC 模块，硬件自动清零
B1 REVIN：输入位反转
B0 REVOUT：输出位反转
```

CRC16-Modbus 默认要求 `REVIN=1`、`REVOUT=1`，上电默认值也是 1。

基本流程：

1. 写 `RST=1` 初始化 CRC。
2. 保持 `REVIN=1`、`REVOUT=1`。
3. 按帧顺序逐字节写 `CRC16_DIN`。
4. 读取 `CRC16_DOL` 和 `CRC16_DOH`。
5. Modbus 发送顺序通常为 CRC 低字节在前、高字节在后。

---

## 24. 推荐系统初始化顺序

```text
1. 关闭总中断 EA
2. 使能 XFR 访问 EAXFR
3. 选择并稳定系统时钟
4. 配置 GPIO 初始电平
5. 配置 GPIO 模式和上拉/驱动属性
6. 配置外设引脚复用
7. 初始化定时器
8. 初始化 UART / SPI / I2C / ADC / PWM
9. 清除所有外设中断标志
10. 设置中断优先级
11. 使能各外设中断
12. 最后置 EA=1
```

参考结构：

```c
void Sys_Init(void)
{
    EA = 0;

    P_SW2 |= 0x80;     /* 允许访问 XFR。 */

    XtalSelect_Init();
    GPIO_Init();
    Timer0_Init();
    Uart1_Init();
    Uart2_Init();
    Adc_Init();

    EA = 1;
}
```

---

## 25. Codex 代码审查清单

Codex 阅读或生成 STC8H 驱动时，应逐项检查：

### 寄存器与型号

- [ ] 寄存器是否属于 STC8H8K64U，而不是其他 STC8/STM32 型号。
- [ ] XFR 访问前是否设置 `EAXFR`。
- [ ] 是否误覆盖 `P_SW2` 中的 EAXFR 或串口/I2C 复用位。
- [ ] 官方头文件是否已声明寄存器，避免重复定义冲突。

### GPIO

- [ ] 输出前是否先设置安全初始电平。
- [ ] 模式是否与硬件一致：推挽、高阻、开漏或准双向。
- [ ] ADC 引脚是否高阻并关闭数字输入。
- [ ] I2C 是否有外部上拉。

### 定时器

- [ ] 1T/12T 是否正确。
- [ ] SYSCLK 是否与公式一致。
- [ ] 重装值高低字节是否正确。
- [ ] 启动前是否清溢出标志。
- [ ] T2/T3/T4 中断内是否软件清标志。

### UART

- [ ] 串口引脚组是否正确。
- [ ] 波特率发生器选择是否正确。
- [ ] 定时器重装值是否按实际 SYSCLK 计算。
- [ ] `RI/TI` 是否分别判断并清零。
- [ ] ISR 是否只做字节搬运，不做长时间协议解析。
- [ ] 环形缓冲区是否处理溢出。

### ADC

- [ ] ADC 上电稳定时间是否满足。
- [ ] 结果左/右对齐解析是否一致。
- [ ] 是否清 `ADC_FLAG`。
- [ ] 是否有超时、滤波和通道切换处理。

### IAP

- [ ] `IAP_TPS` 是否匹配系统时钟。
- [ ] 触发序列是否严格为 `5A A5`。
- [ ] 是否按 512 字节扇区规划擦除。
- [ ] 写入前是否考虑只能 `1→0`。
- [ ] 操作结束是否关闭 IAP。

### 中断与并发

- [ ] 共享变量是否声明 `volatile`。
- [ ] 多字节共享变量读取是否具备原子保护。
- [ ] 中断标志是否按手册要求清除。
- [ ] ISR 是否过长或包含阻塞延时。
- [ ] 中断优先级是否会造成低优先级长期得不到执行。

---

## 26. 原手册重点页码索引

以下为原技术手册印刷页码，便于遇到细节时回查：

| 内容 | 原手册页码附近 |
|---|---:|
| 功能脚切换 | 660 |
| 时钟、复位、看门狗 | 699 |
| SFR/XFR | 870 |
| GPIO | 906 |
| 中断系统 | 1022 |
| 定时器 | 1113 |
| 串口 | 1234 |
| 比较器 | 1325 |
| IAP/EEPROM | 1349 |
| ADC | 1383 |
| SPI | 1443 |
| I2C | 1467 |
| 高级 PWM | 1507 |
| DMA | 1742 |
| 双 DPTR | 1782 |
| CRC16-Modbus | 1806 |

---

## 27. 最终原则

本文件用于快速定位和辅助生成代码，不替代原始技术手册。遇到以下情况必须回查原手册：

- 使用不常见 XFR 位定义。
- 使用 USB、DMA、高级 PWM、正交编码器或触摸功能。
- 芯片封装或版本发生变化。
- 需要精确电气参数、最大频率、时序或复位默认值。
- 官方头文件与本文地址不一致。

**以目标芯片最新官方头文件和对应版本技术手册为最终依据。**
