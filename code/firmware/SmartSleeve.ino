/*
 * ESP32 Smart Sleeve - WebSocket Dashboard
 * Hardware: DRV8833 (AIN1=25, AIN2=26, SLP=27) + 4x FSR 406 (34, 35, 32, 33)
 *
 * Libraries required (install via Arduino Library Manager):
 *   - ESPAsyncWebServer  (by Me-No-Dev)
 *   - AsyncTCP           (by Me-No-Dev)
 *
 * After flashing, open Serial Monitor at 115200 to get the ESP32's IP address.
 * Enter that IP in the web app's "Connect" field (e.g. 192.168.1.42).
 */

#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// ─── WiFi Credentials ────────────────────────────────────────────────────────
const char* SSID     = "YOUR_WIFI_SSID";
const char* PASSWORD = "YOUR_WIFI_PASSWORD";

// ─── Pin Definitions ─────────────────────────────────────────────────────────
const int AIN1 = 25;
const int AIN2 = 26;
const int SLP  = 27;
const int fsrPins[] = {34, 35, 32, 33};

// ─── Pressure Target Range (mmHg) ────────────────────────────────────────────
const float TARGET_MIN = 25.0;
const float TARGET_MAX = 35.0;

// ─── Server & State ──────────────────────────────────────────────────────────
AsyncWebServer server(80);
AsyncWebSocket  ws("/ws");

bool  manualMode   = false;   // true = web app is driving the motor
unsigned long manualTimeout = 0; // auto-resume auto mode after 10s idle

// ─── Motor Helpers ───────────────────────────────────────────────────────────
void motorForward() {
  digitalWrite(SLP,  HIGH);
  digitalWrite(AIN1, HIGH);
  digitalWrite(AIN2, LOW);
}
void motorReverse() {
  digitalWrite(SLP,  HIGH);
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, HIGH);
}
void motorStop() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(SLP,  LOW);
}

// ─── Sensor Reading ──────────────────────────────────────────────────────────
float readPressure(int pin) {
  int raw = analogRead(pin);
  float p = (raw - 150) * (50.0 / (3200.0 - 150.0));
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
    // Resume auto mode when the last client leaves
    if (server->count() == 0) { manualMode = false; motorStop(); }
  }
  else if (type == WS_EVT_DATA) {
    String cmd = "";
    for (size_t i = 0; i < len; i++) cmd += (char)data[i];
    cmd.trim();
    Serial.printf("[WS] Command: %s\n", cmd.c_str());

    manualMode    = true;
    manualTimeout = millis() + 10000; // 10 s without a command → resume auto

    if      (cmd == "contract") motorForward();
    else if (cmd == "retract")  motorReverse();
    else if (cmd == "stop")     { motorStop(); manualMode = false; }
  }
}

// ─── Setup ───────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(SLP,  OUTPUT);
  motorStop();

  analogReadResolution(12);

  // Connect to WiFi
  WiFi.begin(SSID, PASSWORD);
  Serial.print("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) { delay(500); Serial.print("."); }
  Serial.printf("\nConnected! IP: %s\n", WiFi.localIP().toString().c_str());

  // WebSocket
  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  // Optional: serve a redirect at the root so you can open the IP in a browser
  server.on("/", HTTP_GET, [](AsyncWebServerRequest* req) {
    req->send(200, "text/plain",
      "Smart Sleeve WebSocket Server running.\n"
      "Open the web app and connect to: " + WiFi.localIP().toString());
  });

  server.begin();
  Serial.println("WebSocket server started on ws://" + WiFi.localIP().toString() + "/ws");
}

// ─── Loop ────────────────────────────────────────────────────────────────────
void loop() {
  ws.cleanupClients();

  // Read sensors
  float s[4], total = 0;
  for (int i = 0; i < 4; i++) {
    s[i]  = readPressure(fsrPins[i]);
    total += s[i];
  }
  float avg = total / 4.0;

  // Determine auto-control status string
  String statusStr;
  if (manualMode) {
    if (millis() > manualTimeout) { manualMode = false; motorStop(); }
    statusStr = "Manual";
  } else {
    if (avg < TARGET_MIN) {
      motorForward();
      statusStr = "Tightening";
    } else if (avg > TARGET_MAX) {
      motorReverse();
      statusStr = "Releasing";
    } else {
      motorStop();
      statusStr = "Target Met";
    }
  }

  // Build JSON and broadcast to all WebSocket clients
  // {"s":[12.3,15.4,18.2,14.1],"avg":15.0,"st":"Tightening"}
  String json = "{\"s\":[";
  for (int i = 0; i < 4; i++) {
    json += String(s[i], 1);
    if (i < 3) json += ",";
  }
  json += "],\"avg\":" + String(avg, 1);
  json += ",\"st\":\"" + statusStr + "\"}";

  if (ws.count() > 0) {
    ws.textAll(json);
  }

  // Serial debug
  Serial.printf("Avg: %.1f mmHg | %s\n", avg, statusStr.c_str());

  delay(500);
}
