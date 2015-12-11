//#define NU32_STANDALONE  // uncomment if program is standalone, not bootloaded
#include "NU32.h"          // config bits, constants, funcs for startup and UART
#include "LCD.h"
#include "i2c.h"
#include "NU32_I2C.h"

#define MSG_LEN 20

#define ADC_I2C_BAUD  9600
#define READ_ADC_ADDRESS   (0x42)
#define WRITE_ADC_ADDRESS   (0x43)
#define PU_CTRL_ADDR  0x80

#define FULL_DUTY 6 //About 10 MHz
#define VSYNC PORTBbits.RB8
#define HREF PORTBbits.RB10
#define PCLK PORTBbits.RB9
#define RESET LATEbits.LATE9


//Global Variables
int streaming = 1; // Is data streaming out?
int read_state = 0; //Toggles to read every other byte
volatile int img_array[144][174]; //Array to store image
unsigned char pixel[100]; //For UART
char data[100]; //For UART
unsigned char buffer; //Holds the pixel brightness value when first read
unsigned char uart_input; //Assigned to hold the input from processing

int rows = 0; //Counter to track which row
int columns = 0; //Colum counter
int i = 0; //Used to output the image vector in the vsync interrupt
int j = 0; //Same as above

//42 - 0x2A for read and 43 - 0x2B  for write

//read in D0 ­> D7 bits to an 8­bit variable pixel[idx]
char read_byte() {
	char c=0x0; //c's default value is 0b0000;
	c = PORTB;
	LATDbits.LATD3 = 1;
	LATDbits.LATD3 = 0;
	return(c);
}

// This is about a ms delay, used for I2C communication
void delay() {
	int d;
	for (d=0; d<1000; d++){};
}

//Function to reset the camera
void camera_reset(){
	LATEbits.LATE9 = 0;
	delay();
	delay();
	delay();
	LATEbits.LATE9 = 1;
	NU32_WriteUART1("Camera reset \r\n");
}

void camera_config(){
	int i;
	for(i=1;i<10000;i++){}
	//configure the register that slows down pclk period to xclk*32

	delay();
	I2Cstartevent();
	I2Csendonebyte(0x42);
	I2Csendonebyte(0x11);
	I2Csendonebyte(0b10000011);// pclk is 32 times xclk period
	I2Cstopevent();
	delay();

	//Enable scaling
	I2Cstartevent();
	I2Csendonebyte(0x42);
	I2Csendonebyte(0x0c);
	I2Csendonebyte(0b00001000);
	I2Cstopevent();
	delay();

	//Selecting QCIF format
	I2Cstartevent();
	I2Csendonebyte(0x42);
	I2Csendonebyte(0x12);
	I2Csendonebyte(0b00001000);
	I2Cstopevent();
	delay();

	// SUNNY SETTINGS
	//Turn off auto white balance
	I2Cstartevent();
	I2Csendonebyte(0x42);
	I2Csendonebyte(0x13);
	I2Csendonebyte(0xe5);
	I2Cstopevent();
	I2Cstartevent();
	I2Csendonebyte(0x42);
	I2Csendonebyte(0x01);
	I2Csendonebyte(0x5a);
	I2Cstopevent();
	I2Cstartevent();
	I2Csendonebyte(0x42);
	I2Csendonebyte(0x02);
	I2Csendonebyte(0x5c);
	I2Cstopevent();
}

//configure PIC32 digital inputs to receive camera signals
//this function may not be necessary; I think B pins default to digital input
void data_config() {
	DDPCONbits.JTAGEN = 0;
	TRISB = 0xFFFF; //set all Port B pins to digital inputs
	AD1PCFG = 0xFFFF;
	TRISDbits.TRISD3 = 0;
	LATDbits.LATD3 = 0;
	TRISEbits.TRISE9 = 0;
	LATEbits.LATE3 = 1;
}

//initialize PWM for camera's XCLK
void xclkInitialize() {
	T3CONbits.TCKPS = 0; // Timer 3 pre­scaler N = 1 (1:1), thus it ticks at 80 Mhz (PBCLK/N)
	PR3 = FULL_DUTY - 1; // This makes run at 80 Mhz / (N * (PR2+1)) == 10 MHz
	TMR3 = 0; // Set the initial timer count to 0
	OC1CONbits.OCM = 0b110; // PWM mode without the failsafe for OC1
	OC1CONbits.OCTSEL = 1; // use timer 3
	OC1RS = FULL_DUTY/2; // Next duty duty cycle is 0
	OC1R = FULL_DUTY/2; // Initial duty cycle of 0
	T3CONbits.ON = 1; // Turn on timer 3
	OC1CONbits.ON = 1; // Turn on output compare 1

}

/**********************************************************************
* Interrupt(s)
*********************************************************************/

void __ISR(_EXTERNAL_1_VECTOR,IPL7SOFT) frameINT(void)
{
//We're here because VSYNC has triggered. Now we want to wait until HREF
//triggers, meaning actual data is being transmitted.
	while (streaming&&rows<144){ //keep looping until HREF goes high...
		if(HREF){ //if HREF is high...
			while(HREF){
				if (PCLK&&columns<=174) { //if PCLK goes high and row not filled
					if(read_state == 1){
						//Store the brightness to the image array
						buffer = (int)read_byte();
						img_array[rows][columns] = buffer;
						columns++;//increment columns
					}
					read_state = !read_state;//Toggle read state
				
					while(PCLK){{}
						}
					}
			}
			columns = 0;//Reset column counter for each new row
			rows++;//Increment the counter every new row
			read_state = 0;//Starting over, reading every other byte
		}

		//Print the image over uart
		//This could also be done by concatenating a long string terminated
		//by \n
		for(j=0;j<144;j++){
			for (i=0;i<174;i++){
				int pixel = img_array[j][i];
				sprintf(data,"%d ",pixel);
				NU32_WriteUART1(data);
			}
		}
		NU32_WriteUART1("\n");
	break;
	}
	
	streaming = 1; //Streaming is reset
	columns = 0; //Columns reset
	rows = 0; //Rows reset
	IFS0bits.INT1IF = 0; //clear the interrupt flag
}



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
  //i2c_initialize(ADC_I2C_BAUD);
  
  I2Cinitialize(SLOW_BAUD_RATE); //init I2C module. SCL2 pin A2, SDA2 pin A3
  camera_reset();
  camera_config();
  data_config(); //configure PIC32 digital inputs to receive camera signals


  __builtin_enable_interrupts();
  


  while (1) {
    NU32_WriteUART1("Press 'r' to read camera \r\n");
    NU32_WriteUART1("Press 'w' to write to camera \r\n");
    NU32_WriteUART1("Press 'l' to chat with LCD \r\n");
    NU32_WriteUART1("Press 'c' to create clock signal \r\n");
    NU32_WriteUART1("Press 'i' to get image \r\n");
    NU32_WriteUART1("\r\n");
    NU32_ReadUART1(msg, MSG_LEN);             // get the response
    LCD_Clear();                              // clear LCD screen
    LCD_Move(0,0);
    LCD_WriteString(msg);                     // write msg at row 0 col 0

     
    switch(msg[0])
	 {
		case 'c':
		{
			LCD_Clear();
	 		LCD_Move(0,0);
			LCD_WriteString("Generating Clock Signal");
			
			T3CONbits.TCKPS = 0;     // Timer3 prescaler N=4 (1:4)
		  	PR3 = 5;              // period = (PR3+1) * N * 12.5 ns = 10 MHz
		  	TMR3 = 0;                // initial TMR3 count is 0
		  	
		  	OC1CONbits.OCM = 0b110;  // PWM mode without fault pin; other OC1CON bits are defaults
			OC1CONbits.OCTSEL = 1;
		  	OC1RS = 5;             // duty cycle = OC1RS/(PR2+1) 
		  	OC1R = 5;              // initialize before turning OC1 on; afterward it is read-only
		  	T3CONbits.ON = 1;        // turn on Timer3
		  	OC1CONbits.ON = 1;       // turn on OC1
			
			LCD_Move(1,0);
			LCD_WriteString("Done :)");

			break;
		}

		case 'i':
		{
			while (streaming&&rows<144){ //keep looping until HREF goes high...
				if(HREF){ //if HREF is high...
					while(HREF){
						if (PCLK&&columns<=174) { //if PCLK goes high and row not filled
							if(read_state == 1){
								//Store the brightness to the image array
								buffer = (int)read_byte();
								img_array[rows][columns] = buffer;
								columns++;//increment columns
							}
							read_state = !read_state;//Toggle read state
							while(PCLK){{}
							}
						}
					}
					columns = 0;//Reset column counter for each new row
					rows++;//Increment the counter every new row
					read_state = 0;//Starting over, reading every other byte
				}
			


				//Print the image over uart
				//This could also be done by concatenating a long string terminated
				//by \n
				for(j=0;j<144;j++){
					for (i=0;i<174;i++){
						int pixel = img_array[j][i];
						sprintf(data,"%d ",pixel);
						NU32_WriteUART1(data);
					}
				}
				NU32_WriteUART1("\n");
			break;
			}
	
			streaming = 1; //Streaming is reset
			columns = 0; //Columns reset
			rows = 0; //Rows reset
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
