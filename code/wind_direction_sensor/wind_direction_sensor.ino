const int windVanePin = A0;

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("CYC-FX1 Wind Direction Sensor Test");
}

void loop() {
  // Read analog value (0-1023)
  int sensorValue = analogRead(windVanePin);
  
  // Convert to voltage (0-5V scale, but sensor outputs 0-2.5V)
  float voltage = sensorValue * (5.0 / 1023.0);
  
  // Convert to degrees (0-360)
  // Since sensor outputs 0-2.5V for 0-360°, we scale accordingly
  float direction = (voltage / 2.5) * 360.0;
  
  // Keep direction in 0-360 range
  if (direction < 0) direction = 0;
  if (direction > 360) direction = 360;
  
  Serial.print("Raw Value: ");
  Serial.print(sensorValue);
  Serial.print(" | Voltage: ");
  Serial.print(voltage, 2);
  Serial.print("V | Direction: ");
  Serial.print(direction, 1);
  Serial.println("°");
  
  delay(500);
}