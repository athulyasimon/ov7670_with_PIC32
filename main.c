//#define NU32_STANDALONE  // uncomment if program is standalone, not bootloaded
#include "NU32.h"          // config bits, constants, funcs for startup and UART
#include "LCD.h"
#include "i2c.h"

#define MSG_LEN 20

#define ADC_I2C_BAUD  9600
#define READ_ADC_ADDRESS   (0x42)
#define WRITE_ADC_ADDRESS   (0x43)
#define PU_CTRL_ADDR  0x80


//42 - 0x2A for read and 43 - 0x2B  for write

int main() {
  char msg[MSG_LEN];
  int nreceived = 1;

  NU32_Startup();         // cache on, interrupts on, LED/button init, UART init

  NU32_LED1 = 1;
  NU32_LED2 = 1;

  NU32_LED1 = 1;
  NU32_LED2 = 0;

  __builtin_disable_interrupts();
 
  LCD_Setup();
  i2c_initialize(ADC_I2C_BAUD);

  __builtin_enable_interrupts();
  


  while (1) {
    NU32_WriteUART1("Press 'r' to read camera \r\n");
    NU32_WriteUART1("Press 'w' to write to camera \r\n");
    NU32_WriteUART1("Press 'l' to chat with LCD \r\n");
    NU32_WriteUART1("Press 'c' to create clock signal \r\n");
    NU32_WriteUART1("\r\n");
    NU32_ReadUART1(msg, MSG_LEN);             // get the response
    LCD_Clear();                              // clear LCD screen
    LCD_Move(0,0);
    LCD_WriteString(msg);                     // write msg at row 0 col 0
    // sprintf(msg, "Received %d", nreceived);   // display how many messages received
    // ++nreceived;
    // LCD_Move(1,3);
    // LCD_WriteString(msg); 
     
    switch(msg[0])
	 {
		case 'c':
		{
			LCD_Clear();
	 		LCD_Move(0,0);
			LCD_WriteString("Generating Clock Signal");
			
			T3CONbits.TCKPS = 0;     // Timer2 prescaler N=4 (1:4)
		  	PR3 = 3999;              // period = (PR2+1) * N * 12.5 ns = 100 us, 10 kHz
		  	TMR3 = 0;                // initial TMR2 count is 0
		  	OC1CONbits.OCTSEL = 1;
		  	OC1CONbits.OCM = 0b110;  // PWM mode without fault pin; other OC1CON bits are defaults
		  	OC1RS = 3000;             // duty cycle = OC1RS/(PR2+1) = 25%
		  	OC1R = 3000;              // initialize before turning OC1 on; afterward it is read-only
		  	T3CONbits.ON = 1;        // turn on Timer2
		  	OC1CONbits.ON = 1;       // turn on OC1
			
			LCD_Move(1,0);
			LCD_WriteString("Done :)");

			break;
		}
	 	case 'l':
	 	{
	 		LCD_Clear();
	 		LCD_Move(0,0);
			LCD_WriteString("Hello - type :)");
			LCD_Move(1,0);
			
		    NU32_ReadUART1(msg, MSG_LEN);             // get the response
		    LCD_Clear();
			LCD_Move(0,0);
		    LCD_WriteString(msg);
		    break;
		}

		case 'r':
		{
			LCD_Clear();
	 		LCD_Move(0,0);
			LCD_WriteString("Reading Camera");
			
			//The device slave addresses are 42 for write and 43 for read
			//i2c_read(char deviceaddress, char registeraddress, char value);
			i2c_read(READ_ADC_ADDRESS, PU_CTRL_ADDR, 0x00);

			
			LCD_Move(1,0);
			LCD_WriteString("Camera Read");

			break;
		}
		case 'w':
		{
			LCD_Clear();
	 		LCD_Move(0,0);
			LCD_WriteString("Writing To Cam");
			
			//The device slave addresses are 42 for write and 43 for read
			i2c_write(WRITE_ADC_ADDRESS, PU_CTRL_ADDR, 0x01);
			
			LCD_Move(1,0);
			LCD_WriteString("Done :)");

			break;
		}
		
     NU32_WriteUART1(msg);
     NU32_WriteUART1("\r\n");
     }

  }
    return 0;
}
