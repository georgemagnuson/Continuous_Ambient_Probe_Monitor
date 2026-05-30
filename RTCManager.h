/* ==========================================================================
   PROJECT   : Continuous Ambient & Probe Monitor (CAPM)
   FILE      : RTCManager.h
   PURPOSE   : DS3231 RTC initialisation, NTP sync, and time string helper.

   WIRING    : SDA -> D6 / GPIO 12    SCL -> D7 / GPIO 13
   I2C ADDR  : 0x68 (DS3231)          0x50 (AT24C32 EEPROM on same module)

   TIME FLOW
   ---------
   1. initRTC()       — Wire.begin on custom pins, detect DS3231, log result
   2. syncRTCFromNTP() — called after NTP succeeds; stamps DS3231 with UTC+TZ
   3. getRTCTimeString() — returns "HH:MM:SS"; falls back to NTP system clock
                           if RTC was not detected at boot
   ========================================================================== */

#ifndef RTCMANAGER_H
#define RTCMANAGER_H

#include <Wire.h>
#include <RTClib.h>
#include <time.h>
#include <D1Mini_CAPM.h>

inline bool initRTC(RTC_DS3231& rtc) {
  Serial.println(F("\n[CAPM RTC] Initialising DS3231 on D6/D7 (GPIO 12/13)..."));
  Wire.begin(I2C_SDA_PIN, I2C_SCL_PIN);

  if (!rtc.begin()) {
    Serial.println(F("[CAPM RTC] ERROR: DS3231 not found on I2C bus — check wiring"));
    return false;
  }

  if (rtc.lostPower()) {
    Serial.println(F("[CAPM RTC] WARNING: RTC lost power — time invalid until NTP sync"));
  } else {
    DateTime now = rtc.now();
    Serial.print(F("[CAPM RTC] Loaded time from module : "));
    char buf[20];
    snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
             now.year(), now.month(), now.day(),
             now.hour(), now.minute(), now.second());
    Serial.println(buf);
  }

  Serial.println(F("[CAPM RTC] DS3231 ready"));
  return true;
}

inline void syncRTCFromNTP(RTC_DS3231& rtc) {
  time_t sysNow = time(nullptr);
  if (sysNow < 1500000000UL) {
    Serial.println(F("[CAPM RTC] NTP time not valid — DS3231 not updated"));
    return;
  }
  // Convert UTC to local time (TZ_INFO applied by configTime) before stamping RTC
  struct tm localTm;
  localtime_r(&sysNow, &localTm);
  rtc.adjust(DateTime(localTm.tm_year + 1900, localTm.tm_mon + 1, localTm.tm_mday,
                      localTm.tm_hour, localTm.tm_min, localTm.tm_sec));
  DateTime adjusted = rtc.now();
  char buf[20];
  snprintf(buf, sizeof(buf), "%04d-%02d-%02d %02d:%02d:%02d",
           adjusted.year(), adjusted.month(), adjusted.day(),
           adjusted.hour(), adjusted.minute(), adjusted.second());
  Serial.print(F("[CAPM RTC] DS3231 synchronised from NTP: "));
  Serial.println(buf);
}

inline String getRTCTimeString(RTC_DS3231& rtc, bool rtcReady) {
  char buf[9];
  if (rtcReady) {
    DateTime t = rtc.now();
    snprintf(buf, sizeof(buf), "%02d:%02d:%02d", t.hour(), t.minute(), t.second());
    return String(buf);
  }
  // Fallback: ESP8266 system clock (set by NTP)
  time_t sysNow = time(nullptr);
  struct tm ti;
  localtime_r(&sysNow, &ti);
  snprintf(buf, sizeof(buf), "%02d:%02d:%02d", ti.tm_hour, ti.tm_min, ti.tm_sec);
  return String(buf);
}

#endif
