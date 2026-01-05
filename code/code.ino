#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <WiFiS3.h>

#define QMC5883L_ADDR 0x0D

// WiFi AP settings
const char* ap_ssid = "WeatherStation";
const char* ap_password = "weather123";  // At least 8 characters
WiFiServer server(80);

// SD Card pin
bool SD_card_connected = false;
const int chipSelect = 10;
File dataFile;
String currentFilename = "";
bool fileReady = false;

// Your calibration values
float x_min = -243.0;
float x_max = 998.0;
float y_min = -928.0;
float y_max = 400.0;

// Calculate offsets and scales
float x_offset = (x_max + x_min) / 2.0;
float y_offset = (y_max + y_min) / 2.0;
float x_scale = (x_max - x_min) / 2.0;
float y_scale = (y_max - y_min) / 2.0;
float avg_scale = (x_scale + y_scale) / 2.0;

// GPS setup
TinyGPSPlus gps;
SoftwareSerial gpsSerial(3, 4);  // GPS TX to pin 3, RX to pin 4

const int windVanePin = A0;
const int sensorPin = 2;
volatile unsigned long lastPulseTime = 0;
volatile int pulseCount = 0;
unsigned long lastTime = 0;
const unsigned long debounceDelay = 10;  // 10ms debounce

// Averaging array for compass (5 seconds of data at 500ms intervals = 10 samples)
const int NUM_SAMPLES = 10;
float compassReadings[NUM_SAMPLES];
int sampleIndex = 0;
int samplesCollected = 0;

// Store latest values for web display
float latest_wind_speed = 0;
float latest_wind_direction = 0;
float latest_heading = 0;

// Historical data storage (15 minutes at 1-minute intervals)
const int HISTORY_SIZE = 15;
struct HistoricalData {
  unsigned long timestamp;  // millis() when recorded
  float windSpeed;
  float windDirection;
};
HistoricalData history[HISTORY_SIZE];
int historyIndex = 0;
int historyCount = 0;
unsigned long lastHistoryTime = 0;

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(115200);
  Wire.begin();

  // Initialize QMC5883L
  Wire.beginTransmission(QMC5883L_ADDR);
  Wire.write(0x0B);
  Wire.write(0x01);
  Wire.endTransmission();
  delay(100);
  
  Wire.beginTransmission(QMC5883L_ADDR);
  Wire.write(0x09);
  Wire.write(0x1D);
  Wire.endTransmission();

  delay(2000);
  Serial.println("CYC-FX1 Wind Direction Sensor Test");

  pinMode(sensorPin, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(sensorPin), countPulse, FALLING);
  
  delay(2000);
  Serial.println("RS-FSJT-NPN Anemometer - Working!");

  // Initialize averaging array
  for (int i = 0; i < NUM_SAMPLES; i++) {
    compassReadings[i] = 0;
  }
  
  // Initialize history
  for (int i = 0; i < HISTORY_SIZE; i++) {
    history[i].timestamp = 0;
    history[i].windSpeed = 0;
    history[i].windDirection = 0;
  }

  // Initialize WiFi Access Point with static IP
  Serial.println("Creating Access Point...");
  
  // Stop any existing WiFi connections
  WiFi.end();
  delay(1000);
  
  // Configure static IP address
  IPAddress local_ip(192, 168, 1, 1);      // Your desired IP
  IPAddress gateway(192, 168, 1, 1);       // Gateway (usually same as IP for AP)
  IPAddress subnet(255, 255, 255, 0);      // Subnet mask
  
  WiFi.config(local_ip, gateway, subnet);
  
  // Start Access Point
  int status = WiFi.beginAP(ap_ssid, ap_password);
  
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed!");
    while (true);
  }
  
  delay(2000); // Give it time to start
  
  IPAddress ip = WiFi.localIP();
  Serial.print("AP IP address: ");
  Serial.println(ip);
  Serial.print("Connect to WiFi SSID: ");
  Serial.println(ap_ssid);
  Serial.print("Password: ");
  Serial.println(ap_password);
  
  server.begin();
  Serial.println("Web server started");

  // Initialize SD card
  if (!SD.begin(chipSelect)) {
    Serial.println("SD card initialization failed!");
  }else{
    SD_card_connected = true;
    Serial.println("SD card initialized");
  }
  Serial.println("Waiting for GPS date to create filename...");
}

void loop() {
  unsigned long currentTime = millis();

  // Handle web clients
  handleWebClients();

  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  // Initialize file once we have GPS date
  if (!fileReady && gps.date.isValid() && gps.time.isValid()) {
    initializeLogFile();
  }

  // Collect samples every 500ms for averaging
  static unsigned long lastSampleTime = 0;
  if (currentTime - lastSampleTime >= 500) {
    // Read and store compass heading
    compassReadings[sampleIndex] = readCompass();
    
    // Update sample index
    sampleIndex = (sampleIndex + 1) % NUM_SAMPLES;
    if (samplesCollected < NUM_SAMPLES) {
      samplesCollected++;
    }
    
    lastSampleTime = currentTime;
  }

  // Calculate wind speed every 2 seconds
  if (currentTime - lastTime >= 2000) {
    latest_wind_speed = calculateWindSpeed();
    Serial.print("Wind Speed: ");
    Serial.print(latest_wind_speed, 1);
    Serial.println("kts");

    latest_wind_direction = readWindDirection();
    Serial.print("Wind direction: ");
    Serial.print(latest_wind_direction, 1);
    Serial.println("°");

    printGPSDateTime();

    latest_heading = getAverageCompass();
    Serial.print("Heading: ");
    Serial.print(latest_heading);
    Serial.println("°");

    // Log data to SD card only if file is ready and we have enough samples
    if (SD_card_connected && fileReady && samplesCollected >= NUM_SAMPLES) {
      logDataToSD(latest_wind_speed, latest_wind_direction, latest_heading);
    }

    lastTime = millis();
  }
  
  // Store historical data every minute
  if (currentTime - lastHistoryTime >= 60000) {  // 60000ms = 1 minute
    storeHistoricalData(latest_wind_speed, latest_wind_direction);
    lastHistoryTime = currentTime;
  }
  
  delay(100);
}

void handleWebClients() {
  WiFiClient client = server.available();
  
  if (client) {
    Serial.println("New client connected");
    String currentLine = "";
    String request = "";
    
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        request += c;
        
        if (c == '\n') {
          if (currentLine.length() == 0) {
            
            // Check for button actions in the request
            if (request.indexOf("GET /reset") >= 0) {
              Serial.println("Reset button clicked!");
              handleResetCommand();
            }
            else if (request.indexOf("GET /calibrate") >= 0) {
              Serial.println("Calibrate button clicked!");
              handleCalibrateCommand();
            }
            else if (request.indexOf("GET /newfile") >= 0) {
              Serial.println("New file button clicked!");
              handleNewFileCommand();
            }
            
            // Send HTTP response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();
            
            // Send HTML page
            client.println("<!DOCTYPE html><html><head>");
            client.println("<meta name='viewport' content='width=device-width, initial-scale=1'>");
            client.println("<meta http-equiv='refresh' content='2'>");
            client.println("<style>");
            client.println("body { font-family: Arial; margin: 20px; background: #f0f0f0; }");
            client.println(".container { max-width: 600px; margin: 0 auto; background: white; padding: 20px; border-radius: 10px; box-shadow: 0 2px 10px rgba(0,0,0,0.1); }");
            client.println("h1 { color: #333; text-align: center; }");
            client.println(".reading { background: #e8f4f8; padding: 15px; margin: 10px 0; border-radius: 5px; }");
            client.println(".label { font-weight: bold; color: #666; }");
            client.println(".value { font-size: 24px; color: #007bff; }");
            client.println(".gps { background: #f8f9e8; }");
            client.println(".button { display: inline-block; padding: 10px 20px; margin: 5px; background: #007bff; color: white; text-decoration: none; border-radius: 5px; text-align: center; }");
            client.println(".button:hover { background: #0056b3; }");
            client.println(".button.danger { background: #dc3545; }");
            client.println(".button.danger:hover { background: #c82333; }");
            client.println(".button.success { background: #28a745; }");
            client.println(".button.success:hover { background: #218838; }");
            client.println(".controls { text-align: center; margin: 20px 0; padding: 15px; background: #f8f9fa; border-radius: 5px; }");
            client.println("table { width: 100%; border-collapse: collapse; margin: 20px 0; }");
            client.println("th, td { padding: 10px; text-align: left; border-bottom: 1px solid #ddd; }");
            client.println("th { background: #007bff; color: white; }");
            client.println("tr:hover { background: #f5f5f5; }");
            client.println(".history-section { margin: 20px 0; }");
            client.println("</style>");
            client.println("</head><body>");
            
            client.println("<div class='container'>");
            client.println("<h1>Weather Station</h1>");
            
            // Wind Speed
            client.println("<div class='reading'>");
            client.println("<div class='label'>Wind Speed</div>");
            client.print("<div class='value'>");
            client.print(latest_wind_speed, 1);
            client.println(" kts</div></div>");
            
            // Wind Direction
            client.println("<div class='reading'>");
            client.println("<div class='label'>Wind Direction</div>");
            client.print("<div class='value'>");
            client.print(latest_wind_direction, 1);
            client.println("&#xB0;</div></div>");
            
            // Compass Heading
            client.println("<div class='reading'>");
            client.println("<div class='label'>Compass Heading</div>");
            client.print("<div class='value'>");
            client.print(latest_heading, 1);
            client.println("&#xB0;</div></div>");
            
            // GPS Position
            client.println("<div class='reading gps'>");
            client.println("<div class='label'>GPS Position</div>");
            if (gps.location.isValid()) {
              client.print("<div class='value' style='font-size: 18px;'>");
              client.print(gps.location.lat(), 6);
              client.print(", ");
              client.print(gps.location.lng(), 6);
              client.println("</div>");
            } else {
              client.println("<div class='value'>Waiting for GPS...</div>");
            }
            client.println("</div>");
            
            // GPS Date/Time
            client.println("<div class='reading gps'>");
            client.println("<div class='label'>GPS Date/Time</div>");
            if (gps.date.isValid() && gps.time.isValid()) {
              client.print("<div class='value' style='font-size: 18px;'>");
              client.print(gps.date.year());
              client.print("-");
              if (gps.date.month() < 10) client.print("0");
              client.print(gps.date.month());
              client.print("-");
              if (gps.date.day() < 10) client.print("0");
              client.print(gps.date.day());
              client.print(" ");
              if (gps.time.hour() < 10) client.print("0");
              client.print(gps.time.hour());
              client.print(":");
              if (gps.time.minute() < 10) client.print("0");
              client.print(gps.time.minute());
              client.print(":");
              if (gps.time.second() < 10) client.print("0");
              client.print(gps.time.second());
              client.println("</div>");
            } else {
              client.println("<div class='value'>Waiting for GPS...</div>");
            }
            client.println("</div>");
            
            // Satellites
            client.println("<div class='reading gps'>");
            client.println("<div class='label'>Satellites</div>");
            client.print("<div class='value'>");
            client.print(getNumberOfSatellites());
            client.println("</div></div>");
            
            // Historical data table
            client.println("<div class='history-section'>");
            client.println("<h2>Wind History (Last 15 Minutes)</h2>");
            client.println("<table>");
            client.println("<tr><th>Time Ago</th><th>Wind Speed (kts)</th><th>Wind Direction (&#xB0;)</th></tr>");
            
            if (historyCount == 0) {
              client.println("<tr><td colspan='3' style='text-align: center;'>No historical data yet...</td></tr>");
            } else {
              // Display history from newest to oldest
              for (int i = 0; i < historyCount; i++) {
                int idx = (historyIndex - 1 - i + HISTORY_SIZE) % HISTORY_SIZE;
                if (idx < 0) idx += HISTORY_SIZE;
                
                unsigned long minutesAgo = (millis() - history[idx].timestamp) / 60000;
                
                client.println("<tr>");
                client.print("<td>");
                client.print(minutesAgo);
                client.print(" min ago</td>");
                client.print("<td>");
                client.print(history[idx].windSpeed, 1);
                client.print("</td>");
                client.print("<td>");
                client.print(history[idx].windDirection, 1);
                client.println("</td></tr>");
              }
            }
            
            client.println("</table>");
            client.println("</div>");
            
            // Control buttons
            client.println("<div class='controls'>");
            client.println("<h3>Controls</h3>");
            client.println("<a href='/reset' class='button danger'>Reset Pulse Count</a>");
            client.println("<a href='/calibrate' class='button success'>Start Calibration</a>");
            client.println("<a href='/newfile' class='button'>Create New Log File</a>");
            client.println("</div>");
            
            client.println("<p style='text-align: center; color: #999; margin-top: 20px;'>Auto-refresh every 2 seconds</p>");
            client.println("</div></body></html>");
            
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }
      }
    }
    
    client.stop();
    Serial.println("Client disconnected");
  }
}

void initializeLogFile() {
  // Create filename with GPS date (format: YYYYMMDD.CSV)
  char filename[20];
  sprintf(filename, "%04d%02d%02d.CSV", gps.date.year(), gps.date.month(), gps.date.day());
  currentFilename = String(filename);
  
  Serial.print("Using file: ");
  Serial.println(currentFilename);

  // Check if file exists, if not create with headers
  if (!SD.exists(currentFilename.c_str())) {
    Serial.println("Creating new file with headers...");
    dataFile = SD.open(currentFilename.c_str(), FILE_WRITE);
    if (dataFile) {
      // Write CSV headers
      dataFile.println("Timestamp,Latitude,Longitude,Satellites,Wind_Speed_kts,Wind_Direction_deg,Heading_deg");
      dataFile.close();
      Serial.println("Headers written successfully");
      fileReady = true;
    } else {
      Serial.println("Error creating file!");
    }
  } else {
    Serial.println("File exists, will append data");
    fileReady = true;
  }
}

void logDataToSD(float windSpeed, float windDirection, float heading) {
  dataFile = SD.open(currentFilename.c_str(), FILE_WRITE);
  
  if (dataFile) {
    // Timestamp from GPS
    if (gps.date.isValid() && gps.time.isValid()) {
      dataFile.print(gps.date.year());
      dataFile.print("-");
      if (gps.date.month() < 10) dataFile.print("0");
      dataFile.print(gps.date.month());
      dataFile.print("-");
      if (gps.date.day() < 10) dataFile.print("0");
      dataFile.print(gps.date.day());
      dataFile.print(" ");
      if (gps.time.hour() < 10) dataFile.print("0");
      dataFile.print(gps.time.hour());
      dataFile.print(":");
      if (gps.time.minute() < 10) dataFile.print("0");
      dataFile.print(gps.time.minute());
      dataFile.print(":");
      if (gps.time.second() < 10) dataFile.print("0");
      dataFile.print(gps.time.second());
    } else {
      dataFile.print("N/A");
    }
    dataFile.print(",");
    
    // GPS Position
    if (gps.location.isValid()) {
      dataFile.print(gps.location.lat(), 6);
      dataFile.print(",");
      dataFile.print(gps.location.lng(), 6);
    } else {
      dataFile.print("N/A,N/A");
    }
    dataFile.print(",");
    
    // Number of satellites
    dataFile.print(getNumberOfSatellites());
    dataFile.print(",");
    
    // Wind speed
    dataFile.print(windSpeed, 2);
    dataFile.print(",");
    
    // Wind direction
    dataFile.print(windDirection, 2);
    dataFile.print(",");
    
    // Heading
    dataFile.println(heading, 2);
    
    dataFile.close();
    Serial.println("Data logged to SD card");
  } else {
    Serial.println("Error opening file for writing!");
  }
}

float readCompass() {
  Wire.beginTransmission(QMC5883L_ADDR);
  Wire.write(0x00);
  Wire.endTransmission();
  Wire.requestFrom(QMC5883L_ADDR, 6);
  float heading = -1;
  
  if (Wire.available() >= 6) {
    int16_t x_raw = Wire.read() | (Wire.read() << 8);
    int16_t y_raw = Wire.read() | (Wire.read() << 8);
    int16_t z_raw = Wire.read() | (Wire.read() << 8);
    
    // Apply calibration
    float x_cal = (x_raw - x_offset) * (avg_scale / x_scale);
    float y_cal = (y_raw - y_offset) * (avg_scale / y_scale);
    
    // Calculate heading
    heading = atan2(y_cal, x_cal) * 180.0 / PI;
    if (heading < 0) heading += 360;
  }
  return heading;
}

float getAverageCompass() {
  // Calculate average of compass readings using circular mean
  float sumSin = 0;
  float sumCos = 0;
  
  for (int i = 0; i < samplesCollected; i++) {
    float radians = compassReadings[i] * PI / 180.0;
    sumSin += sin(radians);
    sumCos += cos(radians);
  }
  
  float avgRadians = atan2(sumSin / samplesCollected, sumCos / samplesCollected);
  float avgHeading = avgRadians * 180.0 / PI;
  
  if (avgHeading < 0) avgHeading += 360;
  
  return avgHeading;
}

int getNumberOfSatellites() {
  if (gps.satellites.isValid()) {
    return gps.satellites.value();
  } else {
    return 0;
  }
}

void printGPSDateTime() {
  Serial.print("GPS Date/Time: ");
  if (gps.date.isValid() && gps.time.isValid()) {
    Serial.print(gps.date.month());
    Serial.print("/");
    Serial.print(gps.date.day());
    Serial.print("/");
    Serial.print(gps.date.year());
    Serial.print(" ");
    if (gps.time.hour() < 10) Serial.print("0");
    Serial.print(gps.time.hour());
    Serial.print(":");
    if (gps.time.minute() < 10) Serial.print("0");
    Serial.print(gps.time.minute());
    Serial.print(":");
    if (gps.time.second() < 10) Serial.print("0");
    Serial.print(gps.time.second());
  } else {
    Serial.print("Waiting for date and time data...");
  }
  Serial.println();

  Serial.print("GPS Position: ");
  Serial.print(gps.location.lat(), 6);
  Serial.print(" - ");
  Serial.print(gps.location.lng(), 6);
  Serial.println("");
}

float calculateWindSpeed() {
    // Wind speed: pulses per second * calibration factor
    float pulsesPerSecond = pulseCount / 2.0;  // Divided by 2 since we measure over 2 seconds
    float windSpeed = pulsesPerSecond * 0.315;
    
    pulseCount = 0;

    return windSpeed / 1.852; // speed in knots
}

float readWindDirection() {
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

  return direction;
}

void countPulse() {
  unsigned long currentTime = millis();
  
  if (currentTime - lastPulseTime > debounceDelay) {
    pulseCount++;
    lastPulseTime = currentTime;
  }
}

// Command handler functions - customize these for your needs
void handleResetCommand() {
  // Example: Reset pulse count
  pulseCount = 0;
  Serial.println("Pulse count reset to 0");
}

void handleCalibrateCommand() {
  // Example: Start calibration routine
  Serial.println("Starting calibration...");
  // Add your calibration code here
  // For example: read multiple compass values, find min/max, etc.
}

void handleNewFileCommand() {
  // Example: Force creation of new log file
  fileReady = false;
  Serial.println("Forcing new file creation on next GPS lock");
}

void storeHistoricalData(float windSpeed, float windDirection) {
  // Store new data point
  history[historyIndex].timestamp = millis();
  history[historyIndex].windSpeed = windSpeed;
  history[historyIndex].windDirection = windDirection;
  
  // Move to next position
  historyIndex = (historyIndex + 1) % HISTORY_SIZE;
  
  // Track how many entries we have (max 15)
  if (historyCount < HISTORY_SIZE) {
    historyCount++;
  }
  
  Serial.print("Stored historical data point #");
  Serial.println(historyCount);
}
