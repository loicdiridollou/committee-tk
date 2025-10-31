const int sensorPin = 2;
volatile unsigned long lastPulseTime = 0;
volatile int pulseCount = 0;
unsigned long lastTime = 0;
const unsigned long debounceDelay = 10;  // 10ms debounce

void setup() {
  Serial.begin(115200);
  pinMode(sensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sensorPin), countPulse, FALLING);
  
  delay(2000);
  Serial.println("RS-FSJT-NPN Anemometer - Working!");
}

void loop() {
  unsigned long currentTime = millis();
  
  // Calculate wind speed every 2 seconds
  if (currentTime - lastTime >= 2000) {
    // Wind speed: pulses per second * calibration factor
    float pulsesPerSecond = pulseCount;
    float windSpeed = pulsesPerSecond * 0.315;
    
    Serial.print("Pulses: ");
    Serial.print(pulseCount);
    Serial.print(" | Wind Speed: ");
    Serial.print(windSpeed, 1);
    Serial.println(" km/h");
    
    pulseCount = 0;
    lastTime = currentTime;
  }
}

void countPulse() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastPulseTime > debounceDelay) {
    pulseCount++;
    lastPulseTime = currentTime;
  }
}