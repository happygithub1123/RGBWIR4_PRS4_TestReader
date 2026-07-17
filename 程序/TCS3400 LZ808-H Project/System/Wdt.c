#include "Wdt.h"

/* 看门狗初始化，超时时间约4550ms */
void Wdt_Init(void)
{
	WDT_CONTR &= 0x7F;
	WDT_CONTR |= 0x28;
	WDT_CONTR |= 0x07;	
}


/* 喂狗：清除看门狗计数器 */
void Wdt_Fooddog(void)
{
	WDT_CONTR |= 0x10;
}