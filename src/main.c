#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <sys/wait.h>
#include <math.h>
int input_mode; 
#include <pthread.h>
#include <process.h>
#include <spawn.h>
#include <signal.h>
#include <stdbool.h>

uint16_t readpotentiometer1;
uint16_t readpotentiometer2;

#define	INTERRUPT		iobase[1] + 0				// Badr1 + 0 : also ADC register
#define	MUXCHAN			iobase[1] + 2				// Badr1 + 2
#define	TRIGGER			iobase[1] + 4				// Badr1 + 4
#define	AUTOCAL			iobase[1] + 6				// Badr1 + 6
#define 	DA_CTLREG		iobase[1] + 8				// Badr1 + 8

#define	 AD_DATA			iobase[2] + 0				// Badr2 + 0
#define	 AD_FIFOCLR		iobase[2] + 2				// Badr2 + 2

#define	TIMER0				iobase[3] + 0				// Badr3 + 0
#define	TIMER1				iobase[3] + 1				// Badr3 + 1
#define	TIMER2				iobase[3] + 2				// Badr3 + 2
#define	COUNTCTL			iobase[3] + 3				// Badr3 + 3
#define	DIO_PORTA		iobase[3] + 4				// Badr3 + 4
#define	DIO_PORTB		iobase[3] + 5				// Badr3 + 5
#define	DIO_PORTC		iobase[3] + 6				// Badr3 + 6
#define	DIO_CTLREG		iobase[3] + 7				// Badr3 + 7
#define	PACER1				iobase[3] + 8				// Badr3 + 8
#define	PACER2				iobase[3] + 9				// Badr3 + 9
#define	PACER3				iobase[3] + a				// Badr3 + 0xa
#define	PACERCTL			iobase[3] + b				// Badr3 + 0xb

#define 	DA_Data			iobase[4] + 0				// Badr4 + 0
#define	DA_FIFOCLR		iobase[4] + 2				// Badr4 + 2

int badr[5];
#define POINTS  151
struct pci_dev_info info;
void *hdl;
uintptr_t iobase[6];
uintptr_t dio_in;
uint16_t adc_in;

float amplitude =0.7;

unsigned int data;
unsigned int output[POINTS];
unsigned int collectData[POINTS];
float delta,dummy;

int state = 1;

uintptr_t dio_in;
uintptr_t dio_in2;

//#define POINTS  200
FILE *fp;
FILE *fp2;

uintptr_t dio_switch;
int switch0;
int switch0_prev;
unsigned int i, count;
unsigned short chan;

pid_t pid;

//Wave parameters
int f;      //frequency
float A;    //amplitude
float avg;  //average value of wave voltage
int D;      //duty cycle (only affects square wave)
int wave_type;  //1 square, 2 sawtooth, 3 triangular, 0 sine


void read_potentiometer()
{
 	//start ADC
    count=0x00;                     // Port 1 = 0x00 , Port 2 = 0x01 
    chan= ((count & 0x0f)<<4) | (0x0f & count);
    out16(MUXCHAN,0x0D00|chan);		  // Set channel	 - burst mode off.
    delay(1);											  // allow mux to settle
 	out16(AD_DATA,0);
 	while(!(in16(MUXCHAN)&0x4000));
 	readpotentiometer1 = in16(AD_DATA); //read potentiometer 1
 	
 	count=0x01;                     // Port 1 = 0x00 , Port 2 = 0x01 
    chan= ((count & 0x0f)<<4) | (0x0f & count);
    out16(MUXCHAN,0x0D00|chan);		  // Set channel	 - burst mode off.
    delay(1);											  // allow mux to settle
 	out16(AD_DATA,0);
 	while(!(in16(MUXCHAN)&0x4000));
 	readpotentiometer2 = in16(AD_DATA); //read potentiometer 2
 }

void update_LED()
{
    out8(DIO_PORTB,dio_switch); // visualize the four switch states to four LEDs
}

 // initialize variables
 uint8_t port_value ;
 uint8_t switch1_value;
 uint8_t switch1_prev = 0;
 uint8_t switch2_value;
 uint8_t switch2_prev = 0;
 int status;
 pid_t wait_pid;
 pid_t pid
;
 uint8_t read_port_value() {
    port_value = in8(DIO_PORTA);
    return port_value;
 }

void *hardware_input_thread(void *arg)
{
    pid_t pid
;
    printf("\n==================================================================\n");   
    printf("Now you can change the following parameters from hardware: \n");
    printf("Adjust potentiometer 1 at left side for changing Amplitude(A),\n");
    printf("Adjust potentiometer 2 at rightunsigned short chan;
 side for changing average\n");
    printf("Adjust the second right limit switch to 1  tp terminate this program\n");
    printf("------------------------------------------------------------------\n");
    printf("WAVE \t \t A \t avg \t f level \t Duty Cycle\n");
    printf("------------------------------------------------------------------\n");
    while(input_mode==1)
    {
        update_LED();   //visualize the four switch states to the four LEDs
        
        //Read from potentiometer 1 and 2
        read_potentiometer();

        //Convert potentiometer readings to amplitude and average and update the variables
        //maximum potentiometer reading= 32768
        A = readpotentiometer1*1.0/32768 * 5;           //map 0 to 32768 --> 0 to 5 volt   
        avg = readpotentiometer2*1.0/32768 * 5 - 5;    //map 0 to 32768 --> -5 to 5 volt 
        
       
        //print the wave parameters    
        printf("%d (%s) \t %.2f \t %.2f \t %d \t\t %d \n", wave_type,wave_type==1?"square":(wave_type==2?"sawtooth":(wave_type==3?"triangular":"sine")),A,avg,f,D);
        
        delay(1000);   
       
            // read input value of the port
        port_value = in8(DIO_PORTA);

            // isolate value of switch 1 (second LSB)
        switch1_value = (port_value >> 1) & 0x01;

            // check if value of switch 1 has changed
        if (switch1_value != switch1_prev) {
                // kill the program if value of switch 1 has changed
              
            printf("Program Terminated\n");
            printf("Thank You For Using\n");
            exit(0);

         // update switch1_prev to current value of switch 1
            switch1_prev = switch1_value;
            
             // add a delay to reduce CPU usage
            delay(10); // adjust the delay as needed
            }
            
               	port_value = read_port_value();
                switch2_value = (port_value >> 2) & 0x01;

                // check if value of switch 2 has changed
                if (switch2_value != 0) {
                    printf("Function 5 activated\n");

                    // Create a child process without waiting for it to finish
                    pid = spawnl(P_NOWAIT, "/home/guest/f5", "/home/guest/f5", NULL);
                    if (pid < 0) {
                        perror("spawnl failed");
                        exit(EXIT_FAILURE);   
                    }
                        
                    // Monitor the child process
                    
                    do {
                        // Update the value of switch2_value and switch2_prev
                        port_value = read_port_value();
                        // switch2_prev = switch2_value;
                        switch2_value = (port_value >> 2) & 0x01;

                        // If switch2_value is equal to switch2_prev, terminate the child process
                        if (switch2_value == 0) {
                            kill(pid, SIGTERM);
                            printf("\n");
                        }

                    // Check the status of the child process without blocking
                    wait_pid = waitpid(pid, &status, WNOHANG);

                    // Add a small sleep to avoid excessive CPU usage
                    usleep(10000); // 10 ms
                    } while (wait_pid == 0);   
                } 
                // Add a small sleep to avoid excessive CPU usage
                usleep(10000); // 10 ms 
                
                    
                        
                            

                        
                        

                
    }
}
            
    
        
int s;  //scanf return value

int mode_;
float A_dummy, avg_dummy;
int f_dummy, D_dummy, wave_type_dummy;

void input_A()
{
    //input A
    printf("mode 1: Input amplitude: \n");
    
    s=scanf("%f",&A_dummy);
    while ((getchar()) != '\n');    //flush the scanf buffer. This is especially useful when the argument is not of the correct type
    
    if(s==0)    //re-ask user if input is not of type float
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid argument. Your input is not of float type.\n");
        printf("*******************************************************\n");
        input_A(); 
    }
    //check if Amplitude is valid
    else if(A_dummy>5 || A_dummy<0)  //specify the invalid range of valid A
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid A\n");//change error message
        printf("A must be between 0 and 5\n");
        printf("*******************************************************\n");
        input_A(); 
    }   
    else    //valid
    {
        A=A_dummy;
    }
}

void input_f()
{
    //input f
    printf("mode 2: Input frequency: \n");
    
    s=scanf("%d",&f_dummy);
    while ((getchar()) != '\n');    //flush the scanf buffer. This is especially useful when the argument is not of the correct type
    
    if(s==0)    //re-ask user if input is not of type int
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid argument. Your input is not an integer.\n");
        printf("*******************************************************\n");
        input_f(); 
    }
    //check if frequency is valid
    else if(f_dummy!=1 && f_dummy!=2 && f_dummy!=3 && f_dummy!=4 && f_dummy!=5)
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid f\n");
        printf("Choose one level of f (1,2,3,4,5)\n");
        printf("*******************************************************\n");
        input_f(); 
    } 
    else    //valid
    {
        f=f_dummy;
    }
}

void input_avg()
{
    //input avg
    printf("mode 3: Input average: \n");
    
    s=scanf("%f",&avg_dummy);
    while ((getchar()) != '\n');    //flush the scanf buffer. This is especially useful when the argument is not of the correct type
    
    if(s==0)    //re-ask user if input is not of type float
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid argument. Your input is not of float type.\n");
        printf("*******************************************************\n");
        input_avg(); 
    }
    //check if average is valid
    else if(avg_dummy<-5 || avg_dummy>5)  
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid avg\n");
        printf("avg must be between -5 and 5\n");
        printf("*******************************************************\n");
        input_avg(); 
    }   
    else    //valid
    {
        avg=avg_dummy;
    }
}
void input_D()
{
    //input D
    printf("mode 4: Input duty cycle(Only for square wave): \n");
    
    s=scanf("%d",&D_dummy);
    while ((getchar()) != '\n');    //flush the scanf buffer. This is especially useful when the argument is not of the correct type
    
    if(s==0)    //re-ask user if input is not of type int
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid argument. Your input is not an integer.\n");
        printf("*******************************************************\n");
        input_D(); 
    }
    //check if duty cycle is valid
    else if(D_dummy<0 || D_dummy>100) 
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid D\n");
        printf("D must be between 0 and 100\n");
        printf("*******************************************************\n");
        input_D(); 
    }
    else    //valid
    {
        D=D_dummy;
    }
}

void input_wave_type()
{
    //input wave type
    printf("Input wave type (0 for sine, 1 for square, 2 for sawtooth, 3 for triangular): \n");// sine(1), square(2), tri(3), 
    
    s=scanf("%d",&wave_type_dummy);
    while ((getchar()) != '\n');    //flush the scanf buffer. This is especially useful when the argument is not of the correct type
    
    if(s==0)    //re-ask user if input is not of type int
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid argument. Your input is not an integer.\n");
        printf("*******************************************************\n");
        input_wave_type(); 
    }
    //check if wave type is valid
    else if(wave_type_dummy!=1 && wave_type_dummy!=2 && wave_type_dummy!=3 && wave_type_dummy !=0)
    {
        printf("\n*******************************************************\n");
        printf("ERR: Invalid input\n");
        printf("Input 0 for sine, 1 for square, 2 for sawtooth, 3 for triangular\n");
        printf("*******************************************************\n");
        input_wave_type(); 
    }   
    else    //valid
    {
        wave_type=wave_type_dummy;
    }
}

void keyboard()
{
        printf("\n");
        printf("==================================================================\n");
        printf("A\t\t:%f\n", A);
        printf("avg\t\t:%f\n", avg);
        printf("wave type\t:%d (%s)\n", wave_type, wave_type == 0 ? "sine" : (wave_type == 1 ? "square" : (wave_type == 2 ? "sawtooth" : "triangle")));
        printf("Duty cycle\t:%d\n",D);
        printf("f level\t\t:%d\n",f);
        printf("------------------------------------------------------------------\n");
        printf("0: Terminate program\n");
        printf("1: Change amplitude (A): 0 to 5 \n");
        printf("2: Change frequency level (f): 1, 2, 3, 4, 5\n");
        printf("3: Change average (avg): -5 to 5\n");
        printf("4: Change duty cycle (D) for square wave: 0 to 100\n");
        printf("5: Change wave type: 1 for square, 2 for sawtooth, 3 for triangular \n");
        printf("Enter selection: \n");
        s=scanf("%d",&mode_);
        while ((getchar()) != '\n');    //flush the scanf buffer. This is especially useful when the argument is not of correct type
    
        if(s==0)    ///re-ask user if input is not of type integer
        {   
            printf("\n*******************************************************\n");
            printf("ERR: Invalid argument. Your input is not an integer.\n");
            printf("*******************************************************\n");
            keyboard();
        } 
        else        //check if input is valid (0,1,2,3,4,5)
        {
            switch(mode_){
                case 1:
                    input_A();
                    break;
                case 2:
                    input_f();
                    break;
                case 3:
                    input_avg();
                    break;
                case 4:
                    input_D();
                    break;
                case 5:
                    input_wave_type();
                    break;
                case 0:
                    raise(SIGINT);
                    //exit program
                    break;
                default:    //invalid input
                    printf("\n*******************************************************\n");
                    printf("ERR: Invalid argument. Please enter 0, 1, 2, 3, 4, or 5 \n");
                    printf("*******************************************************\n");
                    keyboard();      //ask the user to reenter mode
            }
        }
}

void *keyboard_input_thread(void *arg)
{
     while(input_mode==0)
     {
        keyboard();
     }
}
//thread ID
pthread_t keyboard_input_thread_ID;
pthread_t hardware_input_thread_ID;

/**
    function to convert raw reading of digital output into reading of switch0
    
    @param arg
    @return reading of switch0 (either 0 or 1)
*/
int switch0_value(int switch_value)
{
	return switch_value%2;
}

void *switch_input_mode_thread(void *arg)
{
    //initialize variables
	dio_switch = in8(DIO_PORTA);
	switch0=switch0_value(dio_switch);
    switch0_prev=switch0;
    input_mode=switch0;
    if(input_mode==0)
    {
        pthread_create( &keyboard_input_thread_ID, NULL, &keyboard_input_thread, NULL );   //keyboard_input_thread
    }
    else if(input_mode==1)
    {
        pthread_create( NULL, NULL, &hardware_input_thread, NULL );   //hardware_input_thread
    }
    
    while(1)
    {
		
       //read SWITCH
		dio_switch = in8(DIO_PORTA);
		switch0=switch0_value(dio_switch);
		out8(DIO_PORTB,dio_switch); //update LED light

		if (switch0!=switch0_prev)
        {
            //DEBOUNCING
			delay(1);
			//read SWITCH0 again
			dio_switch = in8(DIO_PORTA);
			if(switch0_value(dio_switch)==switch0)
			{
				//change detected from 0 to 1
            	if(switch0==0)	//0=keyboard input
            	{
            	    
               		//input from keyboard
               		input_mode=0;
                	
                	delay(1000);
                	
					//kill hardware_input thread, spawn keyboard_input thread

					pthread_create(&keyboard_input_thread_ID, NULL, &keyboard_input_thread, NULL );   //keyboard_input_thread
					
           		}
				//change detected from 1 to 0
           		else if(switch0==1)	//1=hardware input
           		{
               	    //input from hardware
               	    input_mode=1;
               	    
               	    delay(1000);
               	    
				    //keyboard_input thread terminates itself, spawn hardware_input thread
				    pthread_cancel(keyboard_input_thread_ID);
				    pthread_create( &hardware_input_thread_ID, NULL, &hardware_input_thread, NULL );   //hardware_input_thread
           		}
            //update switch0_prev 
            switch0_prev = switch0;
 
			}     
        }
    }
}

void initialization()
{

    printf("Program Activated! Welcome!");
    


    memset(&info,0,sizeof(info));
    if(pci_attach(0)<0) {
        perror("pci_attach");
        exit(EXIT_FAILURE);
    }
    
	/* Vendor and Device ID */
    info.VendorId=0x1307;
    info.DeviceId=0x01;

    if ((hdl=pci_attach_device(0, PCI_SHARE|PCI_INIT_ALL, 0, &info))==0) {
        perror("pci_attach_device");
        exit(EXIT_FAILURE);
    }
  	// Determine assigned BADRn IO addresses for PCI-DAS1602
    for(i=0;i<5;i++) {
        badr[i]=PCI_IO_ADDR(info.CpuBaseAddress[i]);
    }

    for(i=0;i<5;i++) {  // expect CpuBaseAddress to be the same as iobase for PC
        iobase[i]=mmap_device_io(0x0f,badr[i]);
    }
	// Modify thread control privity
    if(ThreadCtl(_NTO_TCTL_IO,0)==-1) {
        perror("Thread Control");
        exit(1);
    }
}
void initialize_DIO()
{
	out8(DIO_CTLREG,0x90);		//set portA (switch) as Digital input, portB (LED) as Digital output
}
void Initialize_ADC()
{
 	out16(TRIGGER,0x2081);  //set trigger control
 	out16(AUTOCAL,0x007f);  //set calibration register
 	out16(AD_FIFOCLR,0);    //clear ADC FIFO
 	out16(MUXCHAN,0x0C10);  //select the channel
}

unsigned int i;
unsigned int data;

int N=100;
int t_delay;
int f_to_t(int f_level)
{
    switch(f_level)
    {
        case(1):
            return 250;
        case(2):
            return 200;
        case(3):
            return 150;
        case(4):
            return 100;
        case(5):
            return 50;
    }
};
void sine_wave()
{
    while(wave_type==0)         //stops if wave_type is not 0 (sine)
    {
        for(i=0;i<N;i++) 
        {
            t_delay=f_to_t(f);
            data=( ( ( sinf((float)(i*2*3.1415/N))*A ) + avg+10)/10.0  * 0x7999 );
	        out16(DA_CTLREG,0x0523);			// DA Enable, #0, #1, SW 10V bipolar		2/6
   	        out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
            out16(DA_Data,(short) data);  
            delay(t_delay);
																																																	
      	 }
    }
}

void square_wave()
{
    while(wave_type==1)     //stops if wave_type is not 1 (square)
    {
            t_delay=f_to_t(f);
            data=( (avg+A+10) /10.0  * 0x7999 );
	        out16(DA_CTLREG,0x0523);			    // DA Enable, #0, #1, SW 10V bipolar		2/6
   	        out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
            out16(DA_Data,(short) data);
            delay((int)N*t_delay*D/100);
            
            data=( (avg-A+10) /10.0  * 0x7999 );
            out16(DA_CTLREG,0x0523);			    // DA Enable, #0, #1, SW 10V bipolar		2/6
   	        out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
            out16(DA_Data,(short) data);
            delay((int)N*t_delay*(100-D)/100);

    }
}

void sawtooth_wave()
{
    while(wave_type==2)     //stops if wave_type is not 2 (sawtooth)
    {
        t_delay=f_to_t(f);

        for(i=0;i<N;i++) 
        {
            data=(( avg-A+ A*2*i/N + 10)/10.0  * 0x7999);
	        out16(DA_CTLREG,0x0523);			    // DA Enable, #0, #1, SW 10V bipolar		
   	        out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
            out16(DA_Data,(short) data);
			delay(t_delay);		//micros																								
																																
  	    }
    }
}

void triangular_wave()
{
    while(wave_type==3)     //stops if wave_type is not 3 (triangular)
    {
        t_delay=f_to_t(f);

        for(i=0;i<N/2;i++) 
        {
            data=( ( avg-A+ A*4*i/N + 10)/10.0  * 0x7999 );
	        out16(DA_CTLREG,0x0523);			    // DA Enable, #0, #1, SW 10V bipolar		
   	        out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
            out16(DA_Data,(short) data);
			delay(t_delay);		//micros																								
																																
  	    }
  	    for(i=0;i<N/2;i++) 
        {
            data=( ( avg+A- A*4*i/N + 10)/10.0  * 0x7999 );
	        out16(DA_CTLREG,0x0523);			    // DA Enable, #0, #1, SW 10V bipolar
   	        out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
            out16(DA_Data,(short) data);
			delay(t_delay);		//micros																								
																																
  	    }
    }
}

void *generate_wave_thread(void *arg)
{   
    while(1)
    {
        switch(wave_type)
        {
            case(1):    //square
                square_wave();
                break;
            case(2):    //sawtooth
                sawtooth_wave();
                break;
            case(3):    //triangular
                triangular_wave();
                break;
            case(0):    //sine
                sine_wave();
                break;
            default:
                printf("invalid wave type error\n");
                break;
        }
    } 
}
int j;
char colon=':';
char argument;
char* argument_value;
//variable for file logging
FILE *fp;
//variables for finding the duration that the program runs
struct timespec start,stop;
double period;
void signal_handler( int signum) 
{
    //get the time when the program stops
    if(clock_gettime(CLOCK_REALTIME,&stop)==-1)
    { 
        printf("clock gettime stop error");
    }
    //compute duration that program has run
    period=(double)(stop.tv_sec-start.tv_sec)+ (double)(stop.tv_nsec- start.tv_nsec)/1000000000;
    ///create/open log.txt for logging & log exit message and duration that the program has run
    fp = fopen("log.txt","a");
    fprintf(fp,"Ending program \n");
    fprintf(fp,"Program runs for %lf seconds \n\n",period);
    fclose(fp);
    pci_detach_device(hdl);
    system("/usr/bin/clear");                           //clear screen
    printf("Program Terminated\n");         //exit message
    printf("Thank You For Using\n");
   
    exit(EXIT_SUCCESS);                                 //exit the program
}
int main(int argc, char * argv[])
{
    //attach signal_handler to catch SIGINT
    signal(SIGINT, signal_handler);
    f=1;            //frequency mode level
    A=2.0;          //ampltude
    avg=0;          //average
    D=50;           //duty cycle for square wave
    wave_type=0;    //sine
    for(j=1;j<argc;j++)
    {
        //find colon
        if(argv[j][1]!=colon)   //invalid argument: colon not found at second character
        {
            printf("ERROR: Invalid command line argument\n");
            printf("Command line argument should be as:\n");
            printf("./main t:wave_type A:A_value m:mean_value f:frequency mode choose one level(1, 2, 3, 4, 5) D:duty_cyctle_value\n");
            printf("*******************************************************\n");
            return 0;   //invalid, exit program
        }
        //get the argument ( A or avg or f or D or wave type)
        argument=argv[j][0];
        //get the VALUE of the argument
        argument_value=&(argv[j][2]);
        switch (argument)
        {
            case('A'):
                if(sscanf(argument_value,"%f",&A)!=1)   //parse A value and check whether it is of correct data type
                {
                    printf("\n*******************************************************\n");
                    printf("ERROR: A must be FLOAT\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                }
                else if(A>5 || A<0)                     //check if Amplitude is in valid range (0 to 5)
                {
                    printf("\n*******************************************************\n");
                    printf("ERR: Invalid A\n");
                    printf("A must be between 0 and 5\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                }  
                break;
            case('m'):                                  //parse average/mean value and check whether it is of correct data type
                if(sscanf(argument_value,"%f",&avg)!=1)
                {
                    printf("\n*******************************************************\n");
                    printf("ERR: mean must be FLOAT\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                } 
                else if(avg<-5 || avg>5)                //check if average is in valid range (-5 to 5)
                {
                    printf("\n*******************************************************\n");
                    printf("ERR: Invalid avg\n");
                    printf("avg must be between -5 and 5\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                }    
                break;
            case('f'):                                  //parse f level value and check whether it is of correct data type
                if(sscanf(argument_value,"%d",&f)!=1)
                {
                    printf("\n*******************************************************\n");
                    printf("ERR: frequency must be INT\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                }
                else if(f!=1 && f!=2 && f!=3 && f!=4 && f!=5)   //check if frequency is valid (level 1,2,3,4,5)
                {
                    printf("\n*******************************************************\n");
                    printf("ERR: Invalid f\n");
                    printf("Choose one level of f (1,2,3,4,5)\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                } 
                break;
            case('D'):
                if(sscanf(argument_value,"%d",&D)!=1)   //parse D value and check whether it is of correct data type
                {
                    printf("\n*******************************************************\n");
                    printf("ERROR: Duty cycle must be INT\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                }
                else if(D<0 || D>100)                           //check if duty cycle is in valid range(0 to 100)
                {
                    printf("\n*******************************************************\n");
                    printf("ERR: Invalid D\n");
                    printf("D must be between 0 and 100\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                } 
                break;
            case('t'):  
                if(sscanf(argument_value,"%d",&wave_type)!=1)   //parse wave type and check whether it is of correct data type
                {
                    printf("\n*******************************************************\n");
                    printf("ERR: wave type must be INT (0,1,2,3)\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                }
            else if(wave_type!=1 && wave_type!=2 && wave_type!=3 && wave_type!=0)   //check if wave type value is valid
                {
                    printf("\n*******************************************************\n");
                    printf("ERR: Invalid input\n");
                    printf("Input 0 for sine wave, 1 for square wave, 2 for sawtooth wave, 3 for triangular wave\n");
                    printf("*******************************************************\n");
                    return 0;   //invalid, exit program
                }  
                break;
            default:    //invalid
                printf("\n*******************************************************\n");
                printf("ERR: Invalid command line argument\n");
                printf("Command line argument should be as:\n");
                printf("./main t:wave_type A:A_value m:mean_value f:frequency level D:duty_cyctle_value\n");
                printf("*******************************************************\n");
                return 0;   //invalid, exit program
                break;
        }
    }
    initialization();   //initialize hardware
    initialize_DIO();   //initialize Digital Input Output
    Initialize_ADC();   //initialize ADC
    if(clock_gettime(CLOCK_REALTIME,&start)==-1)
    { 
        printf("clock gettime start error");
    } 
    fp = fopen("log.txt","a");
    fprintf(fp,"Starting program\n");
    fclose(fp);
    pthread_create( NULL, NULL, &switch_input_mode_thread, NULL );
    //create generate_wave_thread   
    pthread_create( NULL, NULL, &generate_wave_thread, NULL );        
 	while(1)
 	{}
  	printf("\n\nExit Program\n");
    pci_detach_device(hdl);
    return 0;
}