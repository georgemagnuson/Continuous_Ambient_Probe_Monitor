/* ==========================================================================
   PROJECT   : Continuous Ambient & Probe Monitor (CAPM)
   FILE      : NetworkManager.h
   PURPOSE   : Verbose Wi-Fi diagnostic engine and NTP tracker
   ========================================================================== */

#ifndef NETWORKMANAGER_H
#define NETWORKMANAGER_H

#include <ESP8266WiFi.h>
#include <time.h>
#include "Config.h"

// Helper function to translate hex status codes to English
inline const char* getWiFiStatusString(wl_status_t status) {
  switch (status) {
    case WL_IDLE_STATUS:     return "IDLE (Changing states)";
    case WL_NO_SSID_AVAIL:   return "NO SSID AVAILABLE (Network name not found)";
    case WL_SCAN_COMPLETED:  return "SCAN COMPLETED";
    case WL_CONNECTED:       return "CONNECTED";
    case WL_CONNECT_FAILED:  return "CONNECTION FAILED (Wrong password or rejected)";
    case WL_CONNECTION_LOST: return "CONNECTION LOST";
    case WL_DISCONNECTED:    return "DISCONNECTED (Waiting for router beacon)";
    default:                 return "UNKNOWN ERROR CODE";
  }
}

inline String connectWiFi() {
  Serial.println(F("\n+===========================================+"));
  Serial.print(F("[CAPM NET] Starting Radio Layer Interface...\n"));
  Serial.print(F("[CAPM NET] Target Network SSID : ")); Serial.println(WIFI_SSID);
  
  // Explicitly set station mode and disconnect from any old stuck sessions
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(100);

  Serial.println(F("[CAPM NET] Sending credentials packet to router..."));
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  unsigned long startAttemptTime = millis();
  unsigned long lastDiagnosticPrint = 0;
  int dotCounter = 0;

  // Loop until connected
  while (WiFi.status() != WL_CONNECTED) {
    delay(100);
    dotCounter++;
    
    // Print a dot every 100ms so you know the chip hasn't frozen
    if (dotCounter >= 10) {
      Serial.print(F("."));
      dotCounter = 0;
    }

    // Every 5 seconds, print a comprehensive network health diagnostic update
    if (millis() - lastDiagnosticPrint >= 5000) {
      lastDiagnosticPrint = millis();
      wl_status_t currentStatus = WiFi.status();
      
      Serial.println(F("\n---------------------------------------------"));
      Serial.print(F("[CAPM DIAG] Elapsed Attempt Time: ")); 
      Serial.print((millis() - startAttemptTime) / 1000); Serial.println(F(" seconds"));
      Serial.print(F("[CAPM DIAG] Current Radio State : ")); 
      Serial.println(getWiFiStatusString(currentStatus));
      
      // Request Signal Strength Indicator (RSSI)
      long rssi = WiFi.RSSI();
      Serial.print(F("[CAPM DIAG] Detected Signal Power: "));
      if (rssi == 0 || currentStatus == WL_NO_SSID_AVAIL) {
        Serial.println(F("ZERO SIGNAL (Out of physical range)"));
      } else {
        Serial.print(rssi); Serial.println(F(" dBm"));
      }
      Serial.println(F("---------------------------------------------"));
    }

    // Fail-safe protection: If stuck for more than 45 seconds, try re-initializing
    if (millis() - startAttemptTime > 45000) {
      Serial.println(F("\n[CAPM NET] Connection timeout exceeded! Re-cycling radio hardware..."));
      WiFi.disconnect();
      delay(500);
      WiFi.begin(WIFI_SSID, WIFI_PASS);
      startAttemptTime = millis();
    }
  }

  String localIP = WiFi.localIP().toString();
  Serial.println(F("\n\n[CAPM NET] >>> SUCCESS! Linked to LAN network layer."));
  Serial.print(F("[CAPM NET] Allocated Node IP  : ")); Serial.println(localIP);
  Serial.print(F("[CAPM NET] Router Gateway IP  : ")); Serial.println(WiFi.gatewayIP().toString());
  Serial.print(F("[CAPM NET] Node Mac Address   : ")); Serial.println(WiFi.macAddress());
  
  return localIP;
}

inline bool synchronizeNTP() {
  Serial.println(F("\n[CAPM NTP] Opening UDP Time Query Socket..."));
  Serial.print(F("[CAPM NTP] Target Pool: ")); Serial.println(NTP_SERVER);

  configTime(TZ_INFO, NTP_SERVER);

  int attempts = 0;
  time_t now = time(nullptr);
  
  while (now < 8 * 3600 * 2 && attempts < 20) {
    delay(500);
    now = time(nullptr);
    attempts++;
    Serial.print(F("[CAPM NTP] Waiting for clock frame lock... \n"));
  }

  if (now > 1500000000) {
    struct tm timeinfo;
    localtime_r(&now, &timeinfo);
    Serial.println(F("[CAPM NTP] >>> SUCCESS! System clock synchronized."));
    Serial.printf("[CAPM NTP] Local Time: %02d:%02d:%02d\n", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    return true;
  } else {
    Serial.println(F("[CAPM NTP] ERROR: Time sync handshakes timed out. Web dashboard will fall back to local ticks."));
    return false;
  }
}

inline String getFormattedSystemTime() {
  time_t now = time(nullptr);
  struct tm timeinfo;
  localtime_r(&now, &timeinfo);
  
  char timeStr[9];
  snprintf(timeStr, sizeof(timeStr), "%02d:%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
  return String(timeStr);
}

#endif