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
  float kmeterTemp;   // KMeterISO meat/needle probe — -127.0 when not connected
  float batteryVolts;
  float batteryPct;
};

const int MAX_POINTS = 240; 
DataPoint dataLog[MAX_POINTS];
int logCount = 0;
int logIndex = 0; 

unsigned long lastLogTime = 0;
unsigned long lastHeartbeatTime = 0;
unsigned long totalLogCount = 0;
String globalIP = "0.0.0.0";
bool lastWifiState = false;

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
  if (isnan(t_dht) || isnan(h_dht)) {
    delay(500);
    t_dht = dht.readTemperature();
    h_dht = dht.readHumidity();
  }
  
  dsSensors.requestTemperatures();
  float t_probe = (dsSensors.getDeviceCount() > 0) ? dsSensors.getTempCByIndex(0) : -127.0;

  float b_volts = 0.0;
  float b_pct = readBatteryPercentage(b_volts);

  String curTime = getFormattedSystemTime();
  dataLog[logIndex].timeString  = curTime;
  dataLog[logIndex].dhtTemp     = isnan(t_dht) ? -127.0 : t_dht;
  dataLog[logIndex].dhtHum      = isnan(h_dht) ? -127.0 : h_dht;
  dataLog[logIndex].dsProbe     = t_probe;
  dataLog[logIndex].kmeterTemp  = -127.0;  // placeholder until KMeterISO hardware arrives
  dataLog[logIndex].batteryVolts = b_volts;
  dataLog[logIndex].batteryPct  = b_pct;

  totalLogCount++;

  Serial.println(F("+-----------------------------------------------+"));
  Serial.print(F("| Data Point #                : ")); Serial.println(totalLogCount);
  Serial.print(F("| Local Frame Timestamp       : ")); Serial.println(curTime);
  Serial.print(F("| Device IP Address           : ")); Serial.println(globalIP);
  Serial.print(F("| DHT11 Ambient Air Temp      : ")); Serial.print(t_dht, 1); Serial.println(F(" °C"));
  Serial.print(F("| DHT11 Ambient Air Humidity  : ")); Serial.print(h_dht, 0); Serial.println(F(" %"));
  Serial.print(F("| DS18B20 Sensor              : ")); Serial.print(t_probe, 1); Serial.println(F(" °C"));
  Serial.print(F("| KMeterISO Needle Probe Temp : ")); Serial.println(F("-- (not connected)"));
  Serial.print(F("| Battery Level               : ")); Serial.print(b_volts, 2); Serial.print(F("V (")); Serial.print(b_pct, 0); Serial.println(F("%)"));
  Serial.println(F("+-----------------------------------------------+"));

  logIndex = (logIndex + 1) % MAX_POINTS;
  if (logCount < MAX_POINTS) logCount++;
}

void handleRoot() {
  server.sendHeader("Cache-Control", "no-cache, private");
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleDataPoint() {
  if (logCount == 0) {
    logDataPoint();
  }
  
  int lastSavedIdx = (logIndex - 1 + MAX_POINTS) % MAX_POINTS;
  
  String json = "{";
  json += "\"time_string\":\"" + dataLog[lastSavedIdx].timeString + "\",";
  json += "\"dht_temp\":"          + String(dataLog[lastSavedIdx].dhtTemp, 1)      + ",";
  json += "\"dht_humidity\":"      + String(dataLog[lastSavedIdx].dhtHum, 0)       + ",";
  json += "\"ds_probe\":"          + String(dataLog[lastSavedIdx].dsProbe, 1)      + ",";
  json += "\"kmeter_temp\":"       + String(dataLog[lastSavedIdx].kmeterTemp, 1)   + ",";
  json += "\"battery_volts\":"     + String(dataLog[lastSavedIdx].batteryVolts, 2) + ",";
  json += "\"battery_percentage\":" + String(dataLog[lastSavedIdx].batteryPct, 0);
  json += "}";
  
  /* Force a precise content length declaration */
  server.setContentLength(json.length());
  server.sendHeader("Cache-Control", "no-cache, private");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  
  server.send(200, "application/json", json);
}

void handleFullDataDump() {
  Serial.println(F("[CAPM HTTP] Streaming historical array via chunked transfer..."));

  server.sendHeader("Cache-Control", "no-cache, private");
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "application/json", "");

  if (logCount == 0) {
    server.sendContent("[]");
    server.sendContent("");
    return;
  }

  int startIdx = (logCount == MAX_POINTS) ? logIndex : 0;
  String batch = "[";

  for (int i = 0; i < logCount; i++) {
    int idx = (startIdx + i) % MAX_POINTS;

    batch += "{";
    batch += "\"time_string\":\"" + dataLog[idx].timeString + "\",";
    batch += "\"dht_temp\":"           + String(dataLog[idx].dhtTemp, 1)      + ",";
    batch += "\"dht_humidity\":"       + String(dataLog[idx].dhtHum, 0)       + ",";
    batch += "\"ds_probe\":"           + String(dataLog[idx].dsProbe, 1)      + ",";
    batch += "\"kmeter_temp\":"        + String(dataLog[idx].kmeterTemp, 1)   + ",";
    batch += "\"battery_volts\":"      + String(dataLog[idx].batteryVolts, 2) + ",";
    batch += "\"battery_percentage\":" + String(dataLog[idx].batteryPct, 0)   + "}";

    bool last = (i == logCount - 1);
    if (!last) batch += ",";

    // Flush every 5 entries to keep chunk count low and avoid watchdog resets
    if ((i % 5 == 4) || last) {
      if (last) batch += "]";
      server.sendContent(batch);
      batch = "";
      yield();
    }
  }

  server.sendContent("");  // terminate chunked transfer
}

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); }
  delay(1500); 

  // BOARD_LED (GPIO 2) is intentionally left unconfigured — it shares the
  // DHT11 data pin on the stacked shield and must not be driven as OUTPUT.
  pinMode(GREEN_LED_PIN, OUTPUT);
  pinMode(RED_LED_PIN, OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);

  digitalWrite(GREEN_LED_PIN, LOW);
  digitalWrite(RED_LED_PIN, LOW);
  digitalWrite(BUZZER_PIN, HIGH);

  runFullHardwareTestSuite();

  globalIP = connectWiFi();
  synchronizeNTP();

  dht.begin();
  dsSensors.begin();
  delay(2000);   // DHT11 requires 1s+ after begin() before first valid read
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

  bool wifiNow = (WiFi.status() == WL_CONNECTED);
  if (wifiNow != lastWifiState) {
    lastWifiState = wifiNow;
    Serial.println(wifiNow ? F("[CAPM NET] WiFi reconnected.") : F("[CAPM NET] WiFi lost."));
  }

  unsigned long currentMillis = millis();
  if (currentMillis - lastLogTime >= LOG_INTERVAL) {
    lastLogTime = currentMillis;
    logDataPoint();
  }
}