//#define NU32_STANDALONE  // uncomment if program is standalone, not bootloaded
#include "NU32.h"          // config bits, constants, funcs for startup and UART
#include "i2c.h"
#include "NU32_I2C.h"

#define MSG_LEN 20
#define FULL_DUTY 6000 //About 10 MHz
#define ADC_I2C_BAUD  9600

#define HREF PORTBbits.RB10
#define PCLK PORTBbits.RB9

/******************************************************************************
* HELPER FUNCTION PROTOTYPES
******************************************************************************/

void xclkInitialize();
char read_byte();
void data_config();
void CNinitialize();
void camera_reset();
void write_reset();
void delay();

/******************************************************************************
* GLOBAL VARIABLES
*****************************************************************************/
unsigned int newF = 0;
volatile char img_array[144][174];
int rows = 0;
int columns = 0;
int i = 0;
int j = 0;
int h = 0;
unsigned char pixel;
unsigned char data[100];
/**********************************************************************
* Interrupt(s)
*********************************************************************/
void __ISR(_CHANGE_NOTICE_VECTOR, IPL3SOFT) CNISR(void) { // INT step 1
	newF = PORTF; // since pins on port F are being monitored by CN,
				  // must read both to allow continued functioning
	write_reset();
	LATESET = 0b100000000;

	CNCONbits.ON = 0; // Temporarily turn CN pin off
	// NU32_LED1 = !NU32_LED1; // toggle LED1
	IFS1bits.CNIF = 0; // clear the interrupt flag
	NU32_WriteUART1("interrupt \r\n");

	while(rows < 144){
		if(HREF){
			while(HREF){
				if(PCLK && columns <= 174){
					img_array[rows][columns] = read_byte();
					columns++;
				}
				columns = 0;
				rows++;
			}
		}
	}

	for(j=0; j<144; j++){
		for(i=0; i<1; i++){
			pixel = img_array[j][i];
			sprintf(data, "%d", pixel);
			NU32_WriteUART1(data);
			NU32_WriteUART1(" ");
		}
		
	}

	columns = 0;
	rows = 0;	
	NU32_WriteUART1("\n");
}

/******************************************************************************
* MAIN FUNCTION
******************************************************************************/
int main(){
	unsigned char msg[MSG_LEN];
	unsigned char buffer;
	unsigned char data[100];
	char * value_ptr;
	char value;
	NU32_Startup();

	__builtin_disable_interrupts();

	i2c_initialize(ADC_I2C_BAUD);
	xclkInitialize();
	data_config();
	CNinitialize();

  	__builtin_enable_interrupts();

  	//Camera settings
	i2c_write(0x42, 0x11, 0b10000011); //pclk is 32 times xclk period
	i2c_write(0x42, 0x0c, 0b00001000); //Enable scaling
	i2c_write(0x42, 0x12, 0b00001000); //QCIF format

  	while(1){
  		NU32_WriteUART1("Press 'r' to read a byte \r\n");
  		NU32_WriteUART1("Press 'h' to check HREF \r\n");
  		NU32_WriteUART1("Press 'c' to reset camera \r\n");
  		NU32_WriteUART1("Press 's' to read a setting \r\n");
  		NU32_WriteUART1("\r\n");
    	NU32_ReadUART1(msg, MSG_LEN);             // get the response

    	switch(msg[0]){
    		case 'r':
    		{
    			buffer = read_byte();
    			sprintf(data, "%d",buffer);
    			NU32_WriteUART1(data);
    			break;
    		}

    		case 'h':
    		{
    			for(h=0; h<100; h++){
    				buffer = HREF;
    				sprintf(data, "%d",buffer);
    				NU32_WriteUART1(data);
    			}    				
    			break;
    		}

    		case 'c':
    		{
    			camera_reset();
    			NU32_WriteUART1("Camera reset \r\n");
    			break;
    		}

    		case 's':
    		{
			    i2c_startevent();
				// i2c_sendonebyte(0x42);
				// i2c_sendonebyte(0x01);

			 //    i2c_repeatstartevent();
				i2c_sendonebyte(0x43);
				*value_ptr = i2c_readonebyte(0x01);
				i2c_stopevent();
				// value_ptr = &value;
				// I2Cread(0x42, 0x01, value_ptr);
    			// i2c_read(0x42, 0x0c, value_ptr);
    			sprintf(data, "%d", value_ptr);
    			NU32_WriteUART1(data);
    			NU32_WriteUART1("\r\n");
    			break;
    		}
    	}

  	}
}

//configure PIC32 digital inputs to receive camera signals
//All pins on portB are analog inputs by default
void data_config() {
	DDPCONbits.JTAGEN = 0; // Disable JTAG, make pins 4 and 5 of Port A available.
	TRISB = 0xFFFF; //set all Port B pins to inputs
	AD1PCFG = 0xFFFF; //set all Port B pins to digital 
	TRISDbits.TRISD3 = 0; //0 = output
	LATDbits.LATD3 = 0;	  //0 sets output to low
	TRISEbits.TRISE9 = 0; //0 = output
	TRISEbits.TRISE8 = 0;
	LATESET = 0b110000000; //set pins 8 and 9 high
	// LATEbits.LATE3 = 1;   //1 sets output to floating
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

//read in D0 ­> D7 bits to an 8­bit variable pixel[idx]
char read_byte() {
	char c=0x0; //c's default value is 0b0000;
	c = PORTB;
	return(c);
}

void CNinitialize(){
	CNPUEbits.CNPUE17 = 1; //CN17/RF4 input has internal pull-up
	CNCONbits.ON = 1; // step 2: configure peripheral: turn on CN
	CNEN = (1<<17)|(1<<18); // listen to CN17/RF4, CN18/RF5
	IPC6bits.CNIP = 3; // step 3: set interrupt priority
	IPC6bits.CNIS = 2; // step 4: set interrupt subpriority
	IFS1bits.CNIF = 0; // step 5: clear the interrupt flag
	IEC1bits.CNIE = 1; // step 6: enable the CN interrupt
}

//Function to reset the camera
void camera_reset(){
	LATEbits.LATE9 = 0; //0
	LATEbits.LATE9 = 1; //1
}

void write_reset(){
	LATECLR = 0b100000000;
	delay();
	delay();
	delay();
}


// This is about a ms delay, used for I2C communication
void delay() {
	int j;
	for (j=0; j<1000; j++) {}
}
