/* ==========================================================================
   PROJECT   : Continuous Ambient & Probe Monitor (CAPM)
   FILE      : Config.h
   PURPOSE   : Hardware pin maps, system variables, and network configurations
   ========================================================================== */

#ifndef CONFIG_H
#define CONFIG_H

// --- Hardware Pin Configurations ---
#define DS18B20_PIN 5   // Pin D1 -> GPIO 5
#define DHT_PIN     2   // Pin D4 -> GPIO 2 (stacked shield — fixed by hardware)
#define DHTTYPE     DHT11
#define BOARD_LED   2   // Onboard Blue Link LED (GPIO 2 / D4) — shares pin with DHT_PIN
#define BATTERY_PIN A0  // Analog battery cell scale line

// --- Physical Notification Hardware Pins ---
#define GREEN_LED_PIN 4   // Pin D2 -> GPIO 4  (System Heartbeat)
#define RED_LED_PIN   0   // Pin D3 -> GPIO 0  (Alarm Path Offline)
#define BUZZER_PIN    14  // Pin D5 -> GPIO 14 (Siren Path Offline)

// --- Network Credentials ---
// Real credentials are in Config_private.h (excluded from version control)
#include "Config_private.h"
const char* const WIFI_SSID = WIFI_SSID_PRIVATE;
const char* const WIFI_PASS = WIFI_PASS_PRIVATE;

// --- Automated NTP Time Zone Configuration ---
const char* const NTP_SERVER = "nz.pool.ntp.org";
const char* const TZ_INFO = "NZST-12NZDT,M9.5.0,M4.1.0/3"; // Auto-shifts for Auckland

// --- Logging & Notification Baselines ---
const unsigned long LOG_INTERVAL = 15000;       // Continuous trace record (15 Sec)
const unsigned long HEARTBEAT_INTERVAL = 3000;  // Visual pulse speed indicator
const unsigned long HEARTBEAT_DURATION = 80;    // Active pulse window duration

#endif