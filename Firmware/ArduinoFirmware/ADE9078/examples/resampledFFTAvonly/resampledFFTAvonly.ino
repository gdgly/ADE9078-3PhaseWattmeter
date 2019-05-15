//Resampled Wave form buffer for ADE9078 to read and report values (ADE9078_TEST)
//California Plug Load Research Center - 2019


#include <ADE9078.h>
#include <SPI.h>
#include <EEPROM.h>
#include "arduinoFFT.h"


//Architecture Control:
//Make sure you select in the ADE9078.h file the proper board architecture, either Arduino/AVR/ESP8266 or ESP32
//REMINDER: ONLY SELECT THE SINGLE OPTION FOR THE BOARD TYPE YOU ARE USING!


//Define ADE9078 object with hardware parameters specified
#define local_SPI_freq 115200  //Set SPI_Freq at 1MHz  - used for Arduino/AVR configuration
#define local_SS 10  //Set the SS pin for SPI communication as pin 10, typical on Arduino Uno and similar boards

//****WFB settings********
#define WFB_ALL_SEGMENTS 512
#define BURST_MEMORY_BASE 0x801//well, its supposed to be 0x800 but that doesn't work that well for some reason

arduinoFFT FFT = arduinoFFT();

#define SAMPLES 64             //FFT Total samples input - Must be a power of 2
unsigned int sampling_period_us;  //holder for microseconds for FFT
#define SAMPLING_FREQUENCY 60 //Hz, match rate to match sampling frequency for input data from data source

//Read, re-sample, process, report flow control
bool SampleBufferFilled = 0; //Used to indicate when the buffer has been filled and is ready for readout, ready to be read out
bool FFTCalculationComplete = 0; //Flow control for readout of values
bool FFTInputBufferFilled = 0; //Flow control for readout of values


//Declare object for buffer & FFT values to be received
struct FFTDataHolder  //this is the holder for the inputs and the FFT returns
	{
		double vRealPhaseAv[SAMPLES]; //holder for Real (frequency values): Phase A, Voltage
		double vImagPhaseAv[SAMPLES];  //holder for Img. (Phase values): Phase A, Voltage

		//double vRealPhaseAi[SAMPLES]; //holder for Real (frequency values): Phase A, Current
		//double vImagPhaseAi[SAMPLES];  //holder for Img. (Phase values): Phase A, Current
		//
		// double vRealPhaseBv[SAMPLES]; //holder for Real (frequency values): Phase B, Voltage
		// double vImagPhaseBv[SAMPLES];  //holder for Img. (Phase values): Phase B, Voltage
		//
		// double vRealPhaseBi[SAMPLES]; //holder for Real (frequency values): Phase B, Current
		// double vImagPhaseBi[SAMPLES];  //holder for Img. (Phase values): Phase B, Current
		//
		// double vRealPhaseCv[SAMPLES]; //holder for Real (frequency values): Phase C, Voltage
		// double vImagPhaseCv[SAMPLES];  //holder for Img. (Phase values): Phase C, Voltage
		//
		// double vRealPhaseCi[SAMPLES]; //holder for Real (frequency values): Phase C, Current
		// double vImagPhaseCi[SAMPLES];  //holder for Img. (Phase values): Phase C, Current
		//
		// double vRealPhaseNi[SAMPLES]; //holder for Real (frequency values): Neutral, Current
		// double vImagPhaseNi[SAMPLES];  //holder for Img. (Phase values): Neutral, Current
};

const int readCount = WFB_ALL_SEGMENTS/WFB_RESAMPLE_SEGMENTS; //==8

struct InitializationSettings* is = new InitializationSettings; //define structure for initialized values

ADE9078 myADE9078(local_SS, local_SPI_freq, is); // Call the ADE9078 Object with hardware parameters specified, local variables are copied to private variables inside the class when object is created.



void setup() {

	//Need to add:
	//1)Read in EEPROM values, check the "Configured bit", if 1, display, calibration values available, load these calibration values to the ADE9078's RAM
	//2)If 0, then display that no calibration values are present and load default values (Tell user to run calibration firmware)
	//3) Proceed with load and operation of this demo
  Serial.begin(115200);
  delay(200);
  is->vAGain=1;
  is->vBGain=1;
  is->vCGain=1;

  is->iAGain=1;
  is->iBGain=1;
  is->iCGain=1;
  is->iNGain=1;

  is->powerAGain=1;
  is->powerBGain=1;
  is->powerCGain=1;

  //Use these settings to configure wiring configuration at stertup
  //FYI: B010=2 in DEC
  //FYI: B100 = 4 in DEC

	//4 Wire Wye configuration - non-Blondel compliant:
	is->vConsel=0;
	is->iConsel=0;

	//Delta, Blondel compliant:
	//is->vConsel=4; //byte value of 100,
	//is->iConsel=0;


	 //Please continue for all cases

  SPI.begin();
  delay(200);
    myADE9078.initialize(); //Call initialization of the ADE9078 withe default configuration plus options specified
	//EEPROMInit()  //call only once on a virgin chip to "partition" EEPROM for the input type expected moving forward
	//load_data_allfields();  //load EEPROM values
  delay(200);

  sampling_period_us = round(1000000*(1.0/SAMPLING_FREQUENCY));  //calculate the sampling period in microseconds for the FFT, relative to 1 MHZ
}


void loop() {
  FFTDataHolder fftData;
  arduinoFFT AvFFT = arduinoFFT(fftData.vRealPhaseAv, fftData.vImagPhaseAv, SAMPLES, SAMPLING_FREQUENCY);

    //CONFIGURE WFB
  myADE9078.configureWFB();
  myADE9078.burstAvOnly();

  myADE9078.startFillingBuffer();
  //int wait = 0;//takes about 40-41ms to fill buffer
  bool check = 0;
  //Serial.println("check");
  while (check != 1){
  	delay(1);
  	// Serial.println(wait);
  	// wait++;
  	check = myADE9078.isDoneSampling();
    Serial.print("wait status: ");
    Serial.println(check);
  }

  myADE9078.stopFillingBuffer();

  //Serial.println("done sampling, start reading");


  for (int i=0; i < readCount; ++i)//readcount = 8
  {
      // Serial.println("Outer loop starting");

      uint16_t burstMemoryOffset = i* 4 * 64; // each segment is 16 bytes, we read in sets of 64
      uint16_t startingAddress = BURST_MEMORY_BASE + burstMemoryOffset;
      myADE9078.spiBurstResampledWFB_Avonly(startingAddress);

      // Serial.print("Outer Loop: ");
      // Serial.println(i);

      //memcpy
      for (int seg=0; seg < WFB_RESAMPLE_SEGMENTS; ++seg)
      {
        // int s = sizeof(FullResample);
        // Serial.print("Size of FullResample: ");
        // Serial.println(s); // should print 7168, and it is.

        int segOffSet = seg + (i*64);
        // Serial.print("Loop position: ");
        // Serial.print(i);
        // Serial.print(", ");
        // Serial.println(seg);
        // Serial.print("Segment Offset: ");
        // Serial.println(segOffSet);

        // myADE9078.readIrms();
        // Serial.print("A, B, C rms (I): ");
        // Serial.print(myADE9078.lastReads.irms.a);
        // Serial.print(" ");
        // Serial.print(myADE9078.lastReads.irms.b);
        // Serial.print(" ");
        // Serial.print(myADE9078.lastReads.irms.c);
        // Serial.print(" ");

        // Serial.print("Av\t");
        // Serial.println(myADE9078.lastReads.resampledData.Va[seg]); Serial.print(" ");

        fftData.vRealPhaseAv[seg] = myADE9078.lastReads.resampledData.Va[seg];
        fftData.vImagPhaseAv[seg] = 0;
      }
      AvFFT.Windowing(fftData.vRealPhaseAv, SAMPLES, FFT_WIN_TYP_HAMMING, FFT_FORWARD);
      AvFFT.Compute(fftData.vRealPhaseAv, fftData.vImagPhaseAv, SAMPLES, FFT_FORWARD);
      AvFFT.ComplexToMagnitude(fftData.vRealPhaseAv, fftData.vImagPhaseAv, SAMPLES);

      Serial.print("FFT:");
      Serial.print("Av:");
      for(int i=0; i<(SAMPLES/2); i++){
        //Serial.print((i * 1.0 * SAMPLING_FREQUENCY) / SAMPLES, 1);
        //Serial.print(",");
        Serial.println(fftData.vRealPhaseAv[i], 1);
        //Serial.print(";");
      }
      //Serial.println("$");
  }

  Serial.println("Finished reading from ADE chip.");

}