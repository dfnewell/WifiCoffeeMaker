/*
 * main.h
 *
 *  Created on: Aug 14, 2015
 *      Author: secakmak
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#include "inc/tm4c123gh6pm.h"
#include "Hidden.h"
#include "inc/hw_gpio.h"
#include "inc/hw_types.h"
#include "inc/hw_memmap.h"
#include "inc/hw_ints.h"
#include "inc/hw_nvic.h"

#include "driverlib/fpu.h"
#include "driverlib/debug.h"
#include "driverlib/sysctl.h"
#include "driverlib/pin_map.h"
#include "driverlib/gpio.h"
#include "driverlib/pwm.h"
#include "driverlib/timer.h"
#include "driverlib/adc.h"
#include "driverlib/uart.h"
#include "driverlib/interrupt.h"

#include "utils/uartstdio.h"
#include "utils/cmdline.h"

#include "DelayTimer.h"

static char COMMAND[128];
static char ReceivedData[512]; // This is a 512 bit buffer for storing chars off the UART1
static char str[128];

bool process = true;
bool InitializeRoutine();

char *Substring(char *src, char *dst, int start, int stop);
int SearchIndexOf(char src[], char str[]);
char* itoa(int i, char b[]);
void ftoa(float f,char *buf);
//Globally enable interrupts 
void IntGlobalEnable(void)
{
    __asm(" cpsie i\n");
}

//Globally disable interrupts 
void IntGlobalDisable(void)
{
    __asm(" cpsid i\n");
}


#define Period  320000 //(16000000/50) 50Hz

// INITIALIZE BOARD COMPONENTS:
void InitUART(void)
{
    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOA);
    SysCtlPeripheralEnable(SYSCTL_PERIPH_UART0);
    GPIOPinConfigure(GPIO_PA0_U0RX);
    GPIOPinConfigure(GPIO_PA1_U0TX);
    GPIOPinTypeUART(GPIO_PORTA_BASE, GPIO_PIN_0 | GPIO_PIN_1);
    UARTClockSourceSet(UART0_BASE, UART_CLOCK_PIOSC);
    UARTStdioConfig(0, 115200, 16000000);
}

void InitESPUART(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOB);
	SysCtlPeripheralEnable(SYSCTL_PERIPH_UART1);
	GPIOPinConfigure(GPIO_PB0_U1RX);
	GPIOPinConfigure(GPIO_PB1_U1TX);
	GPIOPinTypeUART(GPIO_PORTB_BASE, GPIO_PIN_0 | GPIO_PIN_1);
	UARTClockSourceSet(UART1_BASE, UART_CLOCK_PIOSC);
	UARTConfigSetExpClk(UART1_BASE, 16000000, 115200, (UART_CONFIG_WLEN_8 | UART_CONFIG_STOP_ONE | UART_CONFIG_PAR_NONE));
	UARTEnable(UART1_BASE);

}
void InitCoffeeSwitch(void)
{
	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOE);
	GPIOPinTypeGPIOOutput(GPIO_PORTE_BASE,GPIO_PIN_4);
	GPIOPinWrite(GPIO_PORTE_BASE,GPIO_PIN_4,0x00);
  GPIO_PORTF_DIR_R &= ~0x10;    // (c) make PF4 in (built-in button)
  GPIO_PORTF_DEN_R |= 0x10;     //     enable digital I/O on PF4
  GPIO_PORTF_PUR_R |= 0x10;     //     enable weak pull-up on PF4
}



void
Interrupt_Init(void)
{
	NVIC_EN0_R |= 0x40000000;  		// enable interrupt 30 in NVIC (GPIOF)
	NVIC_PRI7_R &= 0x00E00000; 		// configure GPIOF interrupt priority as 0
	GPIO_PORTF_IM_R |= 0x10;   		// arm interrupt on PF4
	GPIO_PORTF_IS_R &= ~0x11;     // PF4 is edge-sensitive
	GPIO_PORTF_IBE_R &= ~0x11;   	// PF4 single edge trigger 
	GPIO_PORTF_IEV_R &= ~0x11;  	// PF4 falling edge event
	IntGlobalEnable();						  // Enable Interrupts
}

//interrupt handler
void GPIOPortF_Handler(void){
	//switch debounce
	NVIC_EN0_R &= ~0x40000000; 
	SysCtlDelay(53333);	// Delay for a while
	NVIC_EN0_R |= 0x40000000; 
	
	//SW1 has action
	if(GPIO_PORTF_RIS_R&0x10)
	{
		// acknowledge flag for PF4
		GPIO_PORTF_ICR_R |= 0x10; 
		
		//SW1 is pressed
		if((GPIO_PORTF_DATA_R&0x10)==0x00) 
		{
			UARTprintf("press!");
			//Toggle power for coffee maker
			GPIOPinWrite(GPIO_PORTE_BASE,GPIO_PIN_4,~GPIOPinRead(GPIO_PORTE_BASE,GPIO_PIN_4));
		}
	}
	
}



//-----------------------------------------------------------------
// WIFI COMMANDS FOR ESP8266:
void SendATCommand(char *cmd)
{
	while(UARTBusy(UART1_BASE));
	while(*cmd != '\0')
	{
		UARTCharPut(UART1_BASE, *cmd++);
	}
	UARTCharPut(UART1_BASE, '\r'); //CR
	UARTCharPut(UART1_BASE, '\n'); //LF

}

int recvString(char *target, char *data, int timeout, bool check) //Function to receive strings from UART1
{
	int i=0;
	char a;
	unsigned long start = millis();

    while (millis() - start < timeout)
    {
    	while(UARTCharsAvail(UART1_BASE))
    	{
              a = UARTCharGet(UART1_BASE);
              if(a == '\0') continue;
              data[i]= a;
              i++;
    	}

    	if(check)
    	{
    		if (SearchIndexOf(data, target) != -1) // This search looks for a desired target text in string, if found it exits, if not 
    		{																			 // it retuns a 0.
    			break;
    		}
    	}
    }

    return 0;
}

bool recvFind(char *target, int timeout,bool check) // Looks through an string input using recvString (defined above)
{																										// Basically simplified recvString() that returns boolean rather than int.
	recvString(target, ReceivedData, timeout, check);

	if (SearchIndexOf(ReceivedData, target) != -1)
	{
		return true;
	}
	return false;
}

bool recvFindAndFilter(char *target, char *begin, char *end, char *data, int timeout) 
{	// Runs receive command, 
	recvString(target, ReceivedData, timeout, true);

    if (SearchIndexOf(ReceivedData, target) != -1) //Basically, if target is in recvd data
    {
         int index1 = SearchIndexOf(ReceivedData, begin); // establish start and end of fields for found target
         int index2 = SearchIndexOf(ReceivedData, end);   // based on user definition

         if (index1 != -1 && index2 != -1) // If both the start and end text strings are found
         {
             index1 += strlen(begin); // Change index1 to be location of first occurence + the length of begin 
             Substring(ReceivedData,data,index1, index2); //Make a substring that is the length of index 1 to index 2 and store in data.
             return true;
         }
     }
     data = ""; //elsewise the data will be empty and exit "false"
     return false;
}

bool ATesp(void)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT");
	return recvFind("OK",5000, true);
}

bool RSTesp(void)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+RST");
	return recvFind("OK",5000, false);
}

bool CWMODEesp(void)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+CWMODE=1");
	return recvFind("OK",5000, true);
}

bool CWJAPesp(void)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand(WIFI_CREDENTIALS); //Your Wifi: NetworkName, Password
	return recvFind("OK",10000, true);
}

bool CWQAPesp(void)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+CWQAP");
	return recvFind("OK",10000, true);
}

bool CIPMUXesp(void)
{
	memset(ReceivedData, 0, sizeof(ReceivedData)); // this line basically clears ReceivedData.
	SendATCommand("AT+CIPMUX=1");
	return recvFind("OK",5000, true);
}

bool ATGMResp(char *version)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+GMR");
	return recvFindAndFilter("OK", "\r\r\n", "\r\n\r\nOK", version,10000);
}

bool aCWMODEesp(char *list)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+CWMODE=?");
	return recvFindAndFilter("OK", "+CWMODE:(", ")\r\n\r\nOK", list,10000);
}

bool aCWLAPesp(char *list)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+CWLAP");
	return recvFindAndFilter("OK","\r\r\n", "\r\n\r\nOK", list,15000);
}

bool aCIFSResp(void)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+CIFSR");
	recvString("OK",ReceivedData,15000,true);
	UARTprintf(ReceivedData);
	return recvFind("OK",2000,true);
} 


bool CIPSTOesp(void)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+CIPSTO=10000");
	return recvFind("OK",2000, true);
}

bool CIPSERVEResp(void) // This opens communication between esp as client and a server as remote host (Java script built for this)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+CIPSERVER=1,80"); //Sets up Wifi module to be the server
	return recvFind("OK",2000, true);
}
/*
bool CIPSTARTesp(void) // This opens communication between esp as client and a server as remote host (Java script built for this)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+CIPSTART=\"TCP\",\"192.255.0.100\",80"); //Server IP and Port: such as 192.255.0.100, 9999
	return recvFind("OK",2000, true);
}
*/
bool CIPCLOSEesp(void)
{
	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand("AT+CIPCLOSE");
	return recvFind("OK",5000, true);
}

bool CIPSENDesp(char *text)
{
	//int len = strlen(text)+2;

	//itoa(len,str);

	char* AT_CMD_SEND = "AT+CIPSEND=0,20";
	char CMD_TEXT[128];
	strcpy(CMD_TEXT,AT_CMD_SEND);
	//strcat(CMD_TEXT,str);

	memset(ReceivedData, 0, sizeof(ReceivedData));
	SendATCommand(CMD_TEXT);
	delay(20);
	SendATCommand(text);
	return recvFind("SEND OK",2000, true);
}

void HardwareReset()
{
	GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_0); //Output
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_4, 0x00); //LOW ->Reset to ESP8266
    delay(50);
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_0); //Output ->// Open drain; reset -> GND
    delay(10);
    GPIOPinTypeGPIOInput(GPIO_PORTD_BASE, GPIO_PIN_0); //Input ->// Back to high-impedance pin state
    delay(3000);
}

void ProcessCommand(char *CommandText) //This receives incoming TCP commands from Telnet and processes them
{
	long Status;
	char *array[10];
	int i=0;

	array[i] = strtok(CommandText,":"); // This parses the command text, basically the incoming line starts with "+IPD:"
																			// and so the next few lines up to memset(COMMAND...) "cut" the IPD+ part off 

	while(array[i]!=NULL)
	{
		array[++i] = strtok(NULL,":");
	}

	memset(COMMAND, 0, sizeof(COMMAND)); 							// Clears the cache of COMMAND so we can store the received TelNet message
	strncpy(COMMAND, array[1], (strlen(array[1])-1)); // Copies the command into COMMAND

	UARTprintf("CMD->%s\n",COMMAND); // Confirms what's received into the terminal (UART0)
	// Programmed buttons for the Telnet phone app: BTN1 = ActionA; BTN2 = ActionB; BTN3 = ActionC; BTN4 = ActionD
	if(COMMAND[6] == 'A') 		// Now within here, we set up our functions for turning the thing on, off and making a timer 
		{ UARTprintf("ON!\n");  // if we can make it happen
		  GPIOPinWrite(GPIO_PORTE_BASE,GPIO_PIN_4,GPIO_PIN_4);
			SendATCommand("AT+CIPSEND=0,16");
			delay(5);
			SendATCommand("Coffee Maker On!");
		}
	else if(COMMAND[6] == 'B')
	  {
			UARTprintf("OFF!\n");
			GPIOPinWrite(GPIO_PORTE_BASE,GPIO_PIN_4,0x00);
			SendATCommand("AT+CIPSEND=0,17");
			delay(5);
			SendATCommand("Coffee Maker Off!");
		}
	else if(COMMAND[6] == 'C')
		UARTprintf("SET TIME!\n");
	else if(COMMAND[6] == 'D') //This just shows we can send something and have the chip respond
	{	  SendATCommand("AT+CIPSEND=0,6");
			delay(10);
			SendATCommand("Hello!");
	}
	else
		UARTprintf("Bad command!\n");
}

void QuitProcess(void)
{
	process = false;
}
//------------------------------------------------------------------------------------------------------------------------------------

int main(void)
{
	bool TaskStatus;

	SysCtlClockSet(SYSCTL_SYSDIV_1 | SYSCTL_USE_OSC |   SYSCTL_OSC_MAIN | SYSCTL_XTAL_16MHZ);

	SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOF);
    GPIOPinTypeGPIOOutput(GPIO_PORTF_BASE,GPIO_PIN_1 | GPIO_PIN_2 | GPIO_PIN_3);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x00);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x00);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x00);

    SysCtlPeripheralEnable(SYSCTL_PERIPH_GPIOD);
    GPIOPinTypeGPIOOutput(GPIO_PORTD_BASE, GPIO_PIN_0);
    GPIOPinWrite(GPIO_PORTD_BASE, GPIO_PIN_0, 0x00);
    GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x02);

    timerInit();

    HardwareReset();

	InitUART();
	InitESPUART();
	InitCoffeeSwitch();
	Interrupt_Init();
	IntMasterEnable();
	
	UARTprintf("Execute!\n");

	delay(1000);

	TaskStatus = ATesp();
	TaskStatus = RSTesp();
	TaskStatus = CWMODEesp();

	while(true)
	{
		UARTprintf("Trying to connect wi-fi\n");
		TaskStatus = CWJAPesp();
		if(TaskStatus)
		{
			UARTprintf("Connection is established!\n");
			break;
		}
	}

	GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x00);
	GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x04);

	TaskStatus = CIPMUXesp(); 
	TaskStatus = CIPSERVEResp();

	while(true)
	{
		// Here is where ongoing comms is set up with the board and the program made by the initial user.
		// While(true) is basically infinite loop.
		TaskStatus = InitializeRoutine();
		if(TaskStatus) //If you exit InitializeRoutine for some reason, you'll end up in here.
		{
			//CIPCLOSEesp();
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x00); //Turn off Green Led
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x04); //Turn on Blue Led - Error light
		}
		else
		{
			delay(10);
			CWQAPesp(); // Board quits access point, need to hit reset bascially.
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x00); //Turn off Blue Led
			GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_1, 0x02); //Turn on Red Led - Inactive light
		}
	}

}

bool InitializeRoutine()
{
	bool status;
	process = true;

	UARTprintf("Waiting to start...\n");
	while(true) // this waits until status turns true given break on line 400
	{
		status = aCIFSResp();
		if(status)
		{
			UARTprintf("\n\r Ready to receive commands \n\r");
			break;
		}

		delay(50); 
	}

	GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_2, 0x00);
	GPIOPinWrite(GPIO_PORTF_BASE, GPIO_PIN_3, 0x08); //Turn on Green Led-> READY

	int i=0;
	char a;
	memset(ReceivedData, 0, sizeof(ReceivedData)); //Clear Received Data
	unsigned long start;

	while(process) 
	{
		if(UARTCharsAvail(UART1_BASE)) // This is where the board is looking for the received TelNet message
		{
			if (SearchIndexOf(ReceivedData, "+IPD,") != -1) // When it's found the board collects the message in 'ReceivedData'
			{
				i=0;
				memset(ReceivedData, 0, sizeof(ReceivedData));

				start = millis();

			    while (millis() - start < 5000) //before timeout
			    {
			    	while(UARTCharsAvail(UART1_BASE))
			    	{
			    		a = UARTCharGet(UART1_BASE);
			    		if(a == '\0') continue;
			    		ReceivedData[i]= a;
			    		i++;
			    	}

		    		if (SearchIndexOf(ReceivedData, "\n") != -1) // Once it's at the end of Received Data break out of the fetch loop
		    		{
		    			break;
		    		}
			    }

					ProcessCommand(ReceivedData); // Now process it (see function definition from above)

				i=0;
				memset(ReceivedData, 0, sizeof(ReceivedData));
			}
			else // In case the UART1 message is not from Telnet, then it just gets processed into the ReceivedData buffer... 
			{
				a = UARTCharGet(UART1_BASE);
				if(a == '\0') continue;
				ReceivedData[i]= a;
				i++;
			}
		}
	}

	return true;
}

char *Substring(char *src, char *dst, int start, int stop)
{
	int len = stop - start;
	strncpy(dst, src + start, len);

	return dst;
}

int SearchIndexOf(char src[], char str[]) // Searches source of an array for a string.
{
   int i, j, firstOcc;
   i = 0, j = 0;

   while (src[i] != '\0') // Before the end of the source string
   {

      while (src[i] != str[0] && src[i] != '\0') // While the source is not at the beginning of the target str, and not at the end of the source
         i++; //go to the next char in the source array

      if (src[i] == '\0') // when it's at the end of the source 
         return (-1);     // then is didn't find the target str and returns -1.

      firstOcc = i;				// tracks the first occurrence?

      while (src[i] == str[j] && src[i] != '\0' && str[j] != '\0') // while index positions match, and are not at the string end 
      {
         i++; // increment both indexing variables
         j++;
      }

      if (str[j] == '\0') // if the target string reaches the end value, return the first position they matched?
         return (firstOcc);
      if (src[i] == '\0') // if the source string gets to its end, target not found, return -1.
         return (-1);

      i = firstOcc + 1; //I guess this next two reset the indexes?
      j = 0;
   }

   return (-1); //Again, if target not found, exit.
}

char* itoa(int i, char b[])
{
    char const digit[] = "0123456789";
    char* p = b;
    if(i<0){
        *p++ = '-';
        i *= -1;
    }
    int shifter = i;
    do{
        ++p;
        shifter = shifter/10;
    }while(shifter);
    *p = '\0';
    do{
        *--p = digit[i%10];
        i = i/10;
    }while(i);
    return b;
}

void ftoa(float f,char *buf)
{
    int pos=0,ix,dp,num;
    if (f<0)
    {
        buf[pos++]='-';
        f = -f;
    }
    dp=0;
    while (f>=10.0)
    {
        f=f/10.0;
        dp++;
    }
    for (ix=1;ix<8;ix++)
    {
            num = f;
            f=f-num;
            if (num>9)
                buf[pos++]='#';
            else
                buf[pos++]='0'+num;
            if (dp==0) buf[pos++]='.';
            f=f*10.0;
            dp--;
    }
}
