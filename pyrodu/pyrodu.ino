// !00.#100.@100.
#include 		<avr/pgmspace.h>       // For stashing stuff in flash memory
// #include <SD.h>
// #include <ArduinoStream.h>
#include    <SdFat.h> // to avoid the sdvolume stuff use version http://sdfatlib.googlecode.com/files/sdfatlib20130629.zip
#include 		<SdFatUtil.h>
#include     <SdVolume.h>

// Conversion for old pyrosphere model with sparkfun SD reader.
#define VERSION 2

// Pin values for talking to shift registers
#define 		DATA_PIN1 								7 // Data pin for serial communication to shift registers
#define 		LATCH_PIN1 								2 // Latch pin for serial communication to shift registers
#define 		CLOCK_PIN1 								6 // Clock pin for serial communication to shift registers

// #define 		DATA_PIN2 								13 // Data pin for serial communication to shift registers
// #define 		LATCH_PIN2 								8 // Latch pin for serial communication to shift registers
// #define 		CLOCK_PIN2 								12 // Clock pin for serial communication to shift registers


// General Definitions            		
#define 		NUM_REGISTERS 					12 // Total registers (chips, bytes) being talked to
#define 		LINE1											47 // Total chips in the first serial chain.
#define 		LINE2											44 // Total chips in the second serial chain.
#define 		TOTAL_NODES 							96 // 0 -  85 makes 86 nodes
#define 		FILE_NAME_SIZE 						12 // 8.3 filenames need 12 char to define them
                                  		
// Interval Limits                		
#define 		MIN_FRAME_INTERVAL 				 35
#define 		DEFAULT_FRAME_INTERVAL		 100 /// Final Decision? What should this be?
#define			MAX_FRAME_INTERVAL 				 10000
// Duration Limits                		    
#define 		MIN_FRAME_DURATION 				 10
#define 		DEFAULT_FRAME_DURATION		 75 /// Final Decision? What should this be?
#define 		MAX_FRAME_DURATION 				 750

//Holds the current frame? ...Don't think this is used anywhere. 
typedef struct _frame {
  int8_t frameChunk[NUM_REGISTERS];
} Frame;
//Some curious looking code without an explaination. Magic.
#define 		nodeOnMacro( FRAME, FLAMENUM )  ( (FRAME).frameChunk[((uint8_t)(FLAMENUM)) >> 3] |= _BV((FLAMENUM & 0x07 ) ) )
#define 		nodeOffMacro( FRAME, FLAMENUM ) ( (FRAME).frameChunk[((uint8_t)(FLAMENUM)) >> 3] &= ~_BV((FLAMENUM & 0x07 ) ) )
#define 		isFlameOn( FRAME, FLAMENUM ) ( (FRAME).frameChunk[((uint8_t)(FLAMENUM)) >> 3] & _BV((FLAMENUM & 0x07 ) ) )
// For SD library interfacing
Sd2Card 		card;
SdVolume 		volume;
SdFile 			root;
SdFile 			animation;

uint8_t	 		partition 						= 1;
uint8_t 		chipSelect 						= 8; // for Sparkfun	4 for Ethernet
dir_t 			directory;

int 				totalFiles;
//
// char 				currentFile[FILE_NAME_SIZE]; 						 // The current animation file we're on, assumining 8+3 filename
char* 			currentPattern; // Place to store the pattern name for output.
char 				currentFile[13];

//Runtime Variables
long 				nodeTimeStamps[TOTAL_NODES]; 						 // Since defining arrays requires you put in the total number of elements, add 1
long 				nodeDurations[TOTAL_NODES]; 						 // Array for disparaging durations
long 				frameInterval 				= DEFAULT_FRAME_INTERVAL;             // Interval between frames
long 				frameDuration 				=	DEFAULT_FRAME_DURATION;              // Time a given  is on 

//Chillout mode
// bool 				chilloutMode 					= false;
// long 				chilloutInterval 			= 60*60*3; 					//Every 3 hours. //milliseconds until next chillout.
// long 				chilloutDuration 			= 60*5; 						//5 Minutes.
// long 				chilloutFrameInterval = 3000; 						//This is how long in between each frame in Chillout Mode
// long 				chilloutFrameDuration = 500; 							//The duration of the flame.

// Control Mode
int 				controlMode 					= 0; 								//default: Auto Random; /// Decide where we start.
//Frame
Frame 			frameBuffer;
char 				messageBuffer[8];  												// Can't forsee more than 8c*s as long as we stay away from long pattern titles.
//
int 				bufferIndex 					= 0; 								//This global manages the buffer index.
//
boolean 		active								= true; 						// Are we even going there or not? ON/OFF switch.
boolean 		status								= false; 						//TODO: Add the capability to enable and disable debugging remotely! See 'toggledebug'
boolean 		debug 								= true;							// Where the frame is updated until we're ready to send the data to the shift registers
boolean 		verbose 							= false;						// Show all the schtuff!

// This array maps a node number to a register and bit. It won't change during the course of the program
// PROTOSPHERE
// const 			prog_int8_t mappingArray_P[TOTAL_NODES] PROGMEM = { 31, 74, 29, 90, 37, 33, 44, 34, 67, 76, 80, 30, 93, 75, 20, 35, 21, 65, 41, 42, 59, 69, 58, 85, 52, 53, 95, 89, 79, 39, 19, 87, 8, 22, 46, 32, 70, 66, 61, 18, 36, 86, 83, 77, 73, 84, 28, 94, 9, 11, 55, 68, 57, 63, 60, 26, 38, 43, 15, 12, 91, 72, 88, 16, 92, 64, 47, 51, 81, 71, 62, 50, 49, 27, 78, 10, 17, 14, 13, 82, 40, 45, 54, 56, 23, 48, 0, 1, 2, 3, 4, 5, 6, 7, 24, 25 };
// const 			prog_int8_t mappingArray_P[TOTAL_NODES] PROGMEM = { 25, 24, 7, 6, 5, 4, 3, 2, 1, 0, 48, 23, 56, 54, 45, 40, 82, 13, 14, 17, 10, 78, 27, 49, 50, 62, 71, 81, 51, 47, 64, 92, 16, 88, 72, 91, 12, 15, 43, 38, 26, 60, 63, 57, 68, 55, 11, 9, 94, 28, 84, 73, 77, 83, 86, 36, 18, 61, 66, 70, 32, 46, 22, 8, 87, 19, 39, 79, 89, 95, 53, 52, 85, 58, 69, 59, 42, 41, 65, 21, 35, 20, 75, 93, 30, 80, 76, 67, 34, 44, 33, 37, 90, 29, 74, 31 }; 

// PYROSPHERE REVERSED
// const 			prog_int8_t mappingArray_P[TOTAL_NODES] PROGMEM = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,77,78,79,80,81,82,83,84,85,86,87,88,89,90,91,92,93,94,95};	
PROGMEM const int8_t mappingArray_P[TOTAL_NODES] = {95,94,93,92,91,90,89,88,87,86,85,84,83,82,81,80,79,78,77,76,75,74,73,72,71,70,69,68,67,66,65,64,63,62,61,60,59,58,57,56,55,54,53,52,51,50,49,48,47,46,45,44,43,42,41,40,39,38,37,36,35,34,33,32,31,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0};	

boolean updateFrame(){
		
  int8_t index=0;
  int8_t data_size=98;              									// This variable sets the number of bytes to read from the file.
  byte fileContents[data_size];          						 	// Temporary buffer to pull data from SD card
    
  index=animation.read(fileContents, data_size); 			//file_contents is the data buffer for storing data. data_size is a variable set to the amount of data to read.

  if(index == data_size ){
    for(int i = 0; i < data_size - 2 ; i++){ 					// -1 for cr at end of frame
      if(fileContents[i] == '1'){
        int j = i;					
        if(i >= 32){ 																	// Dealing with the offsets in the file format
          j--;
        }
        if(i >= 64){ 																	// Still dealing with the offsets in the file format
          j -= 2;
        }
         nodeOn(j);
      }
    }
    return 0;
  } else {
	
    animation.rewind();     													// Go back to beginning of file

    index=animation.read(fileContents, data_size);
    for(int i = 0; i < data_size - 1; i++){
      if(fileContents[i] == '1'){
        nodeOn(i);
      }
    }
    return 1;

  }

}

// Used to populate totalFiles with number of files on SD card 
boolean getTotalFiles(){
  totalFiles = 0;
  while(root.readDir(&directory)>0) {
    totalFiles++;
  }
  root.rewind();
	return true;
}

void randomAnimation(){
	Serial.print("Selecting Random Animation: ");  
	int randNum = random(totalFiles);
  root.rewind();

  // Run root.readDir a random number of time to get the file
  for(int i = 0; i < randNum; i++){
    if(!root.readDir(&directory)) {
       Serial.print ("root.Readdir: ");
       Serial.println(i, DEC);
     } 
  }    
  // Serial.print ("Curs Pos: ");
  // Serial.println(root.curPosition()/32-1, DEC);
  // Serial.println((char*)directory.name); // Magic.
	
	SdFile::dirName(directory, currentFile);
  Serial.println(currentFile);
	
  animation.close(); 
  // Check if the animation.open ran correctly, and try again if not
  if(!animation.open(&root, root.curPosition()/32-1, O_READ) ){
    for(int i = 0; i < 5; i++){
      // Run root.readDir a random number of time to get the file
        for(int j = 0; j < randNum; j++){
          if(!root.readDir(&directory)) {
            Serial.print ("root.Readdir: ");
            Serial.println(i, DEC);
          } else {
            break;
          } 
        }
        // Try again to open file
        animation.close();
     
        if(animation.open(&root, root.curPosition()/32-1, O_READ) ){
          break;
          Serial.println("File opened after error");
          // File opened get out of the loop
        }
      }
    }
  
}

/*

	 #####  ####### ####### #     # ######  
	#     # #          #    #     # #     # 
	#       #          #    #     # #     # 
	 #####  #####      #    #     # ######  
	      # #          #    #     # #       
	#     # #          #    #     # #       
	 #####  #######    #     #####  #
	
	setup

*/

//There is some super weird stuff going on in here. We need to figure out why, and how to fix it.
//DEBUGGING: Pyrosphere won't respond? Wait 5 seconds after bootloading, and then open the Serial Monitor.
// VIOLA! ... I spent at least 6 hours attempting to figure this out, NO JOY. Had to move on, someone, please
// fix this tyranical code. k THX

void setup(){                
	
  pinMode(	DATA_PIN1, 	OUTPUT	);
  pinMode(	LATCH_PIN1, 	OUTPUT	);
  pinMode(	CLOCK_PIN1, 	OUTPUT	);

  // pinMode(	DATA_PIN2, 	OUTPUT	);
  // pinMode(	LATCH_PIN2, 	OUTPUT	);
  // pinMode(	CLOCK_PIN2, 	OUTPUT	);
	
	delay(1000);
  
  // Serial.begin(9600);
  Serial.begin(115200);
	
	Serial.println("One Sec");
	
	delay(1000);
	Serial.println("Setup");
	flash();
	
	status = mount();
	
	Serial.print("Mode: ");
	Serial.println(controlMode);
	
	// randomAnimation();
}


/*

	#     # ####### #     # #     # ####### 
	##   ## #     # #     # ##    #    #    
	# # # # #     # #     # # #   #    #    
	#  #  # #     # #     # #  #  #    #    
	#     # #     # #     # #   # #    #    
	#     # #     # #     # #    ##    #    
	#     # #######  #####  #     #    #

*/

boolean mount(){
	bool error = false;
  
	Serial.print("Initializing Card. ");
	
	//SD Settings.	
	if (VERSION != 1) {
		// DEFINE SDPIN depending on Uno or Mega chip ala: http://www.ladyada.net/learn/arduino/ethfiles.html

		chipSelect 									= 4;	 						// for Arduino Ethernet Shield SD
		int sdpin									 = 54;
		pinMode(sdpin, OUTPUT);                       // set the SS pin as an output (necessary!)
		digitalWrite(sdpin, HIGH);                    // but turn off the W5100 chip!		
	}

  if (!card.init(SPI_FULL_SPEED, chipSelect)) {
		Serial.println("ERROR: card.init"); // initialize the SD card
		error = true;
	} else {
		Serial.println("SUCCESS SD2Card Init");
	}
 	delay(500);

	Serial.print("Mounting Volume. ");
  if (!volume.init(&card)) {
		Serial.println("ERROR: volume.init"); 
		error = true;
	}// initialize a FAT volume
	delay(500);

	Serial.print("Setting Root. ");
  if (!root.openRoot(&volume)) {
		Serial.println("ERROR: openRoot");
		error = true;
	} // open the root directory
	delay(500);
	
	Serial.print("Status: ");
	Serial.println(status);
  
  root.readDir(&directory); 
  if(!animation.open(&root, root.curPosition()/32-1, O_READ)) Serial.println("ERROR: Initial open file");

	delay(500);
  root.rewind();
	delay(2000);

  while(!getTotalFiles()) {}

	Serial.print("Total Files: ");
	Serial.println(totalFiles);

	if(error)  { 	Serial.println("FAIL. Trying again soon..."); return false; }
	else 			 { return true; }
}

/*

	#       ####### ####### ######  
	#       #     # #     # #     # 
	#       #     # #     # #     # 
	#       #     # #     # ######  
	#       #     # #     # #       
	#       #     # #     # #       
	####### ####### ####### #       
	
	loop

*/

// Timing.
static long 				then 						= 0;            // Last time a frame was refreshed
static long 				now 						= 0;                 // Will be populated with the current millisecond at the beginning of the loop

//	Animation Loop Settings.
static int8_t 			loopThresh 			= 3;
static int8_t 			loopCount 			= loopThresh + 1;

//autoPilot (right now random, let's diversify!)
static boolean 			autoPilot 			= true;					//Initiates the pilot
static uint16_t 		serialTimeout 	= 20000;				//How long after serial is silent will autoPilot begin.
static long 				lastSerialCMD 	= 0;						//Last time a serial command was recieved
uint8_t 						readMode 				= 0; 						//Wait

void loop() 
{
	
	if(!status) {
		Serial.println("ReMounting...");
		flash();
		status = mount();
		delay(5000);
		return;
	}

  now = millis();       	// This moment is beautiful.
	
	flameSustain(); 				// Sustains flame based on each pin's last timestamp and current frameDuration	
	
	modeSelektor();					// Select mode based on information.
	
	// serialPolling();				// Check for last CMD

	if (!active) { ceaseFire(); }
  ignite();       				// Send the 1011 and let the people have some fun.	
	
	//We are polling the serial connection.
	while(Serial.available() > 0) {
    char x = Serial.read();
		serialRouting(x);
	}

}

void statusUpdate() {
	// Serial.print("Is Dave there?");
	Serial.println("<=== Pyrosphere Status Update ===>"); 
	Serial.print("Frame Interval: ");
	Serial.println(frameInterval); 
	Serial.print("Frame Duration: ");
	Serial.println(frameDuration); 
	Serial.print("System Mode: ");
	Serial.println(controlMode); 
	Serial.print("Loaded Pattern: ");
	Serial.println(currentFile);
	Serial.print("Current Loop Count: ");
	Serial.println(loopCount);
	
	
	Serial.println("===>");
	
	resetMessageBuffer();
}

void serialRouting(char x){
	//Flags, set read mode., begin

	/// Don't we want to prioritize this by most likely input? Probably +/- first.
	
	if 				( x == '!' ) 		{		readMode 	= 1;  	}					//Pattern
	else if 	( x == '@' ) 		{		readMode 	= 2;  	}	 				//Frame Duration
	else if  	( x == '#' ) 		{		readMode 	= 3; 		}					//Frame Interval
	else if   ( x == '+' ) 		{		readMode 	= 4;  	}					//Shift Register IDs, separated by comma (no whitespace)
	else if   ( x == '-' ) 		{		readMode 	= 5;  	}					//Shift Register IDs, separated by comma (no whitespace)
	else if   ( x == '~' ) 		{		readMode 	= 6;  	}					//System Mode 
	else if   ( x == '*' ) 		{		readMode 	= 7;  	}					//System Mode 	
	else if  	( x == '/' ) 		{		getFiles(); 			}		
	else if  	( x == '?' ) 		{		statusUpdate(); 	}			
	//Add custom flags here.
	
	//Finish up
	else if 	(x == '.') 		{ 	//...
		
		//This will update the global variables accordingly.
		//p
		switch(readMode){
			case 1: 			setPattern();   		break;
			case 2:  			setDuration();  		break;
			case 3:  			setInterval();  		break;
			case 4: 			setValveOn(); 			break;
			case 5: 			setValveOff();			break;
			case 6: 			setMode();					break;			
			case 7: 			setActive();				break;						
			default:  												break;	
		}
		
			lastSerialCMD = now; 						//Used for switching to autoPilot
			readMode = 0;										//We're done reading. (until another.)
			autoPilot = false;
		
			bufferIndex = 0;
					
	}
	else 										{ messageBuffer[bufferIndex++] = x; } 				//Magic.
	
}

/*
                                   
	###### #        ##   #    # ###### 
	#      #       #  #  ##  ## #      
	#####  #      #    # # ## # #####  
	#      #      ###### #    # #      
	#      #      #    # #    # #      
	#      ###### #    # #    # ###### 
                                   
                                                
	 ####   ####  #    # ##### #####   ####  #      
	#    # #    # ##   #   #   #    # #    # #      
	#      #    # # #  #   #   #    # #    # #      
	#      #    # #  # #   #   #####  #    # #      
	#    # #    # #   ##   #   #   #  #    # #      
	 ####   ####  #    #   #   #    #  ####  ###### 


flame control
*/

/*************************************************************************************
 * Serial Polling
 * Checks for last successful serial request.
 *************************************************************************************/

// void serialPolling(){
// 
// 	if (!active) {
// 		ceaseFire();
// 	} else  {
// 		if(now - lastSerialCMD > serialTimeout){  
// 	    if(autoPilot == false){
// 	      Serial.println("Automatic love generation.");
// 	    }      
// 	    autoPilot = true;
// 			controlMode = 1; /// Clean this up
// 	  }
// 	}
// }

/*************************************************************************************
 * Mode Selector
 * Switches behavior based on the current mode.
 *************************************************************************************/
void modeSelektor(){
	long since = now - then;
	
	if (!active) {
		ceaseFire();
		
	// Automatically go to random mode.
	} else if(now - lastSerialCMD > serialTimeout){  
    if(controlMode != 0){
			// Serial.println("Automatic love generation.");
			// autoPilot = true;
			// setMode(0);
    }      
  }			

	if(controlMode != 2 && since > frameInterval || since > MAX_FRAME_INTERVAL){  
	  // Go to next frame
		nextFrame();		
	}	
	
}

void nextFrame(){

	if(updateFrame()){
		
    if(loopCount > loopThresh){
			
			// Serial.println("<=== Repeating! ===>");						
			// delay(1000);flash();delay(100);flash();delay(100);flash();delay(1000);
			
			//// CURATING PATTERNS - LEAVE THESE OFF FOR NOW ///
			if 				(controlMode == 0) 	{ randomAnimation(); }
			else if 	(controlMode == 1) 	{ nextPattern(); 	}			
			
			loopCount = 0;
    } else {
      loopCount++;
    }

  }
  then = now;
}

/*************************************************************************************
 * Safety + Sustain
 * Checks for last successful serial request.
 *************************************************************************************/

void flameSustain(){

	// Check for how long each node has been on.
	
  // for(int i = 0; i < TOTAL_NODES; i++){      // This loop turns off nodes based on their timestamp and how long each is to be on
  // 		long onFor = now - nodeTimeStamps[i];
  //   if(nodeTimeStamps[i] > 0){
  //     if(onFor > frameDuration || onFor > MAX_FRAME_DURATION){
  //       nodeOff(i);
  //     }
  //   }
  // }
	
	// if (verbose) { Serial.println("Flame Sustain.");}
	
  for(int i = 0; i < TOTAL_NODES; i++) {      // This loop turns off nodes based on their timestamp and how long each is to be on
   		long onFor = now - nodeTimeStamps[i];
  
  		if(nodeTimeStamps[i] <= 0) continue; 
  
  		if (controlMode == 2 && (onFor > nodeDurations[i] || onFor > MAX_FRAME_DURATION)) {		
  				nodeOff(i);
  				nodeDurations[i] = 0;
  		} else if (onFor > frameDuration || onFor > MAX_FRAME_DURATION){
  			  nodeOff(i);    
  		}		      					
  }


}

/*

			######  #######    #    ######     ######     #    ####### ####### 
			#     # #         # #   #     #    #     #   # #      #       #    
			#     # #        #   #  #     #    #     #  #   #     #       #    
			######  #####   #     # #     #    ######  #     #    #       #    
			#   #   #       ####### #     #    #       #######    #       #    
			#    #  #       #     # #     #    #       #     #    #       #    
			#     # ####### #     # ######     #       #     #    #       #    
			
			patterns reading

*/	

	/*			
	 * Pattern
	 * Selects from available patterns
	 */

	void setPattern(){
	
		char *patternName = messageBuffer;
		strcat(patternName, ".dat");
		
		// Serial.print("Setting Pattern: ");
		// Serial.println(patternName);
		
		setMode(1);		// break out of auto.
		
	  changePattern(patternName);
		
		resetMessageBuffer();
		
	}

	/**
	 * Interval
	 * Set the interval
	 */

	void setInterval(){
	
		frameInterval = atoi(messageBuffer);
		resetMessageBuffer();
		
			// Serial.print("Setting Interval: ");
			// Serial.println(frameInterval);
	
	}

	/**
	 * Duration
	 * Set the duration during control mode.
	 */

	void setDuration(){
	
		// if (atoi(messageBuffer)) /// setup min max stops
		frameDuration = atoi(messageBuffer);
		resetMessageBuffer();
		
		// Serial.print("Setting Duration: ");
		// Serial.println(frameDuration);
	
	}
	
	/**
	 * Set Register On
	 * Open the specified valve.
	 */

	void setValveOn(){
		
		int valveID = atoi(messageBuffer);

		if (valveID < TOTAL_NODES) {
			if (debug) {
				// Serial.print("Setting Valve On: ");
				// Serial.println(valveID);			
			}

			if (controlMode != 2) {
				setMode(2);
				// controlMode = 2; /// This might be in the wrong place.
			}

		  nodeOn(valveID);
			nodeDurations[valveID] = frameDuration;
		}		
		
		resetMessageBuffer();		

	}
	
	/**
	 * Set Register Off
	 * Close the specified valve.
	 */
	
	void setValveOff(){
		
		int valveID = atoi(messageBuffer);
		
		if (valveID < TOTAL_NODES) {
			if (debug) {
				// Serial.print("Setting Valve Off: ");
				// Serial.println(valveID);			
			}

		  nodeOff(valveID);			
		}		

		resetMessageBuffer();		

	}
	
	/**
	 * Set Mode
	 * Set the receive mode.
	 */
	void setMode(){
		char *modeSig = messageBuffer;
		uint8_t mode = atoi(modeSig); 
		
		setMode(mode);
		
		resetMessageBuffer();
	}

	void setMode(uint8_t m){
	
			
		Serial.print("Setting Mode: ");
		Serial.println(m);
		
		//This will update the global variables accordingly.
		switch(m){
			// Autopilot Random
			case 0: 			controlMode = 0;   		
				// frameInterval = DEFAULT_FRAME_INTERVAL;
				// frameDuration = DEFAULT_FRAME_DURATION;
				loopCount = 0;
				break; // Off.

			// Autopilot Progressive	
			case 1: 			controlMode = 1;   		
				// frameInterval = DEFAULT_FRAME_INTERVAL;
				// frameDuration = DEFAULT_FRAME_DURATION;
				loopCount = 0;
				break; 

			// Valve Control	
			case 2:  			controlMode = 2;  		
				// frameDuration = MAX_FRAME_DURATION;
				break; 

			case 3:  			controlMode = 3;  		
				break; // 
			case 4: 			controlMode = 4;			
				break; // ...
			case 5: 			controlMode = 5;			
				break; // ...
			default:  													
				break;	
		}	
		

		
	}
		
	/**
	 * Set Switch
	 * Switch off, switch on, or reset.
	 */

	void setActive(){
	
		char *activeSig = messageBuffer;
		int sig = atoi(activeSig); 
			
		// Serial.print("Setting Switch: ");
		// Serial.println(sig);
		
		//This will update the global variables accordingly.
		switch(sig){
			case 0: 			active = false;   break; // Off.
			case 1: 			active = true;   	break; // On.
			case 2:  			resetPattern();		break; // Reset ///Do we want to activate here too?
			default:  											break;	
		}	
		
		resetMessageBuffer();
		
	}
	
	/**
	 * Reset Pattern
	 * Rewind to the beginning and set the loop count to 0.
	 */
	void resetPattern () {
		// Serial.println("Resetting Pattern");
		animation.rewind();
		loopCount = 0;
	}
		
	/**
	 * Get Files.
	 * returns a list of the files. 
	 */

	void getFiles(){		 	
		// while(root.readDir(&directory)) {			
		// 	
		// }
	}


	/**
	 * Panic
	 * FREAK OUT AND SHUT DOWN.
	 */

	void ceaseFire(){
		initFrameBuffer();				//Sets everything to off.
	}
	
	void resetMessageBuffer(){
		memset( messageBuffer, '\0', sizeof(messageBuffer) );		
	}

/*


	######     #    #######    #    
	#     #   # #      #      # #   
	#     #  #   #     #     #   #  
	#     # #     #    #    #     # 
	#     # #######    #    ####### 
	#     # #     #    #    #     # 
	######  #     #    #    #     # 


*/

/*************************************************************************************
 * Data Functions:
 * ignite() - sends the contents of frameBuffer to the shift registers
 * initFrameBuffer() - sets every node to off
 *************************************************************************************/

// Send the frameBuffer (buffer) to the shift registers
void ignite(){
	volatile int foo;
  digitalWrite(LATCH_PIN1, LOW);
	// digitalWrite(LATCH_PIN2, LOW);
  for(int i = 0; i < NUM_REGISTERS; i++){
    // if (i < LINE1) {
			// LINE1
			shiftOut(DATA_PIN1, CLOCK_PIN1, MSBFIRST, frameBuffer.frameChunk[i]);
		// } else {
			// ELSE LINE2
// /			shiftOut(DATA_PIN2, CLOCK_PIN2, MSBFIRST, frameBuffer.frameChunk[i-LINE1]);
		// }
  }
	for (foo = 0; foo<3000; foo++);
  digitalWrite(LATCH_PIN1, HIGH);
	for (foo = 0; foo<3000; foo++);
	// digitalWrite(LATCH_PIN2, HIGH);
}
  
void initFrameBuffer(){
  for(int i = 0; i < NUM_REGISTERS; i++){
    frameBuffer.frameChunk[i] = B00000000;
  }
}


void nextPattern() {	

	if(root.readDir(&directory)<=0){
		
		// Serial.print("Rewinding..."); 
		
    root.rewind();
    root.readDir(&directory);
		
  }

	SdFile::dirName(directory, currentFile);
	Serial.print("Next Pattern..."); Serial.println(currentFile);    
	changePattern(currentFile);
  
	
  //Stub 
	
}

void prevPattern () {
	
  //Stub 

}

void changePattern (char *fileName){
  animation.close();                              //Close the file

	Serial.print("Opening Pattern: "); Serial.println(fileName);
	
  if(!animation.open(&root, fileName, O_READ))             //Open the file in read mode.
		{ 
			Serial.print("Couldn't open pattern:"); Serial.print(fileName); 
		}
}


/*

	######  ####### ######  #     #  #####   #####  ### #     #  #####  
	#     # #       #     # #     # #     # #     #  #  ##    # #     # 
	#     # #       #     # #     # #       #        #  # #   # #       
	#     # #####   ######  #     # #  #### #  ####  #  #  #  # #  #### 
	#     # #       #     # #     # #     # #     #  #  #   # # #     # 
	#     # #       #     # #     # #     # #     #  #  #    ## #     # 
	######  ####### ######   #####   #####   #####  ### #     #  #####  
	
	debugging

*/


void toggleDebug(char *val) {
  if (atoi(val) == 0) {
    debug = false;
    Serial.println("goodbye");
  } else {
    debug = true;
    Serial.println("hello");
  }
}


/*
	Flashes all the torches on the pirosphere once. (for 400 milliseconds)
*/

void flash(){
	digitalWrite(LATCH_PIN1, LOW);
  for(int i = 0; i < NUM_REGISTERS; i++){
    shiftOut(DATA_PIN1, CLOCK_PIN1, MSBFIRST, B11111111);
  }
  digitalWrite(LATCH_PIN1, HIGH);
	delay(400);
	digitalWrite(LATCH_PIN1, LOW);
  for(int i = 0; i < NUM_REGISTERS; i++){
    shiftOut(DATA_PIN1, CLOCK_PIN1, MSBFIRST, B00000000);
  }
  digitalWrite(LATCH_PIN1, HIGH);
}

/*

                                                    
	####### #     # 
	   #     #   #  
	   #      # #   
	   #       #    
	   #      # #   
	   #     #   #  
	   #    #     # 
	
	tx
		
*/

// /**
//  * Digital read
//  * @param char pin pin identifier
//  */
// void dr(char *pin) {
//   if (debug) {
//     Serial.println("dr"); }
//     
//   int p = getPin(pin);
//   if (p == -1 && debug) {
//     Serial.println("badpin"); 
//   } else {
//     pinMode(p, INPUT);
//     int oraw = digitalRead(p);
//     char m[7];
//     sprintf(m, "%02d::%02d", p,oraw);
//     Serial.println(m);
//   }
// }
// 
// int getPin(char *pin) { //Converts to A0-A5, and returns -1 on error
//   int ret = -1;
//   if (pin[0] == 'A' || pin[0] == 'a') {
//     switch(pin[1]) {
//       case '0': ret = A0; break;
//       case '1': ret = A1; break;
//       case '2': ret = A2; break;
//       case '3': ret = A3; break;
//       case '4': ret = A4; break;
//       case '5': ret = A5; break;
//       default:            break;
//     }
//   } else {
//     ret = atoi(pin);
//     if (ret == 0 && (pin[0] != '0' || pin[1] != '0')) {
//       ret = -1; }
//   }
//   
//   return ret;
// }

/*

	###  #####  #     # ### ####### ####### 
	 #  #     # ##    #  #     #    #       
	 #  #       # #   #  #     #    #       
	 #  #  #### #  #  #  #     #    #####   
	 #  #     # #   # #  #     #    #       
	 #  #     # #    ##  #     #    #       
	###  #####  #     # ###    #    ####### 
                                        
	####### ######     #     #####  #    # ### #     #  #####  
	   #    #     #   # #   #     # #   #   #  ##    # #     # 
	   #    #     #  #   #  #       #  #    #  # #   # #       
	   #    ######  #     # #       ###     #  #  #  # #  #### 
	   #    #   #   ####### #       #  #    #  #   # # #     # 
	   #    #    #  #     # #     # #   #   #  #    ## #     # 
	   #    #     # #     #  #####  #    # ### #     #  #####  
	
	O_o


*/

void nodeOn(int8_t nodeNum){
  int8_t mappedValue = pgm_read_byte(&mappingArray_P[nodeNum]);
  nodeTimeStamps[nodeNum] = millis();
  nodeOnMacro(frameBuffer, mappedValue);
}


void nodeOff(int8_t nodeNum){
  int8_t mappedValue = pgm_read_byte(&mappingArray_P[nodeNum]);
  nodeTimeStamps[nodeNum] = -1;
  nodeOffMacro(frameBuffer, mappedValue);
}


void shiftOut(uint8_t dataPin, uint8_t clockPin, uint8_t bitOrder, uint8_t val)
{
	uint8_t i;
	volatile int foo;
	
	for (i = 0; i < 8; i++)  {
		if (bitOrder == LSBFIRST)
			digitalWrite(dataPin, !!(val & (1 << i)));
		else	
			digitalWrite(dataPin, !!(val & (1 << (7 - i))));
			
		for (foo=0; foo <3000; foo++);
		
		digitalWrite(clockPin, HIGH);
		digitalWrite(clockPin, LOW);		
	}
}

