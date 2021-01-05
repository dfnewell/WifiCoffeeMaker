#include <stdint.h>
#include <stdbool.h>
#include <math.h>
#include "inc/hw_ints.h"
#include "inc/hw_timer.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "driverlib/rom.h"
#include "driverlib/sysctl.h"
#include "driverlib/timer.h"
#include "driverlib/interrupt.h"
#include "inc/hw_nvic.h"
#include "driverlib/gpio.h"
#include "inc/hw_gpio.h"

static unsigned long milliseconds = 0;
volatile unsigned long coffeeTimer = 0;
void timerInit(void)
{

	SysTickPeriodSet(0x00FFFFFF);
  SysTickEnable();

    SysCtlPeripheralEnable(SYSCTL_PERIPH_TIMER5);
    TimerConfigure(TIMER5_BASE, TIMER_CFG_PERIODIC_UP);

    TimerLoadSet(TIMER5_BASE, TIMER_A, SysCtlClockGet()/1000);

    IntEnable(INT_TIMER5A);
    TimerIntEnable(TIMER5_BASE, TIMER_TIMA_TIMEOUT);

    TimerEnable(TIMER5_BASE, TIMER_A);

    IntMasterEnable();

}

unsigned long micros(void)
{
	return (milliseconds * 1000) + (HWREG(TIMER5_BASE + TIMER_O_TAV) / 80);
}

unsigned long millis(void)
{
	return milliseconds;
}

void delayMicroseconds(unsigned int us)
{
	volatile unsigned long elapsedTime;
	unsigned long startTime = HWREG(NVIC_ST_CURRENT);
	do{
		elapsedTime = startTime-(HWREG(NVIC_ST_CURRENT) & 0x00FFFFFF);
	}
	while(elapsedTime <= us*80);
}

void delay(uint32_t milliseconds)
{
		unsigned long i;
		for(i=0; i<milliseconds; i++){
			delayMicroseconds(1000);
		}
}

void Timer5IntHandler(void)
{
    TimerIntClear(TIMER5_BASE, TIMER_TIMA_TIMEOUT);

	milliseconds++;
	
	/*if (GPIOPinRead(GPIO_PORTE_BASE,GPIO_PIN_4)) // If coffee maker is on, track time
	{
		coffeeTimer = millis();
	}
	
	if (millis()- coffeeTimer > 1800*1000) // Once the coffee timer exceeds 30 minutes, turn off.
	{ 
		GPIOPinWrite(GPIO_PORTE_BASE,GPIO_PIN_4,0x00);
		coffeeTimer = 0; // reset coffee timer
	}*/
		
		
}


