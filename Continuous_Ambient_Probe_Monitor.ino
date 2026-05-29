/* ==========================================================================
   PROJECT   : Continuous Ambient & Probe Monitor (CAPM)
   FILE      : Continuous_Ambient_Probe_Monitor.ino
   PURPOSE   : Verbose main loop scheduling and active API serving
   ========================================================================== */

#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "DHT.h"

#include "Config.h"
#include "WebTemplate.h"
#include "HardwarePOST.h"
#include "NetworkManager.h"

OneWire oneWire(DS18B20_PIN);
DallasTemperature dsSensors(&oneWire);
DHT dht(DHT_PIN, DHTTYPE);
ESP8266WebServer server(80);

struct DataPoint {
  String timeString; 
  float dhtTemp;
  float dhtHum;
  float dsProbe;
  float batteryVolts;
  float batteryPct; 
};

const int MAX_POINTS = 240; 
DataPoint dataLog[MAX_POINTS];
int logCount = 0;
int logIndex = 0; 

unsigned long lastLogTime = 0;
unsigned long lastHeartbeatTime = 0;
String globalIP = "0.0.0.0";

float readBatteryPercentage(float& outVolts) {
  int rawADC = analogRead(BATTERY_PIN);
  outVolts = ((float)rawADC / 1023.0) * 4.2; 
  if (outVolts < 3.2) return 0.0;
  float percentage = ((outVolts - 3.2) / (4.2 - 3.2)) * 100.0;
  return (percentage > 100.0) ? 100.0 : ((percentage < 0.0) ? 0.0 : percentage);
}

void handleHeartbeatSignal() {
  unsigned long currentMillis = millis();
  if (currentMillis - lastHeartbeatTime >= HEARTBEAT_INTERVAL) {
    lastHeartbeatTime = currentMillis;
    digitalWrite(GREEN_LED_PIN, HIGH); 
  }
  if (digitalRead(GREEN_LED_PIN) == HIGH && (currentMillis - lastHeartbeatTime >= HEARTBEAT_DURATION)) {
    digitalWrite(GREEN_LED_PIN, LOW);  
  }
}

void logDataPoint() {
  Serial.println(F("\n[CAPM RECORDER] Polling sensors for data capture..."));

  float t_dht = dht.readTemperature();
  float h_dht = dht.readHumidity();
  
  dsSensors.requestTemperatures();
  float t_probe = (dsSensors.getDeviceCount() > 0) ? dsSensors.getTempCByIndex(0) : -127.0;

  float b_volts = 0.0;
  float b_pct = readBatteryPercentage(b_volts);

  String curTime = getFormattedSystemTime();
  dataLog[logIndex].timeString = curTime;
  dataLog[logIndex].dhtTemp = isnan(t_dht) ? -127.0 : t_dht;
  dataLog[logIndex].dhtHum = isnan(h_dht) ? -127.0 : h_dht;
  dataLog[logIndex].dsProbe = t_probe;
  dataLog[logIndex].batteryVolts = b_volts;
  dataLog[logIndex].batteryPct = b_pct;

  Serial.println(F("+-------------------------------------------+"));
  Serial.print(F("| Local Frame Timestamp : ")); Serial.println(curTime);
  Serial.print(F("| DHT11 Air Temp        : ")); Serial.print(t_dht, 1); Serial.println(F(" °C"));
  Serial.print(F("| DHT11 Humidity        : ")); Serial.print(h_dht, 0); Serial.println(F(" %"));
  Serial.print(F("| DS18B20 Water Probe   : ")); Serial.print(t_probe, 1); Serial.println(F(" °C"));
  Serial.print(F("| Battery Level         : ")); Serial.print(b_volts, 2); Serial.print(F("V (")); Serial.print(b_pct, 0); Serial.println(F("%)"));
  Serial.println(F("+-------------------------------------------+"));

  logIndex = (logIndex + 1) % MAX_POINTS;
  if (logCount < MAX_POINTS) logCount++;
}

void handleRoot() {
  // Clear any residual content-length overrides from other endpoints
  server.sendHeader("Cache-Control", "no-cache, private");
  
  // Send the PROGMEM string directly. 
  // The ESP8266 core handles the size and packet slicing flawlessly behind the scenes.
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleDataPoint() {
  if (logCount == 0) {
    logDataPoint();
  }
  
  int lastSavedIdx = (logIndex - 1 + MAX_POINTS) % MAX_POINTS;
  
  String json = "{";
  json += "\"time_string\":\"" + dataLog[lastSavedIdx].timeString + "\",";
  json += "\"dht_temp\":" + String(dataLog[lastSavedIdx].dhtTemp, 1) + ",";
  json += "\"dht_humidity\":" + String(dataLog[lastSavedIdx].dhtHum, 0) + ",";
  json += "\"ds_probe\":" + String(dataLog[lastSavedIdx].dsProbe, 1) + ",";
  json += "\"battery_volts\":" + String(dataLog[lastSavedIdx].batteryVolts, 2) + ",";
  json += "\"battery_percentage\":" + String(dataLog[lastSavedIdx].batteryPct, 0);
  json += "}";
  
  /* Force a precise content length declaration */
  server.setContentLength(json.length());
  server.sendHeader("Cache-Control", "no-cache, private");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  server.send(200, "application/json", json);
}

void handleFullDataDump() {
  Serial.println(F("[CAPM HTTP] Compiling historical array into pristine flat buffer..."));
  
  if (logCount == 0) {
    /* Send a clean, simple empty array response */
    server.sendHeader("Cache-Control", "no-cache, private");
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", "[]");
    return;
  }

  /* 1. Assemble the entire JSON payload smoothly in memory */
  String fullJson = "[";
  int startIdx = (logCount == MAX_POINTS) ? logIndex : 0;
  
  for (int i = 0; i < logCount; i++) {
    int currentIdx = (startIdx + i) % MAX_POINTS;
    
    fullJson += "{";
    fullJson += "\"time_string\":\"" + dataLog[currentIdx].timeString + "\",";
    fullJson += "\"dht_temp\":" + String(dataLog[currentIdx].dhtTemp, 1) + ",";
    fullJson += "\"dht_humidity\":" + String(dataLog[currentIdx].dhtHum, 0) + ",";
    fullJson += "\"ds_probe\":" + String(dataLog[currentIdx].dsProbe, 1) + ",";
    fullJson += "\"battery_volts\":" + String(dataLog[currentIdx].batteryVolts, 2) + ",";
    fullJson += "\"battery_percentage\":" + String(dataLog[currentIdx].batteryPct, 0);
    fullJson += "}";
    
    if (i < logCount - 1) {
      fullJson += ",";
    }
  }
  fullJson += "]";

  /* 2. Set headers WITHOUT setContentLength. Let the core engine handle transmission windows */
  server.sendHeader("Cache-Control", "no-cache, private");
  server.sendHeader("Access-Control-Allow-Origin", "*");

  /* 3. Deliver the monolithic string pass. The library handles frame splits natively */
  server.send(200, "application/json", fullJson);
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(1500); 

  pinMode(BOARD_LED, OUTPUT);
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(BOARD_LED, LOW); 
  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);

  runFullHardwareTestSuite();

  globalIP = connectWiFi();  
  synchronizeNTP();         
  
  digitalWrite(BOARD_LED, HIGH); 

  dht.begin();
  dsSensors.begin();
  logDataPoint(); 

  Serial.println(F("\n[CAPM SYSTEM] Initializing API listener paths..."));
  server.on("/", handleRoot);
  server.on("/data", handleDataPoint);
  server.on("/full_data", handleFullDataDump);
  server.on("/favicon.ico", []() { server.send(204, "text/plain", ""); }); // Stops 404 errors safely
  server.begin();
  Serial.println(F("[CAPM SYSTEM] HTTP Server Active. Waiting for clients..."));
}

void loop() {
  server.handleClient();
  handleHeartbeatSignal();

  if (WiFi.status() != WL_CONNECTED) {
    digitalWrite(BOARD_LED, LOW); 
  } else {
    digitalWrite(BOARD_LED, HIGH); 
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = currentMillis;
    logDataPoint();
  }
}