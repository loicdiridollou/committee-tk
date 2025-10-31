#include "WiFiS3.h"
#include <TinyGPSPlus.h>
#include <SoftwareSerial.h>
#include "arduino_secrets.h" 

///////please enter your sensitive data in the Secret tab/arduino_secrets.h
char ssid[] = SECRET_SSID;        // your network SSID (name)
char pass[] = SECRET_PASS;        // your network password

int status = WL_IDLE_STATUS;
WiFiServer server(80);

// GPS setup
TinyGPSPlus gps;
SoftwareSerial gpsSerial(3, 4);  // GPS TX to pin 3, RX to pin 4

void setup() {
  Serial.begin(115200);
  gpsSerial.begin(38400);  // BN-220 baud rate
  
  while (!Serial) {
    ; // wait for serial port to connect
  }
  Serial.println("GPS Web Server");

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
  }

  // Set custom IP
  WiFi.config(IPAddress(192,48,56,2));

  Serial.print("Creating access point named: ");
  Serial.println(ssid);

  status = WiFi.beginAP(ssid, pass);
  if (status != WL_AP_LISTENING) {
    Serial.println("Creating access point failed");
    while (true);
  }

  delay(10000);
  server.begin();
  printWiFiStatus();
}

void loop() {
  // Read GPS data continuously
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }
  
  // Check WiFi status
  if (status != WiFi.status()) {
    status = WiFi.status();
    Serial.println(status);
    if (status == WL_AP_CONNECTED) {
      Serial.println("Device connected to AP");
    } else {
      Serial.println("Device disconnected from AP");
    }
  }
  
  WiFiClient client = server.available();

  if (client) {
    Serial.println("new client");
    String currentLine = "";
    
    while (client.connected()) {
      delayMicroseconds(10);
      
      if (client.available()) {
        char c = client.read();
        Serial.write(c);
        
        if (c == '\n') {
          if (currentLine.length() == 0) {
            // Send HTTP response
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // HTML content with GPS data
            client.println("<!DOCTYPE html><html><head>");
            client.println("<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<meta http-equiv=\"refresh\" content=\"5\">"); // Auto-refresh every 5 seconds
            client.println("<style>");
            client.println("body { font-family: Arial; text-align: center; margin-top: 50px; }");
            client.println("h1 { color: #333; }");
            client.println(".data { font-size: 24px; margin: 20px; padding: 15px; background: #f0f0f0; border-radius: 10px; }");
            client.println(".valid { color: green; }");
            client.println(".invalid { color: red; }");
            client.println("</style></head><body>");
            
            client.println("<h1>GPS Location Tracker</h1>");
            
            // Display GPS data
            if (gps.location.isValid()) {
              client.print("<div class='data valid'><strong>Latitude:</strong> ");
              client.print(gps.location.lat(), 6);
              client.println("</div>");
              
              client.print("<div class='data valid'><strong>Longitude:</strong> ");
              client.print(gps.location.lng(), 6);
              client.println("</div>");
              
              // Google Maps link
              client.print("<div class='data'><a href='https://www.google.com/maps?q=");
              client.print(gps.location.lat(), 6);
              client.print(",");
              client.print(gps.location.lng(), 6);
              client.println("' target='_blank'>View on Google Maps</a></div>");
            } else {
              client.println("<div class='data invalid'><strong>Location:</strong> Searching for GPS signal...</div>");
            }
            
            // Date and Time
            client.print("<div class='data'><strong>Date/Time:</strong> ");
            if (gps.date.isValid() && gps.time.isValid()) {
              client.print(gps.date.month());
              client.print("/");
              client.print(gps.date.day());
              client.print("/");
              client.print(gps.date.year());
              client.print(" ");
              if (gps.time.hour() < 10) client.print("0");
              client.print(gps.time.hour());
              client.print(":");
              if (gps.time.minute() < 10) client.print("0");
              client.print(gps.time.minute());
              client.print(":");
              if (gps.time.second() < 10) client.print("0");
              client.print(gps.time.second());
            } else {
              client.print("Waiting...");
            }
            client.println("</div>");
            
            // Satellites
            client.print("<div class='data'><strong>Satellites:</strong> ");
            if (gps.satellites.isValid()) {
              client.print(gps.satellites.value());
            } else {
              client.print("0");
            }
            client.println("</div>");
            
            // Altitude
            if (gps.altitude.isValid()) {
              client.print("<div class='data'><strong>Altitude:</strong> ");
              client.print(gps.altitude.meters());
              client.println(" meters</div>");
            }
            
            // Speed
            if (gps.speed.isValid()) {
              client.print("<div class='data'><strong>Speed:</strong> ");
              client.print(gps.speed.kmph());
              client.println(" km/h</div>");
            }
            
            client.println("<p style='color: #666; margin-top: 30px;'>Page refreshes automatically every 5 seconds</p>");
            client.println("</body></html>");

            client.println();
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
    Serial.println("client disconnected");
  }
}

void printWiFiStatus() {
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);
  Serial.print("To see GPS data, open a browser to http://");
  Serial.println(ip);
}