/*
 * ESP32 Smart Sleeve - WebSocket Dashboard
 * Features: WiFi AP, WebSocket, PWM Speed Control, Outlier Rejection,
 *           Debounce, Discrete Pulsing, Brake Mode
 * Hardware: DRV8833 (AIN1=25, AIN2=26, SLP=27) + 4x FSR 406 (34, 35, 32, 33)
 *
 * Library required (install via Arduino Library Manager):
 *   - WebSockets by Markus Sattler (Links2004)  ← search "WebSockets"
 *
 * After flashing:
 *   1. Connect your phone to WiFi network "SCS-Device" (password: scs12345)
 *   2. Open the web app and connect to: 192.168.4.1
 */

#include <WiFi.h>
#include <WebSocketsServer.h>

// ─── Access Point Credentials ────────────────────────────────────────────────
const char* AP_SSID     = "SCS-Device";
const char* AP_PASSWORD = "scs12345";

// ─── Pin Definitions ─────────────────────────────────────────────────────────
const int AIN1 = 25;
const int AIN2 = 26;
const int SLP  = 27;
const int fsrPins[] = {34, 35, 32, 33};

// ─── Target Range (mmHg) ─────────────────────────────────────────────────────
const float TARGET_MIN = 20.0;
const float TARGET_MAX = 40.0;

// ─── Sensor Calibration ──────────────────────────────────────────────────────
const float OUTLIER_THRESHOLD = 7.0;
const float SENSITIVITY_BOOST = 2.0;

// ─── PWM Speed Control (ESP32 Core V3.x API) ─────────────────────────────────
const int MOTOR_SPEED        = 255; // Auto-control speed — full torque for pulsing
const int MOTOR_SPEED_MANUAL = 120; // Manual speed — slower for fine adjustment
const int PWM_FREQ    = 5000;
const int PWM_RES     = 8;

// ─── Pulsing & Timing ────────────────────────────────────────────────────────
const int MOTOR_PULSE_MS  = 75;
const int SETTLE_DELAY_MS = 400;

// ─── Debounce ────────────────────────────────────────────────────────────────
int consecutiveLowReads      = 0;
const int REQUIRED_LOW_READS = 3;

// ─── WebSocket Server (port 81) ──────────────────────────────────────────────
WebSocketsServer wsServer = WebSocketsServer(81);

bool manualMode             = false;
unsigned long manualTimeout = 0;

// ─── Motor Helpers ───────────────────────────────────────────────────────────
void motorForward(int speed = MOTOR_SPEED) {
  digitalWrite(SLP, HIGH);
  ledcWrite(AIN1, speed);
  ledcWrite(AIN2, 0);
}

void motorReverse(int speed = MOTOR_SPEED) {
  digitalWrite(SLP, HIGH);
  ledcWrite(AIN1, 0);
  ledcWrite(AIN2, speed);
}

void motorBrake() {
  digitalWrite(SLP, HIGH);
  ledcWrite(AIN1, 255);
  ledcWrite(AIN2, 255);
}

void motorSleep() {
  ledcWrite(AIN1, 0);
  ledcWrite(AIN2, 0);
  digitalWrite(SLP, LOW);
}

// ─── WebSocket Event Handler ─────────────────────────────────────────────────
void onWsEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.printf("[WS] Client #%u connected\n", clientId);
  }
  else if (type == WStype_DISCONNECTED) {
    Serial.printf("[WS] Client #%u disconnected\n", clientId);
    manualMode = false;
    motorSleep();
  }
  else if (type == WStype_TEXT) {
    String cmd = "";
    for (size_t i = 0; i < length; i++) cmd += (char)payload[i];
    cmd.trim();
    Serial.printf("[WS] Command: %s\n", cmd.c_str());

    manualMode    = true;
    manualTimeout = millis() + 10000;

    if (cmd == "contract") {
      motorForward(MOTOR_SPEED_MANUAL);
    }
    else if (cmd == "retract") {
      motorReverse(MOTOR_SPEED_MANUAL);
    }
    else if (cmd == "stop") {
      motorBrake();
      delay(50);
      motorSleep();
      manualMode = false;
    }
  }
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(SLP, OUTPUT);
  motorSleep();

  ledcAttach(AIN1, PWM_FREQ, PWM_RES);
  ledcAttach(AIN2, PWM_FREQ, PWM_RES);

  analogReadResolution(12);

  WiFi.softAP(AP_SSID, AP_PASSWORD);
  IPAddress ip = WiFi.softAPIP();
  Serial.printf("AP started — SSID: %s\n", AP_SSID);
  Serial.printf("WebSocket: ws://%s:81\n", ip.toString().c_str());

  wsServer.begin();
  wsServer.onEvent(onWsEvent);

  Serial.println("System ready.");
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
  wsServer.loop();

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
    pressures[outlierIndex] = avgPressure;
  }

  Serial.printf("| Final Avg: %.1f mmHg | ", avgPressure);

  // ── Manual mode timeout ──────────────────────────────────────────────────
  if (manualMode && millis() > manualTimeout) {
    manualMode = false;
    motorSleep();
    Serial.println("Manual timeout — resuming auto.");
  }

  // ── Auto-control logic ───────────────────────────────────────────────────
  String statusStr;

  if (manualMode) {
    statusStr = "Manual";
    Serial.println(statusStr);
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

  // ── Broadcast JSON ───────────────────────────────────────────────────────
  String json = "{\"s\":[";
  for (int i = 0; i < 4; i++) {
    json += String(pressures[i], 1);
    if (i < 3) json += ",";
  }
  json += "],\"avg\":" + String(avgPressure, 1);
  json += ",\"st\":\"" + statusStr + "\"}";
  wsServer.broadcastTXT(json);

  delay(SETTLE_DELAY_MS);
}
