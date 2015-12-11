//#define NU32_STANDALONE  // uncomment if program is standalone, not bootloaded
#include "NU32.h"          // config bits, constants, funcs for startup and UART
#include "NU32_I2C1.h"
// #include "i2c.h"

#define FULL_DUTY 3000 //About 10 MHz

#define WRST 0b1000000000	// PORTEbits.RE9
#define WR   0b100000000 	// PORTEbits.RE8

#define RRST 0b1	// PORTFbits.RF0
#define OE   0b10 	// PORTFbits.RF1

#define HREF PORTBbits.RB10
#define RCK PORTBbits.RB9 

//SIOC - A14 (SCL1) 
//SIOD - A15 (SDA1) 
//VSYNC - F5 (CN18)

/******************************************************************************
* HELPER FUNCTION PROTOTYPES
******************************************************************************/
void data_config();
void camera_config();
void delay();

void VSyncInterruptInitialize();
void reset_write_pointer();
void FIFO_write_enable();
void FIFO_write_disable();

void read_image();
void reset_read_pointer();
void FIFO_output_enable();
void FIFO_output_disable();
void rckInitialize();
void clock_on();
void clock_off();
char read_byte();

void display_image();

void wheel_one();


/******************************************************************************
* GLOBAL VARIABLES
*****************************************************************************/
char newF;
int count = 0;
int read_state = 0;

int num_rows = 144;
int num_columns = 174;
volatile unsigned char img_array[144][174];

volatile unsigned char c;



/**********************************************************************
* Interrupt(s)
*********************************************************************/
void __ISR(_CHANGE_NOTICE_VECTOR, IPL3SOFT) VSyncInterrupt(void) { // INT step 1
	newF = PORTF; // since pins on port F are being monitored by CN,
				  // must read both to allow continued functioning

	read_state = !read_state; //first VSync is beginning of frame 
								// and second Vsync is end of frame

	if(read_state){
		reset_write_pointer();
		FIFO_write_enable();
	}

	if(!read_state){
		FIFO_write_disable();
	}
	
	/*****/ 
	//Currently used to turn off CN pin after one full cycle
	count++;
	if(count ==2){
		// CNCONbits.ON = 0; // Temporarily turn CN pin off
		// NU32_WriteUART1("image stored \r\n");
		read_image();
		display_image();
	}

	if(count >= 3){} //do nothing
	/*****/

	IFS1bits.CNIF = 0; // clear the interrupt flag

}

/******************************************************************************
* MAIN FUNCTION
******************************************************************************/
int main(){

	char * value; 
	char value2;
	char msg2[20];

	unsigned char buffer;
	unsigned char data[100];

	NU32_Startup();

	__builtin_disable_interrupts();

	// I2Cinitialize(9600); //SCL1 and SDA1
	I2Cinitialize(SLOW_BAUD_RATE); //SCL1 and SDA1
	camera_config();
	VSyncInterruptInitialize();
	rckInitialize();
	data_config();

	wheel_one();

	__builtin_enable_interrupts();

	


	while(1){
		NU32_WriteUART1("Press 'l' to read a byte \r\n");
		NU32_WriteUART1("Press 'k' to reset \r\n");
		NU32_WriteUART1("Press 'p' to read i2c \r\n");
		NU32_ReadUART1(msg2, 5);             // get the response

		switch(msg2[0]){
    		case 'l':
    		{
    			buffer = read_byte();
    			sprintf(data, "%d",buffer);
    			NU32_WriteUART1(data);
    			break;
    		}
    		case 'k':
    		{
    			count = 0;
    			read_state = 0;
				VSyncInterruptInitialize(); 
    			break;
    		}
    		case 'p':
    		{
    			delay();
    			// delay();
    			// delay();
    			I2Cread(0x42, 0xc, value);
    			sprintf(data, "%d", value);
    			NU32_WriteUART1(data);
    			break;
    		}
    		case 'o':
    		{
    			read_image();
    			display_image();
    			break;
    		}
    		case 'r':
    		{

    		}
    	}
	}

}

/******************************************************************************
* HELPER FUNCTIONS
******************************************************************************/
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
	TRISFbits.TRISF0 = 0; //0 = output
	TRISFbits.TRISF1 = 0;
	LATESET = 0b110000000; //set pins 8 and 9 high
	LATFSET = 0b11; //set pin 0 and 1 high
	// LATFCLR = 0b10; //set pin 1 low
	// LATEbits.LATE3 = 1;   //1 sets output to floating
}

void camera_config(){
	int i;
	for(i=1;i<10000;i++){}
	//configure the register that slows down pclk period to xclk*32

	// I2Cwrite(0x42, 0x11, 0b10000011); //pclk is 32 times xclk period


	delay();
	I2Cstartevent();
	delay();
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

// This is about a ms delay, used for I2C communication
void delay() {
	int j;
	for (j=0; j<1000; j++) {}
}


//Image capture

void VSyncInterruptInitialize(){
	CNCONbits.ON = 1; // step 2: configure peripheral: turn on CN
	CNEN = (1<<18); // listen to CN18/RF5
	IPC6bits.CNIP = 3; // step 3: set interrupt priority
	IPC6bits.CNIS = 2; // step 4: set interrupt subpriority
	IFS1bits.CNIF = 0; // step 5: clear the interrupt flag
	IEC1bits.CNIE = 1; // step 6: enable the CN interrupt
}

void reset_write_pointer(){
	//Reset Write Pointer to 0 which is the beginning of the frame
	//default is high, set pin low to reset
	LATECLR = WRST; //set pin low
	LATESET = WRST; //set pin high
}

void FIFO_write_enable(){
	//Set FIFO write enable to active (high) so that image can be written to ram
	LATESET = WR;	
}

void FIFO_write_disable(){
	//Set FIFO write enable to inactive (low) so that image cannot be written to ram
	LATECLR = WR;	
}

//Reading image

void read_image(){
	reset_read_pointer();

	FIFO_output_enable();
	// clock_on();

	int rows = 0; 
	int cols = 0;
	volatile unsigned char pixel;

	while(rows < num_rows){
		if(HREF){
			while(HREF){
				if(RCK && cols <= num_columns){
					pixel = (int)read_byte();
					img_array[rows][cols] = pixel;
					cols++;
					while(RCK){
						{}; //do nothing, wait for next clock pulse
					}
				}
				cols = 0;
				rows++;
			}
		}
	}

	FIFO_output_disable();
	// clock_off();

}

void reset_read_pointer(){
	//Set the FIFO read buffer pointer to the start of the frame
	//default is high, set pin low to reset
	LATFCLR = RRST; //set pin low
	LATFSET = RRST; //set pin high
}

void FIFO_output_enable(){
	//Set FIFO output enable to active (low) so that image can be read
	LATFCLR = OE;	
}

void FIFO_output_disable(){
	//Set FIFO output enable to inactive (high) 
	LATFSET = OE;	
}

void rckInitialize(){
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

void clock_on(){
	OC1CONbits.ON = 1; // Turn on output compare 1
	NU32_WriteUART1("clock on \r\n");
}

void clock_off(){
	OC1CONbits.ON = 0;
	NU32_WriteUART1("clock off \r\n");
}

//read in D0 ­> D7 bits to an 8­bit variable pixel[idx]
char read_byte() {
	//=0x0; //c's default value is 0b0000;
	c = PORTB;
	return(c);
}

void display_image(){
	//Print out image
	int j;
	int i;
	unsigned char pixel;
	unsigned char msg[20];

	for(j=0; j<10; j++){
		for(i=0; i<10; i++){
			pixel = img_array[j][i];
			sprintf(msg, "%d", pixel);
			NU32_WriteUART1(msg);
			NU32_WriteUART1(" ");
		}
		NU32_WriteUART1("\r\n");
	}
}

void wheel_one(){
	T2CONbits.TCKPS = 4; // Timer 2 pre­scaler N = 1 (1:1), thus it ticks at 80 Mhz (PBCLK/N)
	PR2 = 9999; // This makes run at 80 Mhz / (N * (PR2+1)) == 10 MHz
	TMR2 = 0; // Set the initial timer count to 0
	OC2CONbits.OCM = 0b110; // PWM mode without the failsafe for OC1
	OC2RS = 5000; // Next duty duty cycle is 0
	OC2R = 5000; // Initial duty cycle of 0
	T2CONbits.ON = 1; // Turn on timer 3
	OC2CONbits.ON = 1; // Turn on output compare 1
}
