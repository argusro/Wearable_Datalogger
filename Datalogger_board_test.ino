#include <TinyGPS++.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>
#include "MPU9250.h"
#include <Wire.h>
#include <SPI.h>
#include "FS.h"
#include "SD.h"

// SD card pinout
#define MOSI 13
#define MISO 12
#define CS 15
#define SCLK 14

// Base for filename
#define FILE_BASE_NAME "Datalogger_"

File file;

const uint8_t BASE_NAME_SIZE = sizeof(FILE_BASE_NAME) - 1;
char fileName[] = FILE_BASE_NAME "00.csv";

// Max size for each csv file, 5MB
#define max_file_size 5242880

uint32_t mLastTime = 0;

float yaw, pitch, roll;

float LAT,LON,T,P,A,HUM;
int h,m,s,D,M,Y;

// The TinyGPS++ object
TinyGPSPlus gps;

// Temp and pressure sensor object
Adafruit_BME280 bme; 
// Sealevel pressure for altitude calculation
#define SEALEVELPRESSURE_HPA (1024)
// Motion tracking object
MPU9250 mpu;

void setup() {
  // Debug serial
  Serial.begin(115200);
  // GPS serial
  Serial1.begin(9600);
  Serial.println("\n");
  Serial.println("Booting");
  // Configure SD connections
  SPI.begin(SCLK, MISO, MOSI, CS); 

  Wire.begin();
  delay(2000);
  // Starting sensores with i2c address
  bme.begin(0x76);
  mpu.setup(0x68);
  // Starting SD
  if (!SD.begin(CS)) {
    Serial.println("Card Mount Failed SPI");
  } else {
    Serial.println("SPI");
  }

  while (SD.exists(fileName)) {
    if (fileName[BASE_NAME_SIZE + 1] != '9') {
      fileName[BASE_NAME_SIZE + 1]++;
    } else if (fileName[BASE_NAME_SIZE] != '9') {
      fileName[BASE_NAME_SIZE + 1] = '0';
      fileName[BASE_NAME_SIZE]++;
    } else {
      Serial.println(F("Can't create file name"));
      return;
    }
  }
  file = SD.open(fileName, FILE_WRITE);
   if (!file) {
    Serial.println(F("open failed"));
    return;
   }
   Serial.print(F("opened: "));
   Serial.println(fileName);
  // Log file header
  String doc_header="ms,Yaw,Pitch,Roll,Bat(V),Lat,Lon,Time,Temp(C),Press(hPa),Alt(m),Hum(%)";
  File dataFile = SD.open(fileName, FILE_WRITE);
  dataFile.println(doc_header);
  dataFile.close();  
}

// Deal with GPS serial message
void serialEvent() {                                              // Treat serial event
  while (Serial1.available() > 0) {
    if (gps.encode(Serial1.read())){
      LAT=gps.location.lat(); 
      LON=gps.location.lng(); 
      h=gps.time.hour();
      m=gps.time.minute();
      s=gps.time.second();
      M=gps.date.month();
      D=gps.date.day();
      Y=gps.date.year();
    }
  }
  
}

// Log GPS and Barometer information to file
void second(String dataMPU){
  // Read barometer information
  T=bme.readTemperature();
  P=bme.readPressure() / 100.0F;
  A=bme.readAltitude(SEALEVELPRESSURE_HPA);
  HUM=bme.readHumidity();
  // Read battery voltage
  float Analog0 = analogRead(25);
  float V = (Analog0 / 1024) * 4.18;
  // format GPS and barometer data to be saved on file
  char dataGPS[80];
  sprintf(dataGPS,",%.2f,%.6f,%.6f,%02d/%02d/%02d,%02d:%02d:%02d",V,LAT,LON,D,M,Y,h,m,s);
  char dataBME[80];
  sprintf(dataBME,",%.2f,%.0f,%.2f,%.2f",T,P,A,HUM);

  // Debug output and save to file
  Serial.print(dataGPS);
  Serial.println(dataBME); 

  to_file(dataMPU+dataGPS+dataBME);   
}

// Save string to file
void to_file(String dataString){
  File dataFile = SD.open(fileName, FILE_WRITE);
  if (dataFile) {
    dataFile.println(dataString);
    unsigned long file_size = dataFile.size();
    // If filesize reached 5MB, increase filename
    if (file_size > max_file_size ) {
      fileName[BASE_NAME_SIZE + 1]++;
      // Serial.println("new file: "+ fileName);
    }         
    dataFile.close();
  }
}

void loop() {
  // Check for GPS output
  if (Serial1.available()) {
    serialEvent();
  }
  // Check for motion update
  if (mpu.update()) {
    static uint32_t prev_ms = millis();
    // Confirm that at least 100ms has passed from last measurement
    if (millis() > prev_ms + 100) {
      // Read motion sensor
      yaw = mpu.getYaw();
      pitch = mpu.getPitch();
      roll = mpu.getRoll();
      char dataMPU[80];
      // Format motion information
      sprintf(dataMPU,"%08u,%.2f,%.2f,%.2f",millis()/100,yaw,pitch,roll);
      Serial.print(dataMPU);
      // Check if a full second has passed, to call GPS and Barometer information
      if (millis()/100 % 10 == 0){
        second(dataMPU);
      }
      // Otherwise just save motion information to file
      else{
        to_file(dataMPU);
        Serial.println();
      }
      prev_ms = millis();
    }
  }
}
