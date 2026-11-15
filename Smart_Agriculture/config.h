#pragma once
// =====================================================
//  SMART AGRICULTURE SYSTEM - config.h
//  Every pin and every unconfirmed hardware assumption
//  lives in this file. Change it here only.
// =====================================================

// ---------- Wi-Fi Access Point ----------
#define AP_SSID "SmartAgri"      // Wi-Fi network name
#define AP_PASS "12345678"       // must be at least 8 characters

// ---------- Pins (ESP32 DevKit, GPIO numbers) ----------
#define PIN_DHT        4         // DHT11 DATA
#define PIN_SOIL_ADC   34        // Soil moisture AO   (ADC1, input only)
#define PIN_TDS_ADC    35        // TDS module  A      (ADC1, input only)
#define PIN_LDR_ADC    32        // LDR divider midpoint (ADC1)
#define PIN_RELAY      25        // Relay module IN
#define PIN_SOIL_POWER -1        // -1 = soil sensor VCC wired to 3V3 permanently
                                 // set to 27 only if you wire soil VCC to GPIO27

// ---------- Relay module polarity (UNCONFIRMED - test it) ----------
// 1 = relay turns ON when IN is LOW  (most common). OFF is done by releasing the pin (high-Z),
//     because 3.3 V HIGH may not switch a 5 V module off.
// 0 = relay turns ON when IN is HIGH. If the relay is ON at boot with 1, try 0.
#define RELAY_ACTIVE_LOW 1

// ---------- Timing ----------
#define SENSOR_PERIOD_MS 2000    // DHT11 needs >= 1000 ms between reads

// ---------- Defaults (saved settings override these) ----------
#define DEF_LOW_PCT      35      // pump starts below this soil %
#define DEF_HIGH_PCT     55      // pump stops at/above this soil %
#define DEF_MAX_RUN_SEC  30      // hard limit for one pump run
#define DEF_COOLDOWN_SEC 60      // wait after a run so water can soak in
#define DEF_SOIL_DRY_RAW 3000    // PLACEHOLDER - calibrate from dashboard
#define DEF_SOIL_WET_RAW 1200    // PLACEHOLDER - calibrate from dashboard
#define DEF_LDR_DARK     800     // raw ADC below this = DARK
#define DEF_LDR_BRIGHT   2500    // raw ADC above this = BRIGHT
#define DEF_TDS_K        1.0f    // TDS calibration factor
