#include "GPIO.h"

void GPIO_Init(void)
{
	P0M0 = 0x00; P0M1 = 0x00;   //P0 全部设置为准双向口
	P1M0 = 0x00; P1M1 = 0x00;	//P1 全部设置为准双向口
	P2M0 = 0x00; P2M1 = 0x00;	//P2 全部设置为准双向口
	P3M0 = 0x1c; P3M1 = 0x00;    //P3 全部设置为准双向口
	P4M0 = 0x00; P4M1 = 0x00;	//P4 全部设置为准双向口
	P5M0 = 0x01; P5M1 = 0x00; 	//P5 全部设置为准双向口
}

void Pxup_Init(void)
{
	P_SW2 |= 0x80;		            //将EAXFR位置1，以访问在XDATA区域的扩展SFR
	
	/* 开启所有 I2C 引脚内部上拉电阻，确保软件I2C时序可靠 */
	P0PU |= 0x16;                   //P0.1(BUS3_SCL) P0.2(BUS3_SDA) P0.4(BUS4_SCL)
	P2PU |= 0x60;                   //P2.5(BUS1_SCL) P2.6(BUS1_SDA)
	P4PU |= 0x60;                   //P4.5(BUS2_SCL) P4.6(BUS2_SDA)
	P5PU |= 0x14;                   //P5.2(BUS4_SDA) P5.4(运行指示灯)
	
	P_SW2 &= 0x7F;		            //将EAXFR位置0，恢复访问XRAM
}
