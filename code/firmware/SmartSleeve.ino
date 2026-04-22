/*
 * ESP32 Smart Sleeve - WebSocket Dashboard
 * Features: WiFi AP, WebSocket, PWM Speed Control, Outlier Rejection,
 *           Debounce, Discrete Pulsing, Brake Mode, Non-blocking timing
 * Hardware: DRV8833 (AIN1=25, AIN2=26, SLP=27) + 4x FSR 406 (34, 35, 32, 33)
 *
 * Library required (install via Arduino Library Manager):
 *   - WebSockets by Markus Sattler (Links2004)
 *
 * After flashing:
 *   1. Connect to WiFi network "SCS-Device" (password: scs12345)
 *   2. Open web app and connect to: 192.168.4.1
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

// ─── PWM Speed Control ───────────────────────────────────────────────────────
const int MOTOR_SPEED        = 255;
const int MOTOR_SPEED_MANUAL = 255;
const int PWM_FREQ           = 5000;
const int PWM_RES            = 8;

// ─── Timing (all non-blocking via millis) ────────────────────────────────────
const unsigned long SENSOR_INTERVAL  = 400;  // How often to read sensors & run auto-control
const unsigned long MOTOR_PULSE_MS   = 75;   // Auto-control pulse duration
const unsigned long MANUAL_PULSE_MS  = 20;   // Manual pulse duration
const unsigned long MANUAL_CMD_GAP   = 300;  // Min gap between manual commands
const unsigned long MOTOR_BRAKE_MS   = 50;   // Brake hold duration before sleep
const unsigned long MANUAL_TIMEOUT   = 60000; // Auto-resume if no command for 60s

// ─── Debounce ────────────────────────────────────────────────────────────────
int consecutiveLowReads      = 0;
const int REQUIRED_LOW_READS = 3;

// ─── State machine for non-blocking motor control ────────────────────────────
enum MotorState { MOTOR_IDLE, MOTOR_RUNNING, MOTOR_BRAKING };
MotorState motorState       = MOTOR_IDLE;
unsigned long motorStateEnd = 0;  // When to transition to next state

// ─── Timing trackers ─────────────────────────────────────────────────────────
unsigned long lastSensorRead = 0;
unsigned long lastManualCmd  = 0;
unsigned long manualTimeout  = 0;

// ─── Manual mode flag ────────────────────────────────────────────────────────
bool manualMode = false;

// ─── WebSocket ───────────────────────────────────────────────────────────────
WebSocketsServer wsServer = WebSocketsServer(81);

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

// Kick off a non-blocking motor pulse — auto-control only
void startPulse(bool forward) {
  if (forward) motorForward(); else motorReverse();
  motorState    = MOTOR_RUNNING;
  motorStateEnd = millis() + MOTOR_PULSE_MS;
}

// Called every loop() — advances motor state without blocking
void tickMotor() {
  if (motorState == MOTOR_IDLE) return;

  if (motorState == MOTOR_RUNNING && millis() >= motorStateEnd) {
    motorBrake();
    motorState    = MOTOR_BRAKING;
    motorStateEnd = millis() + MOTOR_BRAKE_MS;
  }
  else if (motorState == MOTOR_BRAKING && millis() >= motorStateEnd) {
    motorSleep();
    motorState = MOTOR_IDLE;
  }
}

// ─── WebSocket Event Handler ─────────────────────────────────────────────────
void onWsEvent(uint8_t clientId, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_CONNECTED) {
    Serial.printf("[WS] Client #%u connected\n", clientId);
  }
  else if (type == WStype_DISCONNECTED) {
    Serial.printf("[WS] Client #%u disconnected\n", clientId);
    manualMode = false;
    motorState = MOTOR_IDLE;
    motorSleep();
  }
  else if (type == WStype_TEXT) {
    String cmd = "";
    for (size_t i = 0; i < length; i++) cmd += (char)payload[i];
    cmd.trim();
    Serial.printf("[WS] Command: %s\n", cmd.c_str());

    if (cmd == "pause") {
      manualMode    = true;
      manualTimeout = millis() + MANUAL_TIMEOUT;
      motorState    = MOTOR_IDLE;
      motorBrake();
      delay(30);
      motorSleep();
    }
    else if (cmd == "contract") {
      if (!manualMode) return; // safety — ignore if not in manual mode
      manualTimeout = millis() + MANUAL_TIMEOUT;
      if (millis() - lastManualCmd >= MANUAL_CMD_GAP) {
        motorForward(MOTOR_SPEED_MANUAL);
        delay(MANUAL_PULSE_MS);
        motorBrake();
        delay(30);
        motorSleep();
        lastManualCmd = millis();
      }
    }
    else if (cmd == "retract") {
      if (!manualMode) return; // safety — ignore if not in manual mode
      manualTimeout = millis() + MANUAL_TIMEOUT;
      if (millis() - lastManualCmd >= MANUAL_CMD_GAP) {
        motorReverse(MOTOR_SPEED_MANUAL);
        delay(MANUAL_PULSE_MS);
        motorBrake();
        delay(30);
        motorSleep();
        lastManualCmd = millis();
      }
    }
    else if (cmd == "resume") {
      manualMode = false;
      motorState = MOTOR_IDLE;
      motorSleep();
      consecutiveLowReads = 0;
      Serial.println("[WS] Auto-control resumed");
    }
    // "stop" from onPointerUp is intentionally ignored —
    // manual pulses are self-terminating so no stop needed
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
  // Always service WebSocket first — no delays block this now
  wsServer.loop();

  // Advance non-blocking motor state machine
  tickMotor();

  // Manual mode timeout check
  if (manualMode && millis() > manualTimeout) {
    manualMode = false;
    motorState = MOTOR_IDLE;
    motorSleep();
    Serial.println("Manual timeout — resuming auto.");
  }

  // Only run sensor reads and auto-control on interval
  if (millis() - lastSensorRead < SENSOR_INTERVAL) return;
  lastSensorRead = millis();

  // ── Read sensors ─────────────────────────────────────────────────────────
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

  // ── Outlier rejection ────────────────────────────────────────────────────
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

  // ── Auto-control (skipped entirely in manual mode) ───────────────────────
  String statusStr;

  if (manualMode) {
    statusStr = "Manual";
    Serial.println(statusStr);
  }
  else if (avgPressure < TARGET_MIN) {
    consecutiveLowReads++;
    if (consecutiveLowReads >= REQUIRED_LOW_READS && motorState == MOTOR_IDLE) {
      statusStr = "Tightening";
      Serial.println(statusStr);
      startPulse(true);
    } else {
      statusStr = "Low (" + String(consecutiveLowReads) + "/" + String(REQUIRED_LOW_READS) + ")";
      Serial.println(statusStr);
    }
  }
  else if (avgPressure > TARGET_MAX) {
    consecutiveLowReads = 0;
    if (motorState == MOTOR_IDLE) {
      statusStr = "Releasing";
      Serial.println(statusStr);
      startPulse(false);
    } else {
      statusStr = "Releasing";
    }
  }
  else {
    consecutiveLowReads = 0;
    statusStr = "Target Met";
    Serial.println(statusStr);
    if (motorState == MOTOR_IDLE) motorSleep();
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
}
