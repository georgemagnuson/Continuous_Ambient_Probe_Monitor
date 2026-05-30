/* ==========================================================================
   PROJECT   : Continuous Ambient & Probe Monitor (CAPM)
   FILE      : Config.h
   PURPOSE   : CAPM-specific constants — timing, NTP, and network credentials.
               Pin map lives in the shared library: #include <D1Mini_CAPM.h>
   ========================================================================== */

#ifndef CONFIG_H
#define CONFIG_H

// --- Network Credentials ---
// Real credentials are in Config_private.h (excluded from version control)
#include "Config_private.h"
const char* const WIFI_SSID = WIFI_SSID_PRIVATE;
const char* const WIFI_PASS = WIFI_PASS_PRIVATE;

// --- Automated NTP Time Zone Configuration ---
const char* const NTP_SERVER = "nz.pool.ntp.org";
const char* const TZ_INFO = "NZST-12NZDT,M9.5.0,M4.1.0/3"; // Auto-shifts for Auckland

// --- Logging & Notification Baselines ---
const unsigned long LOG_INTERVAL       = 15000;  // Testing: 15s. Production: 300000 (5 min)
const unsigned long HEARTBEAT_INTERVAL =  3000;  // Visual pulse speed indicator
const unsigned long HEARTBEAT_DURATION =    80;  // Active pulse window duration

#endif
