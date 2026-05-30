/* ==========================================================================
   PROJECT   : Continuous Ambient & Probe Monitor (CAPM)
   FILE      : KMeterManager.h
   PURPOSE   : KMeterISO K-type thermocouple reader over I2C.
               No external library — raw Wire reads against the STM32 firmware.

   WIRING    : SDA -> D6 / GPIO 12    SCL -> D7 / GPIO 13   (shared with RTC)
   I2C ADDR  : 0x60

   PROTOCOL  : Write register 0x00, requestFrom 4 bytes -> little-endian float
               Thermocouple range: -200 C to +1372 C (K-type)
               Returns -127.0 on read failure or open-circuit detection.
   ========================================================================== */

#ifndef KMETERMANAGER_H
#define KMETERMANAGER_H

#include <Wire.h>
#include <D1Mini_CAPM.h>

#define KMETER_ADDR      0x60
#define KMETER_REG_TEMP  0x00

// K-type thermocouple valid range — anything outside signals open circuit or fault
#define KMETER_TEMP_MIN  -200.0f
#define KMETER_TEMP_MAX  1372.0f

inline bool initKMeter() {
  Serial.println(F("\n[CAPM KMETER] Probing KMeterISO at 0x60..."));
  // Wire already begun by RTCManager — just check device presence
  Wire.beginTransmission(KMETER_ADDR);
  byte err = Wire.endTransmission();
  if (err != 0) {
    Serial.print(F("[CAPM KMETER] ERROR: No ACK at 0x60 (Wire error "));
    Serial.print(err);
    Serial.println(F(")"));
    return false;
  }
  Serial.println(F("[CAPM KMETER] KMeterISO detected and ready"));
  return true;
}

inline float readKMeterTemp(bool kmeterReady) {
  if (!kmeterReady) return -127.0f;

  Wire.beginTransmission(KMETER_ADDR);
  Wire.write(KMETER_REG_TEMP);
  if (Wire.endTransmission() != 0) return -127.0f;

  if (Wire.requestFrom(KMETER_ADDR, (uint8_t)4) < 4) return -127.0f;

  uint8_t buf[4];
  for (uint8_t i = 0; i < 4; i++) buf[i] = Wire.read();

  float temp;
  memcpy(&temp, buf, sizeof(temp));

  // Reject readings outside the physical K-type range — indicates open circuit
  if (temp < KMETER_TEMP_MIN || temp > KMETER_TEMP_MAX) return -127.0f;

  return temp;
}

#endif
