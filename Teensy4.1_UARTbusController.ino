//global scope=================================================================================================
//header files-------------------------------------------------------------------------------------------------
#include <SoftwareSerial.h>
#include <string>
#include "SdFat.h"
#include <SD.h>
#include <SPI.h>
//changable const variables and objects------------------------------------------------------------------------
const int missingDeviceTimeout = 5;     //in milliseconds
const int errorSafegaurdTimeout = 500;  //ditto ^^^
const int numDevices = 2; //the total number of child devices
File RPM_data;
File Accelerometer_data;
File deviceFiles[numDevices] =      {RPM_data,  Accelerometer_data};
std::string fileNames[numDevices] = {"RPM_data", "Accelerometer_data"};
std::string fileHeaders[numDevices] = {
  "Timestamp,Milliseconds,RPM\n" ,
  "Timestamp,Milliseconds,xAccel,yAccel,zAccel\n"
};
//consts-------------------------------------------------------------------------------------------------------
const int pinLED = 13; //for debugging
const int pinTxD = 14; //serial output
const int pinRxD = 15; //serial input
const int pinTransmitFlag = 16; //bool output (LOW = read, HIGH = transmit)
const char EOT = 0; //EndOfTransmission character
SoftwareSerial mySerial(pinRxD, pinTxD, true);
//global variables---------------------------------------------------------------------------------------------
int currDevice = 0;
unsigned long setupTimeOffset = 0;
//SdExFat sd;       //IDK delete?
//SdVolume volume;  //IDK delete?
Sd2Card card;
//setup function===============================================================================================
void setup() { // put your setup code here, to run once:
  Serial.begin(9600);
  pinMode(pinLED, OUTPUT);
  //setup microSD card-----------------------------------------------------------------------------------------
  if (SD.begin(BUILTIN_SDCARD) == false) {
    while (true) { //blink indefinitely
      digitalWrite(pinLED, HIGH);
      delay(333);
      digitalWrite(pinLED, LOW);
      delay(333);
    }
  }
  if (card.init(SPI_HALF_SPEED, BUILTIN_SDCARD) == false) {
    while (true) { //blink indefinitely
      digitalWrite(pinLED, HIGH);
      delay(333);
      digitalWrite(pinLED, LOW);
      delay(333);
    }
  }
  //file creation----------------------------------------------------------------------------------------------
  for (int i = 0; i < numDevices; i++) {
    int currFileNum = 1;
    std::string currFileName = fileNames[i];
    while (true) {
      currFileName += '_';
      currFileName += std::to_string(currFileNum);
      currFileName += ".csv";
      if (SD.exists(currFileName.data())) {
        currFileName = fileNames[i];
        currFileNum++;
        continue;
      }
      else {
        deviceFiles[i] = SD.open(currFileName.data(), O_CREAT | O_TRUNC | O_WRITE);
        break;
      }
    }
  }
  //setup i/o pins and serial object---------------------------------------------------------------------------
  pinMode(pinRxD, INPUT);
  pinMode(pinTxD, OUTPUT);
  pinMode(pinTransmitFlag, OUTPUT);
  digitalWrite(pinTransmitFlag, LOW);
  mySerial.begin(9600);
  //sets the baud rate ^^
  //Teensy4.1 was accurate at a clock setting of 600MHz
  mySerial.listen(); //marks this serial's reciever as the active one in the chip

  //add data titles
  for(int i = 0; i < numDevices; i++) {
    deviceFiles[i].print(fileHeaders[i].data());
  }

  //post-setup-------------------------------------------------------------------------------------------------
  for (int i = 0; i < 30; i++) {
    digitalWrite(pinLED, HIGH);
    delay(50);
    digitalWrite(pinLED, LOW);
    delay(50);
  }
  //set time offset
  setupTimeOffset = millis();
}
//loop function================================================================================================
void loop() { // put your main code here, to run repeatedly:
  delay(1); //for debugging/safety
  //increment device ID----------------------------------------------------------------------------------------
  currDevice++;
  if (currDevice > numDevices)
    currDevice = 1;
  //send ID byte-----------------------------------------------------------------------------------------------
  digitalWrite(pinTransmitFlag, HIGH);
  mySerial.write(currDevice);
  delayMicroseconds(1085); //this number is accurate at 9600 baud
  digitalWrite(pinTransmitFlag, LOW);
  //wait for sensor device response----------------------------------------------------------------------------
  for (int i = 0; i <= missingDeviceTimeout; i++) { //timeout code
    if (mySerial.peek() != -1)
      break;
    if (i == missingDeviceTimeout - 1) {
      return;
    }
    delay(1);
  }
  //record data onto files-------------------------------------------------------------------------------------
  //print time data
  unsigned long currMillis = millis() - setupTimeOffset;
  deviceFiles[currDevice-1].print(millisToTimestamp(currMillis).data());
  deviceFiles[currDevice-1].print(",");
  deviceFiles[currDevice-1].print(currMillis);
  deviceFiles[currDevice-1].print(",");
  //print sensor data
  std::string dataToSave = "";
  for (int i = 0; i < errorSafegaurdTimeout; i++) {
    char currByte = mySerial.read(); //read dataByte char
    if (currByte == 255) {
      delay(1);
      continue;
    }
    
    Serial.write(currByte);
    
    //if EOT, log the string onto the file- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (currByte == EOT) {
      deviceFiles[currDevice - 1].print(dataToSave.data());
      Serial.println(""); //DEBUG-DEBUG-DEBUG
      deviceFiles[currDevice - 1].print("\n");
      break;
    }
    //add byte to the string- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    if (currByte >= ' ') { //log dataByte char
      dataToSave.push_back(currByte);
    }
    //delay for proper timeout- - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
    delay(1);
  }
  //save data--------------------------------------------------------------------------------------------------
  deviceFiles[currDevice - 1].flush();
}

std::string millisToTimestamp(unsigned long millisParam) {
  //calculate values
  int millisecond = millisParam % 1000;
  int second = (millisParam / 1000) % 60;
  int minute = (millisParam / (60*1000) ) % 60;
  int hour = millisParam / (60*60*1000);
  //make output string
  std::string output = "";
  output += std::to_string(hour);
  output += " ";
  if(minute < 10) //align every number
    output += "0";
  output += std::to_string(minute);
  output += " ";
  if(second < 10) //align every number
    output += "0";
  output += std::to_string(second);
  output += " ";
  if(millisecond < 10) //align every number
    output += "0";
  if(millisecond < 100) //align every number
    output += "0";
  output += std::to_string(millisecond);
  return output;
}