#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <hw/pci.h>
#include <hw/inout.h>
#include <sys/neutrino.h>
#include <sys/mman.h>
#include <math.h>
#include <pthread.h>
#include <process.h>
																
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
#define	PACER3				iobase[3] + a				// Badr3 + a
#define	PACERCTL			iobase[3] + b				// Badr3 + b

#define 	DA_Data			iobase[4] + 0				// Badr4 + 0
#define	DA_FIFOCLR		iobase[4] + 2				// Badr4 + 2
	
int badr[5];															// PCI 2.2 assigns 6 IO base addresses

//define the number of points sample for copying into file 
#define POINTS  151
//#define POINTS 1000

struct pci_dev_info info;
void *hdl;

uintptr_t iobase[6];
uintptr_t dio_in;
uint16_t adc_in;

unsigned int i,count,j;
unsigned short chan;
float amplitude =0.7;

unsigned int data;
unsigned int output[POINTS];
unsigned int collectData[POINTS];
float delta,dummy;

int state = 1;

uintptr_t dio_in;
uintptr_t dio_in2;

// initialize two FILE pointers
FILE *fp;
FILE *fp2;

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
// thread to read potentiometer value, only taking in the value from the potentiometer port 1 ( A/D 0 )
void * measureAnalog(void *arg)
 {
    // Allow IO functions access to this thread 
    ThreadCtl(_NTO_TCTL_IO,0);
    while(1){
      // Set up the potentiometer & clear buffer 
      out16(INTERRUPT,0x60c0);				// sets interrupts	 - Clears			
      out16(TRIGGER,0x2081);					// sets trigger control: 10MHz, clear, Burst off,SW trig. default:20a0
      out16(AUTOCAL,0x007f);					// sets automatic calibration : default

      out16(AD_FIFOCLR,0); 						// clear ADC buffer
      out16(MUXCHAN,0x0D00);				  // Write to MUX register - SW trigger, UP, SE, 5v, ch 0-0 	
                                      // x x 0 0 | 1  0  0 1  | 0x 7   0 | Diff - 8 channels
                                      // SW trig |Diff-Uni 5v| scan 0-7| Single - 16 channels

      // Start Port 1 potentiometer & read from the potentiometer
      count=0x00;                     // Port 1 = 0x00 , Port 2 = 0x01 
      chan= ((count & 0x0f)<<4) | (0x0f & count);
      out16(MUXCHAN,0x0D00|chan);		  // Set channel	 - burst mode off.
      delay(1);											  // allow mux to settle
      out16(AD_DATA,0); 							// start ADC 
      while(!(in16(MUXCHAN) & 0x4000));
      adc_in=in16(AD_DATA);   
      // Scale the data value from 0 - 65535 to 0 - 2
      amplitude = (float)adc_in/0x7FFF;

      // Print value from scale -10.0 to 10.0 	
      //printf( "\rData Value: %6.5f ", (((float)adc_in/0x7FFF)-1)*10);									
      fflush( stdout );
      delay(5);											
        
    }
}

// thread that checks the toggle switch/Digital IO to check if there is a change in pattern
// If there is a change in pattern, the process will be terminated 
void * checkDigital(void *arg)
 {  
    ThreadCtl(_NTO_TCTL_IO,0);
    while(1){
      out8(DIO_CTLREG,0x90);					// Port A : Input,  Port B : Output,  Port C (upper | lower) : Output | Output			
      dio_in=in8(DIO_PORTA); 					// Read Port A	
      dio_in2=in8(DIO_PORTA); 					// Read Port A	
      if(dio_in!=dio_in2){
        exit(0);
      }                                        
    }
}

// Output the points to D/A 1 and stored it in an unsigned integer array 
// ONLY D/A 1 is used , D/A 0 is used for main, this is done to prevent output interference that causes 'bleeding'
void generate_wave(int i) {
	
      dummy=(amplitude  * 0x7fff) ;				
    	
  	  data= (unsigned) dummy;			// add offset +  scale
      output[i] = (unsigned) dummy;
      printf( "\rData Value: %6.5f ", (((float)output[i]/0x7FFF)-1)*10);

	  // ONLY USE D/A 1 For function 5 , D/A 0 is used for main
	    out16(DA_CTLREG,0x0443);			// DA Enable, #1, #0, SW 10V bipolar	
      out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
      out16(DA_Data,(short)  data);	
      delay(5);
  
}

//Function that writes the unsigned integer elements in the array to 2 files 
//The first file is for D/A output to an oscilloscope. Data is written in a scale of 0 - 65535
//The second file is a user/human readable file and is written in a scale -10V t0 10V 
void write_file(){

	if((fp=fopen("wave_input","w"))==NULL){
		perror("cannot open");
		exit(1);
	}
	if((fp2=fopen("readamp.c","w"))==NULL){
		perror("cannot open");
		exit(1);
	}

	for(i=1;i<POINTS;i++){
		fprintf(fp2,"%6.5fV \n", (((float)output[i]/0x7FFF)-1)*10);
		fprintf(fp,"%d \n",output[i]);
 	}
	fclose(fp);
	fclose(fp2);

}
 
// The function reads the values from the first file and stored its values in an unsigned integer array
void read_file(){
  int j;
  char line[100];
  if ((fp=fopen("wave_input","r"))==NULL){
	perror("cannot open");
	exit(1);
	}
    j = 1;
    while (fgets(line, sizeof(line), fp)) {
        sscanf(line, "%d", &collectData[j]);
        j++;
  }
  fclose(fp);
}

// The function generates a wave based on the data stored in an unsigned integer array
// ONLY D/A 1 is used , D/A 0 is used for main, this is done to prevent output interference that causes 'bleeding'
void generate_file_wave(int i) {
	 
	  // ONLY USE D/A PORT 1, D/A PORT 0 for main
	    out16(DA_CTLREG,0x0443);			// DA Enable, #1, #0, SW 10V biipolar
      out16(DA_FIFOCLR, 0);					// Clear DA FIFO  buffer
      out16(DA_Data,(short)  collectData[i]);	
      delay(5);
  
}

//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

int main() {

// Set up the PCI board for interfacing with the software programme
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
 			
for(i=0;i<5;i++) {												// expect CpuBaseAddress to be the same as iobase for PC
  iobase[i]=mmap_device_io(0x0f,badr[i]);	
  }													
																		
if(ThreadCtl(_NTO_TCTL_IO,0)==-1) {// Modify thread control privity
  perror("Thread Control");
  exit(1);
  }				
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++  

// Initialize and start the thread of measureAnalog and checkDigital
pthread_create( NULL, NULL, &measureAnalog, NULL );
pthread_create( NULL, NULL, &checkDigital, NULL );

while(state){

// Allows the user to generate an arbitrary wave on the oscilloscope by changing the potentiometer port 0
// For 151 points, stored each points read from the potentiometer port 0 in an unsigned integer array 
// and output the points onto an oscilloscope through D/A port 1
printf( "Writing Phase\n");		
for(i=1 ; i<POINTS ;i++){
  generate_wave(i);
}
printf("\n");
// Write the collected points in the unsigned integer array to 2 files, overwrite the files if it exist, else create new files
// 1st file is for user to read, 2nd file is to be used to generate the arbitrary wave on the oscillscope
write_file();
//delay(500);

printf( "Reading phase\n");	
// Read the data from the 2nd file and stored it into an unsigned integer array
read_file();

//Generate the wave pattern read from the 2nd file onto the oscilloscope by reading the data stored in the unsigned integer array 
//and writing it to D/A 1 to be output onto the oscilloscope 
//The generated wave is repeated 3 times to lengthen the process 	
for(j =1 ; j<4; j++){
	for(i=1 ; i<POINTS ;i++){
  		generate_file_wave(i);
	}
}
printf( "Finish process, please wait for 1s\n");	
//Delay the process for 1s before allowing the user to generate the arbitrary wave again
 delay(1000);
}

exit (0);

}



