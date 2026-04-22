/*
 * ESP32 Smart Sleeve - WebSocket Dashboard
 * Features: WiFi AP, WebSocket, PWM Speed Control, Outlier Rejection,
 *           Debounce, Discrete Pulsing, Brake Mode
 * Hardware: DRV8833 (AIN1=25, AIN2=26, SLP=27) + 4x FSR 406 (34, 35, 32, 33)
 *
 * Libraries required (install via Arduino Library Manager):
 *   - ESPAsyncWebServer  (by lacamera / Me-No-Dev fork)
 *   - AsyncTCP           (by dvarrel / Me-No-Dev fork)
 *
 * After flashing:
 *   1. Connect your phone to WiFi network "SCS-Device" (password: scs12345)
 *   2. Open the web app and connect to: 192.168.4.1
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// ─── Access Point Credentials ────────────────────────────────────────────────
const char* AP_SSID     = "SCS-Device";
const char* AP_PASSWORD = "scs12345";   // min 8 chars, "" for open network

// ─── Pin Definitions ─────────────────────────────────────────────────────────
const int AIN1 = 25;
const int AIN2 = 26;
const int SLP  = 27;
const int fsrPins[] = {34, 35, 32, 33};

// ─── Target Range (mmHg) ─────────────────────────────────────────────────────
const float TARGET_MIN = 20.0;
const float TARGET_MAX = 40.0;

// ─── Sensor Calibration ──────────────────────────────────────────────────────
const float OUTLIER_THRESHOLD = 7.0;  // Drop sensor if deviation > 7 mmHg from avg
const float SENSITIVITY_BOOST = 2.0;  // Calibration multiplier (1.0 = raw)

// ─── PWM Speed Control (ESP32 Core V3.x API) ─────────────────────────────────
const int MOTOR_SPEED = 255;  // 0–255, max torque to overcome spool tension
const int PWM_FREQ    = 5000; // 5 kHz, standard for small DC motors
const int PWM_RES     = 8;    // 8-bit resolution

// ─── Pulsing & Timing ────────────────────────────────────────────────────────
const int MOTOR_PULSE_MS  = 75;  // Motor run duration per burst (ms) — tune this
const int SETTLE_DELAY_MS = 400; // Wait after moving before next sensor read

// ─── Debounce ────────────────────────────────────────────────────────────────
int consecutiveLowReads       = 0;
const int REQUIRED_LOW_READS  = 3; // Must read low this many times before tightening

// ─── Server & Manual Mode State ──────────────────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket  ws("/ws");

bool manualMode          = false;
unsigned long manualTimeout = 0; // Auto-resume after 10s of no manual command

// ─── Motor Helpers ───────────────────────────────────────────────────────────
void motorForward() {
  digitalWrite(SLP, HIGH);
  ledcWrite(AIN1, MOTOR_SPEED);
  ledcWrite(AIN2, 0);
}

void motorReverse() {
  digitalWrite(SLP, HIGH);
  ledcWrite(AIN1, 0);
  ledcWrite(AIN2, MOTOR_SPEED);
}

void motorBrake() {
  // Driving both pins HIGH puts DRV8833 into active brake mode
  // SLP must stay HIGH to keep the driver awake for braking to work
  digitalWrite(SLP, HIGH);
  ledcWrite(AIN1, 255);
  ledcWrite(AIN2, 255);
}

void motorSleep() {
  ledcWrite(AIN1, 0);
  ledcWrite(AIN2, 0);
  digitalWrite(SLP, LOW); // Full sleep — saves battery when idle
}

// ─── Sensor Reading ──────────────────────────────────────────────────────────
// Returns calibrated pressure in mmHg for a given ADC pin
float readPressure(int pin) {
  int raw = analogRead(pin);
  float p = ((raw - 162) * (50.0 / (3256.0 - 162.0))) * SENSITIVITY_BOOST;
  return max(p, 0.0f);
}

// ─── WebSocket Event Handler ──────────────────────────────────────────────────
void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
               AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.printf("[WS] Client #%u connected\n", client->id());
  }
  else if (type == WS_EVT_DISCONNECT) {
    Serial.printf("[WS] Client #%u disconnected\n", client->id());
    if (server->count() == 0) {
      manualMode = false;
      motorSleep();
    }
  }
  else if (type == WS_EVT_DATA) {
    String cmd = "";
    for (size_t i = 0; i < len; i++) cmd += (char)data[i];
    cmd.trim();
    Serial.printf("[WS] Command: %s\n", cmd.c_str());

    manualMode    = true;
    manualTimeout = millis() + 10000;

    if (cmd == "contract") {
      motorForward();
    }
    else if (cmd == "retract") {
      motorReverse();
    }
    else if (cmd == "stop") {
      motorBrake();
      delay(50);      // Brief brake hold
      motorSleep();
      manualMode = false;
    }
  }
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  // Motor pins
  pinMode(SLP, OUTPUT);
  motorSleep();

  // PWM setup (ESP32 Core V3.x — attach directly to pin, no channel needed)
  ledcAttach(AIN1, PWM_FREQ, PWM_RES);
  ledcAttach(AIN2, PWM_FREQ, PWM_RES);

  analogReadResolution(12);

  // Start Access Point
  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("AP started — SSID: %s\n", AP_SSID);
  Serial.printf("Connect phone to that network, then open web app at: http://%s\n", ip.toString().c_str());
  Serial.printf("WebSocket: ws://%s/ws\n", ip.toString().c_str());

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain",
      "SCS WebSocket server running.\n"
      "Connect phone to SCS-Device WiFi, then open web app at 192.168.4.1");
  });

  server.begin();
  Serial.println("System ready. Outlier rejection, pulsing, debounce, and brake mode active.");
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
  ws.cleanupClients();

  // ── Read all 4 sensors ───────────────────────────────────────────────────
  float pressures[4];
  float totalPressure = 0;

  Serial.print("Raw ADCs -> ");
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(fsrPins[i]);
    Serial.printf("FSR[%d]: %d  ", i, raw);

    float p = ((raw - 162) * (50.0 / (3256.0 - 162.0))) * SENSITIVITY_BOOST;
    pressures[i] = max(p, 0.0f);
    totalPressure += pressures[i];
  }

  float avgPressure = totalPressure / 4.0;

  // ── Outlier detection & rejection ───────────────────────────────────────
  float maxDeviation = 0;
  int   outlierIndex = -1;

  for (int i = 0; i < 4; i++) {
    float dev = abs(pressures[i] - avgPressure);
    if (dev > maxDeviation) {
      maxDeviation = dev;
      outlierIndex = i;
    }
  }

  if (maxDeviation > OUTLIER_THRESHOLD) {
    Serial.printf("| Drop FSR[%d] (dev: %.1f) ", outlierIndex, maxDeviation);
    avgPressure = (totalPressure - pressures[outlierIndex]) / 3.0;
    pressures[outlierIndex] = avgPressure; // substitute avg so JSON stays clean
  }

  Serial.printf("| Final Avg: %.1f mmHg | ", avgPressure);

  // ── Manual mode timeout ──────────────────────────────────────────────────
  if (manualMode && millis() > manualTimeout) {
    manualMode = false;
    motorSleep();
    Serial.println("Manual timeout — resuming auto.");
  }

  // ── Auto-control logic (skipped during manual override) ─────────────────
  String statusStr;

  if (manualMode) {
    statusStr = "Manual";
  }
  else if (avgPressure < TARGET_MIN) {
    consecutiveLowReads++;

    if (consecutiveLowReads >= REQUIRED_LOW_READS) {
      statusStr = "Tightening";
      Serial.println(statusStr);
      motorForward();
      delay(MOTOR_PULSE_MS);
      motorBrake();
      delay(50);
      motorSleep();
    } else {
      statusStr = "Low (" + String(consecutiveLowReads) + "/" + String(REQUIRED_LOW_READS) + ")";
      Serial.println(statusStr);
      motorSleep();
    }
  }
  else if (avgPressure > TARGET_MAX) {
    consecutiveLowReads = 0;
    statusStr = "Releasing";
    Serial.println(statusStr);
    motorReverse();
    delay(MOTOR_PULSE_MS);
    motorBrake();
    delay(50);
    motorSleep();
  }
  else {
    consecutiveLowReads = 0;
    statusStr = "Target Met";
    Serial.println(statusStr);
    motorSleep();
  }

  // ── Broadcast JSON to all WebSocket clients ──────────────────────────────
  // {"s":[12.3,15.1,14.8,13.9],"avg":14.0,"st":"Tightening"}
  if (ws.count() > 0) {
    String json = "{\"s\":[";
    for (int i = 0; i < 4; i++) {
      json += String(pressures[i], 1);
      if (i < 3) json += ",";
    }
    json += "],\"avg\":" + String(avgPressure, 1);
    json += ",\"st\":\"" + statusStr + "\"}";
    ws.textAll(json);
  }

  delay(SETTLE_DELAY_MS);
}
