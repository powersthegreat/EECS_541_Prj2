#include <Arduino.h>
#include <TimerOne.h>
#include <params.h>


//Signal Detection Variables
volatile unsigned long fallTime = 0;
volatile unsigned long prevTime = 0;
volatile unsigned long sampledDelays[20];
volatile bool trigger = 0;
volatile int count = 0;
volatile bool checkDelay = 0;
unsigned long detectedBitRate = 0;
volatile bool calibrating = 0;
volatile bool hit;

//Timing Verification Variables
unsigned long prevTimeStamp;
unsigned long driftCorrection;


//Main Variables
bool detectedSignal = 0;
bool validatedSignal = 0;
bool recievingMode = 0;

//Message reading
volatile int incomingPacket[14];
volatile int readCount = 0;




//User input
char command;

//fall Time Detection Callback Function
void fall(){
  if(calibrating){
    prevTime = fallTime;
    fallTime = micros();
    sampledDelays[count % 20] = fallTime - prevTime;
    count++;
  }
  
}

//Function that takes sampled bit delays and averages them
void calibrateDelay(){
  count = 0;
  calibrating = 1;
  attachInterrupt(digitalPinToInterrupt(dataPin),fall,RISING);
  while(count < 20){
    //Busy Wait
  }
  calibrating = 0;
  detachInterrupt(digitalPinToInterrupt(dataPin));

  unsigned long sum = 0;
  for(int i = 5; i < 20; i++){
    Serial.println(sampledDelays[i]);
    sum += sampledDelays[i];
  }
  
  detectedBitRate = sum / 15;

}


void readData(){
    if(trigger){
      //if(readCount > 0){//Ignore first bit in stream
        
        digitalWrite(redLED, LOW);
        if(hit){
          incomingPacket[readCount] = 1;
        }
        else{
          incomingPacket[readCount] = 0;
        }
        
      //}
      

      hit = 0;
      if(readCount == 14){
        readCount = 0;
        trigger = 0;
      }
      digitalWrite(redLED, HIGH);

      readCount++;
    }
    
    
    
}

//Simple cleanup function
void resetArrays(){
  for(int i = 0; i < 20; i++){
    sampledDelays[i] = 0;
  }
  for(int i = 0; i < 14; i++){
    incomingPacket[i] = 0;
  }
}

volatile int hitsCounted;
volatile bool awaitTrigger = 0;
//Use for getting exact start of signal tranmsission. Used for detecting falling edge of bits
void awaitTriggerSignal(){
  if(awaitTrigger){
    digitalWrite(greenLED,LOW);
    if(!trigger){
      trigger = 1;
      // Timer1.restart();
      hit = 0;
      hitsCounted = 0;
      
    }
    else{
      hit = 1;
      hitsCounted++;
      
    }  

    digitalWrite(greenLED, HIGH);

    }
  
  
}



//Verifies the calculated bit delay is usable
bool verifySignal(){
  trigger = 0;
  readCount = 0;
  hit = 0;

   
  awaitTrigger = 1;
  while(!trigger){}


  
  while(trigger){}
  awaitTrigger = 0;

  digitalWrite(greenLED, HIGH);
  digitalWrite(redLED, HIGH);


  for(int i = 1; i < 14; i++){
    if(!incomingPacket[i]){ 
      return(false);
    }
  }
  
  return(true);
}

//Get raw 12 bit packet 
int readMessage(){
  trigger = 0;
  readCount = 0;
  hit = 0;
  hitsCounted = 0;


  awaitTrigger = 1;
  while(!trigger){}


  
  while(trigger){}
  awaitTrigger = 0;

  Serial.print("Hits counted: ");
  Serial.println(hitsCounted);
  return(hitsCounted);
  
}


void setup() {
  // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(dataPin,INPUT);
  pinMode(greenLED,OUTPUT);
  pinMode(redLED,OUTPUT);
  digitalWrite(greenLED, HIGH);
  digitalWrite(redLED, HIGH);
  Timer1.initialize();
  
}


int ham[12];
int hamCorrected[12];
bool ham_calc(int position, int code_length){
  int count = 0, i, j;
  i = position-1;//Removed a one

  // Traverse to store Hamming Code
  while (i < code_length) {
    for (j = i; j < i + position; j++) {
      // If current bit is 1
      if(ham[j] == 1){
        if(j < 12){
          count++;
        }
        
      }
          
    }

    // Update i
    i = i + 2 * position;
  }


  if (count % 2 == 0){
    return 0;
  } 
  else{
    return 1;
  }

}


void decodeHam(){
  uint8_t error, decodedMessage;
  error = decodedMessage = 0;

  int i, p_n = 0, c_l, j, k;
  i = 0;

  while (8 > (int)(1 << i) - (i + 1)) {
    p_n++;
    i++;
  }
  c_l = p_n + 8;

  j = k = 0;


  for(int i = 0; i < 12 ; i++){
      ham[11-i] = incomingPacket[i+1];
      hamCorrected[11-i] = incomingPacket[i+1];
  }


  int ones = 0;
  ones = 0;
    for(int z = 1; z < 14 ; z++){
      if(incomingPacket[z]){
        ones++;
      }
    }
  bool parity;

  if(ones % 2){
      parity = 0;
    }
    else{
      parity = 1;
    }



  // Traverse and update the
  // hamming code
  for (int i = 0; i < p_n; i++) {


    // Find current position
    int position = (int)(1 << i);

    // Find value at current position
    int value = ham_calc(position, c_l);

    // Update the code
    ham[position - 1] = value;
    hamCorrected[position - 1] = value;

  
  }


  for (int i = 0; i < 4; i++) {
    // Calculate error
    int position = (int)(1 << i);
    error |= (uint8_t)(ham[position -1] << i);
  
  }

  Serial.print("Newly Calculated Ham: ");
  for(int i = 0; i < 12; i++) {
      Serial.print(ham[11-i],DEC);
  }
    Serial.print('\n');

  
  if((error > 0) && !parity){
    Serial.println("Two bit error detected");
    Serial.print("Error: ");
    Serial.println(error);
    Serial.print("Parity: ");
    Serial.println(parity);

  }
  else if((error > 0) && parity){
    Serial.print("1 bit error at: ");
    Serial.println(error);
    //hamCorrected[error] = !hamCorrected[error];
  }
  else{
    Serial.println("No error detected");
  }



  j = k = 0;
  int decodedPacket[8];
  int fixedDecodedPacket[8];
  //Extract incoming data from ham
  for(int i = 0; i < 12; i++){
    if(i == (int)(1 << k) - 1){
      k++;
    }
    else{
      decodedPacket[j] = ham[i]; 
      fixedDecodedPacket[j] = hamCorrected[i];
      j++;
    }
  }

  uint8_t data, correctedData;
  for(int i = 0; i < 8; i++){
    data |= (decodedPacket[i] << i);
    correctedData |= (fixedDecodedPacket[i] << i);
  }


  if((error > 0) && !parity){
    Serial.println("Unable to decode");
  }else if((error > 0) && parity){
    //Serial.print("Uncorrected data: ");
    //Serial.println(data,BIN);
    //Serial.print("Corrected data: ");
    //Serial.println(correctedData,BIN);
  }
  else{
    Serial.print("Data: ");
    Serial.println(data,BIN);
  }

}


bool timerActive = 0;
void loop() {
  resetArrays();
  Serial.println("Send a command: 'c' = calibrate, 'r' = read");
  while(!Serial.available()){}
  command = Serial.read(); 

  if(command == 'c'){
    Serial.println("Attempting to detect signal");
    calibrateDelay();
    attachInterrupt(digitalPinToInterrupt(dataPin),awaitTriggerSignal,RISING);
    Serial.print("Detected Bit Rate: ");
    Serial.println(detectedBitRate);
    Timer1.attachInterrupt(readData,detectedBitRate);
    Timer1.start();
    timerActive = 1;


    //Validating
    Serial.println("Attempting to validate signal");
    validatedSignal = verifySignal();
    if(validatedSignal){
      Serial.println("Signal Verified");
      recievingMode = 1;
    }
    else{
      Serial.println("Verify Failed. Trying again"); 
      detectedSignal = 0;
      recievingMode = 0;
    }

    Serial.println("Packet detected");
    for(int i = 1; i < 14; i++){
      Serial.print(incomingPacket[i]);
    }
    Serial.print('\n');
    //do{}while(readMessage());//Trying to fix reading bug. Ugly solution
  }
  else if(command == 'r'){
    if(recievingMode){
      //resetArrays();
      Serial.println("Entering reading mode:");
      readMessage();
      //do{}while(!readMessage());//Trying to fix reading bug. Ugly solution
      Serial.print("Recieved Packet: ");
      for(int i = 1; i < 14; i++){
        Serial.print(incomingPacket[i]);
      }
      Serial.println("");
      decodeHam();
     

    }
    else{
      Serial.println("Signal not calibrated yet");
    }

  }






  
}