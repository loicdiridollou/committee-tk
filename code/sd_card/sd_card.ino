#include <SPI.h>
#include <SD.h>

const int chipSelect = 10;
File dataFile;

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("SD Card Timestamped File Test");
  
  // Initialize SD card
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
    while(1);
  }
  Serial.println("SD card initialized");
  
  // Create filename with timestamp (format: YYYYMMDD.CSV)
  char filename[20];
  sprintf(filename, "%04d%02d%02d.CSV", 2025, 10, 17);
  
  Serial.print("Creating file: ");
  Serial.println(filename);
  
  // Open file in write mode
  dataFile = SD.open(filename, FILE_WRITE);
  
  if (!dataFile) {
    Serial.println("Could not create file!");
    while(1);
  }
  
  Serial.println("File created successfully!");
  
  // Write header
  dataFile.println("Time,Temperature,Humidity,Pressure");
  
  // Write some sample data
  dataFile.println("10:30:45,23.5,65,1013.25");
  dataFile.println("10:31:45,23.6,64,1013.24");
  dataFile.println("10:32:45,23.7,63,1013.23");
  
  // Save to SD card
  dataFile.flush();
  Serial.println("Data written to file");
  dataFile.close();
  
  // Reopen file in read mode and print contents
  Serial.println("\n--- File Contents ---");
  dataFile = SD.open(filename, FILE_READ);
  
  if (!dataFile) {
    Serial.println("Could not open file for reading!");
    return;
  }
  
  while (dataFile.available()) {
    char c = dataFile.read();
    Serial.write(c);
  }
  
  dataFile.close();
  Serial.println("--- End of File ---\n");
}

void loop() {
  // Nothing
}