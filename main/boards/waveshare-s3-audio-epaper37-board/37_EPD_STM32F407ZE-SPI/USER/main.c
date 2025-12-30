//////////////////////////////////////////////////////////////////////////////////	 
//本程序只供学习使用，未经作者许可，不得用于其它任何用途
//中景园电子
//店铺地址：http://shop73023976.taobao.com
//
//  文 件 名   : main.c
//  版 本 号   : v2.0
//  作    者   : ZhaoJian
//  生成日期   : 2023-12-1
//  最近修改   : 
//  功能描述   :演示例程(STM32F103系列)
//              说明: 
//              ----------------------------------------------------------------
//              GND   电源地
//              VCC   3.3v电源
//              SCL   PG12（SCLK）
//              SDA   PD5（MOSI）
//              RES   PD4
//              DC    PD15
//              CS    PD1
//              BLK   PE8
//              ----------------------------------------------------------------
// 修改历史   :
// 日    期   : 
// 作    者   : ZhaoJian
// 修改内容   : 创建文件
//版权所有，盗版必究。
//Copyright(C) 中景园电子2023-12-1
//All rights reserved
//******************************************************************************/

#include "delay.h"
#include "usart.h"
#include "EPD_GUI.h"
#include "Pic.h"


u8 ImageBW[12480];

int main()
{
	float num=12.05;
	u8 dat=0;
	delay_init(168);
	uart_init(115200);
	EPD_GPIOInit();	
	Paint_NewImage(ImageBW,EPD_W,EPD_H,0,WHITE);		//创建画布
  Paint_Clear(WHITE);	
	/************************全刷************************/
	EPD_Init();
	EPD_Display(gImage_1);
	EPD_Update();
	EPD_DeepSleep();
	delay_ms(1000);
	/************************快刷************************/
	EPD_FastInit();
	EPD_Display(gImage_2);
	EPD_Update();
	EPD_DeepSleep();
	delay_ms(1000);
	/************************局刷************************/
 	EPD_FastInit();
	EPD_Display_Clear();
	EPD_Update();  //局刷之前先对E-Paper进行清屏操作
	while(1)
	{
		EPD_PartInit();
		EPD_ShowPicture(24,0,368,200,gImage_3,BLACK);
		EPD_DrawRectangle(20,200,55,235,BLACK,1);
		EPD_DrawRectangle(80,200,115,235,BLACK,0); 
		EPD_DrawCircle(331,220,18,BLACK,1); //Hollow circle.
		EPD_DrawCircle(376,220,18,BLACK,0); 
		EPD_ShowWatch(148,190,num,4,2,48,BLACK);
		num+=0.01;
		EPD_Display(ImageBW);
		EPD_Update();
		delay_ms(1000);
		dat++;
		if(dat==5)
		{
			EPD_FastInit();
			while(1)
			{
			 EPD_Display_Clear();
			 EPD_Update();
	     EPD_DeepSleep();
			}
		}
	}
}


