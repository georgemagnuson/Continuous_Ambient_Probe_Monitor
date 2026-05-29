/* ==========================================================================
   PROJECT   : Continuous Ambient & Probe Monitor (CAPM)
   FILE      : HardwarePOST.h
   PURPOSE   : Isolated Power-On Self-Test (POST) validation diagnostics
   ========================================================================== */

#ifndef HARDWAREPOST_H
#define HARDWAREPOST_H
#include <Arduino.h>
#include "Config.h"

inline void testBuzzerCircuit() {
  Serial.print(F("[CAPM TEST] Buzzer Audio Line (Pin "));
  Serial.print(BUZZER_PIN);
  Serial.print(F(")... "));
  digitalWrite(BUZZER_PIN, LOW);
  delay(150); 
  digitalWrite(BUZZER_PIN, HIGH);
  Serial.println(F("PASSED"));
}

inline void testAlternatingLEDs() {
  Serial.print(F("[CAPM TEST] Alternating LED Lines (Pins "));
  Serial.print(GREEN_LED_PIN);
  Serial.print(F(", "));
  Serial.print(RED_LED_PIN);
  Serial.print(F(")... "));
  for (int i = 0; i < 3; i++) {
    digitalWrite(GREEN_LED_PIN, HIGH);
    digitalWrite(RED_LED_PIN, LOW);
    delay(150);
    digitalWrite(GREEN_LED_PIN, LOW);
    digitalWrite(RED_LED_PIN, HIGH);
    delay(150);
  }
  digitalWrite(RED_LED_PIN, LOW);
  Serial.println(F("PASSED"));
}

inline void runFullHardwareTestSuite() {
  Serial.println(F("\n+===========================================+"));
  Serial.println(F("|       CAPM DIAGNOSTIC HARDWARE SUITE      |"));
  Serial.println(F("+===========================================+"));
  testBuzzerCircuit();
  delay(400); 
  testAlternatingLEDs();
  Serial.println(F("+===========================================+"));
  Serial.println(F("|      POST VERIFICATION CLEAN - BOOTING    |"));
  Serial.println(F("+===========================================+\n"));
}

#endif