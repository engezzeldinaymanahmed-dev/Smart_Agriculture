// =====================================================
//  SMART AGRICULTURE SYSTEM  -  ESP32 (WROOM-32 DevKit)
//  SoftAP + local web dashboard + automatic/manual pump
//
//  Libraries (install from Arduino Library Manager):
//    - "DHT sensor library" by Adafruit
//    - "Adafruit Unified Sensor" by Adafruit
//  Built-in with the ESP32 board package: WiFi, WebServer, Preferences
//
//  STATUS: written but NOT compiled or tested on hardware yet.
// =====================================================
#include <WiFi.h>
#include <WebServer.h>
#include <Preferences.h>
#include <DHT.h>
#include "config.h"
#include "webpage.h"

DHT dht(PIN_DHT, DHT11);
WebServer server(80);
Preferences prefs;

// ---------------- Settings (saved in flash) ----------------
struct Settings {
  int lowPct, highPct, maxRunSec, cooldownSec;
  int soilDryRaw, soilWetRaw;
  int ldrDark, ldrBright;
  float tdsK;
} S;

void loadSettings() {
  prefs.begin("agri", false);
  S.lowPct      = prefs.getInt("low", DEF_LOW_PCT);
  S.highPct     = prefs.getInt("high", DEF_HIGH_PCT);
  S.maxRunSec   = prefs.getInt("maxRun", DEF_MAX_RUN_SEC);
  S.cooldownSec = prefs.getInt("cool", DEF_COOLDOWN_SEC);
  S.soilDryRaw  = prefs.getInt("dry", DEF_SOIL_DRY_RAW);
  S.soilWetRaw  = prefs.getInt("wet", DEF_SOIL_WET_RAW);
  S.ldrDark     = prefs.getInt("ldrD", DEF_LDR_DARK);
  S.ldrBright   = prefs.getInt("ldrB", DEF_LDR_BRIGHT);
  S.tdsK        = prefs.getFloat("tdsK", DEF_TDS_K);
}

void saveSettings() {
  prefs.putInt("low", S.lowPct);
  prefs.putInt("high", S.highPct);
  prefs.putInt("maxRun", S.maxRunSec);
  prefs.putInt("cool", S.cooldownSec);
  prefs.putInt("dry", S.soilDryRaw);
  prefs.putInt("wet", S.soilWetRaw);
  prefs.putInt("ldrD", S.ldrDark);
  prefs.putInt("ldrB", S.ldrBright);
  prefs.putFloat("tdsK", S.tdsK);
}

// ---------------- Live state ----------------
float temp = NAN, hum = NAN;
bool dhtOk = false;
int dhtFails = 0;

int soilRaw = 0, soilPct = 0;
bool soilOk = false;

float tdsMv = 0;
int tdsPpm = 0;
bool tdsOk = false;

int ldrRaw = 0;
bool ldrOk = false;

bool autoMode = false;   // boots in MANUAL on purpose (safe until calibrated)
bool pumpOn = false;
bool estop = false;
bool fault = false;
String faultMsg = "";
String lastReason = "BOOT";
unsigned long pumpStartMs = 0, lastStopMs = 0, lastSensorMs = 0;
bool hasStopped = false;
int startPct = 0;
int failedCycles = 0;

// ---------------- Relay ----------------
void relayWrite(bool on) {
  bool level = RELAY_ACTIVE_LOW ? !on : on;
  digitalWrite(PIN_RELAY, level ? HIGH : LOW);
}

void pumpStop(const char* why) {
  if (pumpOn) {
    pumpOn = false;
    hasStopped = true;
    lastStopMs = millis();
  }
  relayWrite(false);
  lastReason = why;
}

void pumpStart() {
  pumpOn = true;
  pumpStartMs = millis();
  startPct = soilPct;
  relayWrite(true);
  lastReason = "RUNNING";
}

// ---------------- Sensor helpers ----------------
int readRawAvg(int pin) {
  long sum = 0;
  for (int i = 0; i < 16; i++) {
    sum += analogRead(pin);
    delay(2);
  }
  return (int)(sum / 16);
}

float readMvMedian(int pin) {
  int buf[15];
  for (int i = 0; i < 15; i++) {
    buf[i] = analogReadMilliVolts(pin);
    delay(10);
  }
  for (int i = 1; i < 15; i++) {          // insertion sort
    int key = buf[i];
    int j = i - 1;
    while (j >= 0 && buf[j] > key) {
      buf[j + 1] = buf[j];
      j--;
    }
    buf[j + 1] = key;
  }
  return (float)buf[7];
}

void updateSensors() {
  // ---- DHT11 ----
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t) || isnan(h)) {
    dhtFails++;
    if (dhtFails >= 3) {
      dhtOk = false;
      temp = NAN;
      hum = NAN;
    }
  } else {
    dhtFails = 0;
    dhtOk = true;
    temp = t;
    hum = h;
  }

  // ---- Soil moisture ----
  if (PIN_SOIL_POWER >= 0) {
    digitalWrite(PIN_SOIL_POWER, HIGH);
    delay(50);
  }
  soilRaw = readRawAvg(PIN_SOIL_ADC);
  if (PIN_SOIL_POWER >= 0) digitalWrite(PIN_SOIL_POWER, LOW);
  soilOk = (soilRaw > 50 && soilRaw < 4090);
  int span = S.soilDryRaw - S.soilWetRaw;
  if (span == 0) span = 1;
  soilPct = constrain((int)(100L * (S.soilDryRaw - soilRaw) / span), 0, 100);

  // ---- TDS (temperature compensated) ----
  tdsMv = readMvMedian(PIN_TDS_ADC);
  float v = tdsMv / 1000.0f;
  float tc = dhtOk ? temp : 25.0f;           // air temp used as water temp proxy
  float comp = 1.0f + 0.02f * (tc - 25.0f);
  float vc = v / comp;
  float ppm = (133.42f * vc * vc * vc - 255.86f * vc * vc + 857.39f * vc) * 0.5f * S.tdsK;
  if (ppm < 0) ppm = 0;
  tdsPpm = (int)ppm;
  tdsOk = (tdsMv < 3200.0f);

  // ---- LDR ----
  ldrRaw = readRawAvg(PIN_LDR_ADC);
  ldrOk = (ldrRaw > 5 && ldrRaw < 4090);
}

// ---------------- Pump control ----------------
void controlPump() {
  unsigned long now = millis();

  if (estop) {                         // emergency stop beats everything
    pumpStop("E-STOP");
    return;
  }

  // hard run-time limit, valid in BOTH modes
  if (pumpOn && (now - pumpStartMs) >= (unsigned long)S.maxRunSec * 1000UL) {
    pumpStop("MAX RUN TIME");
    if (autoMode) {
      if (soilPct < startPct + 3) failedCycles++; else failedCycles = 0;
      if (failedCycles >= 3) {
        fault = true;
        faultMsg = "Soil moisture did not rise after 3 runs. Check water tank, tubing, pump and soil sensor.";
      }
    }
    return;
  }

  if (!autoMode) return;               // manual mode: only dashboard buttons act

  if (fault) {
    if (pumpOn) pumpStop("FAULT");
    return;
  }
  if (!soilOk) {
    if (pumpOn) pumpStop("SOIL SENSOR ERROR");
    return;
  }

  if (pumpOn) {
    if (soilPct >= S.highPct) {
      pumpStop("TARGET REACHED");
      failedCycles = 0;
    }
  } else {
    bool cooled = !hasStopped || (now - lastStopMs) >= (unsigned long)S.cooldownSec * 1000UL;
    if (soilPct < S.lowPct && cooled) pumpStart();
  }
}

// ---------------- JSON helpers ----------------
String fnum(float v) {
  if (isnan(v)) return String("null");
  return String(v, 1);
}

String soilState() {
  if (!soilOk) return "ERROR";
  if (soilPct < 30) return "DRY";
  if (soilPct > 70) return "WET";
  return "NORMAL";
}

String ldrState() {
  if (!ldrOk) return "ERROR";
  if (ldrRaw < S.ldrDark) return "DARK";
  if (ldrRaw > S.ldrBright) return "BRIGHT";
  return "NORMAL";
}

void sendJson(int code, const String& body) {
  server.sendHeader("Cache-Control", "no-store");
  server.send(code, "application/json", body);
}

void sendMsg(int code, bool ok, const String& msg) {
  sendJson(code, String("{\"ok\":") + (ok ? "true" : "false") + ",\"msg\":\"" + msg + "\"}");
}

// ---------------- HTTP handlers ----------------
void handleData() {
  String alerts = "";
  auto addAlert = [&](const String& a) {
    if (alerts.length()) alerts += ",";
    alerts += "\"" + a + "\"";
  };
  if (estop) addAlert("EMERGENCY STOP is active - pump locked off");
  if (fault) addAlert("FAULT: " + faultMsg);
  if (!dhtOk) addAlert("DHT11 not responding - check wiring");
  if (!soilOk) addAlert("Soil sensor reading out of range - check wiring");
  if (!tdsOk) addAlert("TDS reading out of range - check wiring");
  if (!ldrOk) addAlert("LDR reading out of range - check wiring");
  if (!estop && !pumpOn && lastReason == "MAX RUN TIME") addAlert("Last pump run stopped by max run time");

  long runLeft = 0;
  if (pumpOn) {
    long used = (long)((millis() - pumpStartMs) / 1000UL);
    runLeft = S.maxRunSec - used;
    if (runLeft < 0) runLeft = 0;
  }

  String j = "{";
  j += "\"temp\":" + fnum(temp) + ",";
  j += "\"hum\":" + fnum(hum) + ",";
  j += "\"dhtOk\":" + String(dhtOk ? "true" : "false") + ",";
  j += "\"soilRaw\":" + String(soilRaw) + ",";
  j += "\"soilPct\":" + String(soilPct) + ",";
  j += "\"soilState\":\"" + soilState() + "\",";
  j += "\"soilOk\":" + String(soilOk ? "true" : "false") + ",";
  j += "\"tds\":" + String(tdsPpm) + ",";
  j += "\"tdsMv\":" + String((int)tdsMv) + ",";
  j += "\"tdsOk\":" + String(tdsOk ? "true" : "false") + ",";
  j += "\"ldrRaw\":" + String(ldrRaw) + ",";
  j += "\"ldrState\":\"" + ldrState() + "\",";
  j += "\"ldrOk\":" + String(ldrOk ? "true" : "false") + ",";
  j += "\"pump\":" + String(pumpOn ? "true" : "false") + ",";
  j += "\"mode\":\"" + String(autoMode ? "AUTO" : "MANUAL") + "\",";
  j += "\"estop\":" + String(estop ? "true" : "false") + ",";
  j += "\"fault\":" + String(fault ? "true" : "false") + ",";
  j += "\"runLeft\":" + String(runLeft) + ",";
  j += "\"reason\":\"" + lastReason + "\",";
  j += "\"uptime\":" + String(millis() / 1000UL) + ",";
  j += "\"alerts\":[" + alerts + "]}";
  sendJson(200, j);
}

void handleConfigGet() {
  String j = "{";
  j += "\"low\":" + String(S.lowPct) + ",";
  j += "\"high\":" + String(S.highPct) + ",";
  j += "\"maxRun\":" + String(S.maxRunSec) + ",";
  j += "\"cooldown\":" + String(S.cooldownSec) + ",";
  j += "\"tdsK\":" + String(S.tdsK, 3) + ",";
  j += "\"ldrDark\":" + String(S.ldrDark) + ",";
  j += "\"ldrBright\":" + String(S.ldrBright) + ",";
  j += "\"dry\":" + String(S.soilDryRaw) + ",";
  j += "\"wet\":" + String(S.soilWetRaw) + "}";
  sendJson(200, j);
}

void handleConfigPost() {
  Settings c = S;
  if (server.hasArg("low"))       c.lowPct      = server.arg("low").toInt();
  if (server.hasArg("high"))      c.highPct     = server.arg("high").toInt();
  if (server.hasArg("maxRun"))    c.maxRunSec   = server.arg("maxRun").toInt();
  if (server.hasArg("cooldown"))  c.cooldownSec = server.arg("cooldown").toInt();
  if (server.hasArg("ldrDark"))   c.ldrDark     = server.arg("ldrDark").toInt();
  if (server.hasArg("ldrBright")) c.ldrBright   = server.arg("ldrBright").toInt();
  if (server.hasArg("tdsK"))      c.tdsK        = server.arg("tdsK").toFloat();

  if (c.lowPct < 0 || c.highPct > 100 || c.lowPct >= c.highPct) {
    sendMsg(400, false, "Need 0 <= low < high <= 100");
    return;
  }
  if (c.maxRunSec < 1 || c.maxRunSec > 300) {
    sendMsg(400, false, "Max run must be 1-300 seconds");
    return;
  }
  if (c.cooldownSec < 0 || c.cooldownSec > 3600) {
    sendMsg(400, false, "Cooldown must be 0-3600 seconds");
    return;
  }
  if (c.ldrDark >= c.ldrBright) {
    sendMsg(400, false, "LDR dark threshold must be below bright threshold");
    return;
  }
  if (c.tdsK < 0.1f || c.tdsK > 10.0f) {
    sendMsg(400, false, "TDS factor must be 0.1-10");
    return;
  }
  S = c;
  saveSettings();
  sendMsg(200, true, "Settings saved");
}

void handleCalib() {
  String w = server.arg("w");
  int raw = readRawAvg(PIN_SOIL_ADC);
  if (w == "dry") {
    S.soilDryRaw = raw;
  } else if (w == "wet") {
    S.soilWetRaw = raw;
  } else {
    sendMsg(400, false, "w must be dry or wet");
    return;
  }
  saveSettings();
  sendMsg(200, true, String(w) + " point saved, raw=" + String(raw));
}

void handleMode() {
  String m = server.arg("m");
  if (m == "auto") autoMode = true;
  else if (m == "manual") autoMode = false;
  else {
    sendMsg(400, false, "m must be auto or manual");
    return;
  }
  pumpStop("MODE CHANGE");
  sendMsg(200, true, "Mode set");
}

void handlePump() {
  if (estop) {
    sendMsg(409, false, "Emergency stop is active. Press RESET first.");
    return;
  }
  if (autoMode) {
    sendMsg(409, false, "Switch to MANUAL mode to control the pump by hand.");
    return;
  }
  String s = server.arg("s");
  if (s == "1") {
    if (!pumpOn) pumpStart();
    sendMsg(200, true, "Pump ON");
  } else if (s == "0") {
    pumpStop("MANUAL OFF");
    sendMsg(200, true, "Pump OFF");
  } else {
    sendMsg(400, false, "s must be 1 or 0");
  }
}

void handleStop() {
  estop = true;
  pumpStop("E-STOP");
  sendMsg(200, true, "EMERGENCY STOP");
}

void handleReset() {
  estop = false;
  fault = false;
  faultMsg = "";
  failedCycles = 0;
  lastReason = "RESET";
  sendMsg(200, true, "Reset done");
}

// ---------------- setup / loop ----------------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_RELAY, OUTPUT);
  relayWrite(false);                   // pump OFF before anything else

  if (PIN_SOIL_POWER >= 0) {
    pinMode(PIN_SOIL_POWER, OUTPUT);
    digitalWrite(PIN_SOIL_POWER, LOW);
  }

  analogReadResolution(12);
  analogSetPinAttenuation(PIN_SOIL_ADC, ADC_11db);
  analogSetPinAttenuation(PIN_TDS_ADC, ADC_11db);
  analogSetPinAttenuation(PIN_LDR_ADC, ADC_11db);

  dht.begin();
  loadSettings();

  WiFi.mode(WIFI_AP);
  bool ok = WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println();
  Serial.println("=== SMART AGRICULTURE SYSTEM ===");
  Serial.print("SoftAP start: ");
  Serial.println(ok ? "OK" : "FAILED");
  Serial.print("SSID: ");
  Serial.println(AP_SSID);
  Serial.print("Password: ");
  Serial.println(AP_PASS);
  Serial.print("Open in browser: http://");
  Serial.println(WiFi.softAPIP());

  server.on("/", HTTP_GET, []() { server.send_P(200, "text/html", INDEX_HTML); });
  server.on("/api/data", HTTP_GET, handleData);
  server.on("/api/config", HTTP_GET, handleConfigGet);
  server.on("/api/config", HTTP_POST, handleConfigPost);
  server.on("/api/calib", HTTP_POST, handleCalib);
  server.on("/api/mode", HTTP_POST, handleMode);
  server.on("/api/pump", HTTP_POST, handlePump);
  server.on("/api/stop", HTTP_POST, handleStop);
  server.on("/api/reset", HTTP_POST, handleReset);
  server.onNotFound([]() { sendMsg(404, false, "Not found"); });
  server.begin();

  updateSensors();
}

void loop() {
  server.handleClient();

  unsigned long now = millis();
  if (now - lastSensorMs >= SENSOR_PERIOD_MS) {
    lastSensorMs = now;
    updateSensors();
    Serial.printf("T=%.1f H=%.1f soilRaw=%d soil=%d%% tds=%dppm(%dmV) ldr=%d pump=%d mode=%s reason=%s\n",
                  temp, hum, soilRaw, soilPct, tdsPpm, (int)tdsMv, ldrRaw,
                  pumpOn ? 1 : 0, autoMode ? "AUTO" : "MANUAL", lastReason.c_str());
  }

  controlPump();
}
