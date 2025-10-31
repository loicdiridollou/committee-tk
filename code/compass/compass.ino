#include <Arduino.h>
#include <Wire.h>
#include "bmm150.h"
#include "bmm150_defs.h"

BMM150 bmm = BMM150();

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("Starting");
  
  if (bmm.initialize() == BMM150_E_ID_NOT_CONFORM) {
    Serial.println("Chip ID can not read!");
    while (1);
  } else {
    Serial.println("BMM150 Initialize done!");
    Serial.println("Rotate compass in all directions for calibration...\n");
  }
}

void loop() {
  bmm150_mag_data value;
  bmm.read_mag_data();
  
  value.x = bmm.raw_mag_data.raw_datax;
  value.y = bmm.raw_mag_data.raw_datay;
  value.z = bmm.raw_mag_data.raw_dataz;
  
  // Calculate heading from X and Y
  float heading = atan2(value.y, value.x);
  
  // Convert to degrees (0-360)
  if (heading < 0) {
    heading += 2 * PI;
  }
  float headingDegrees = heading * 180 / M_PI;
  
  Serial.print("X: ");
  Serial.print(value.x);
  Serial.print("  Y: ");
  Serial.print(value.y);
  Serial.print("  Z: ");
  Serial.print(value.z);
  Serial.print("  |  Heading: ");
  Serial.print(headingDegrees);
  Serial.println("°");
  
  delay(500);
}