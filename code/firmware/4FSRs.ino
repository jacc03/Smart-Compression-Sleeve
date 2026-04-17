/*
 * ESP32 Smart Sleeve - 4-Channel Averaging Logic
 * Hardware: DRV8833 (25, 26, 27) + 4x FSR 406 (34, 35, 32, 33)
 */

// Pin Definitions
const int AIN1 = 25;
const int AIN2 = 26;
const int SLP  = 27;
const int fsrPins[] = {34, 35, 32, 33}; // 4 Signal Pins

// Target Range (mmHg)
const float TARGET_MIN = 25.0;
const float TARGET_MAX = 35.0;

void setup() {
  Serial.begin(115200);
  pinMode(AIN1, OUTPUT);
  pinMode(AIN2, OUTPUT);
  pinMode(SLP, OUTPUT);

  digitalWrite(SLP, LOW); // Start in Sleep
  analogReadResolution(12);
  Serial.println("4-Channel System Initialized.");
}

void loop() {
  float totalPressure = 0;

  // 1. Read and Sum all 4 Sensors
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(fsrPins[i]);
    // Mapping 0-4095 to 0-50 mmHg (based on 4.7k resistor)
    float p = (raw - 150) * (50.0 / (3200.0 - 150.0));
    if (p < 0) p = 0;
    totalPressure += p;
  }

  // 2. Calculate Average
  float avgPressure = totalPressure / 4.0;

  Serial.print("Average Pressure: ");
  Serial.print(avgPressure, 1);
  Serial.print(" mmHg | Status: ");

  // 3. Motor Control Logic
  if (avgPressure < TARGET_MIN) {
    Serial.println("Tightening...");
    motorMove(true); // Clockwise
  } 
  else if (avgPressure > TARGET_MAX) {
    Serial.println("Releasing...");
    motorMove(false); // Counter-Clockwise
  } 
  else {
    Serial.println("Target Met - Sleeping.");
    motorStop();
  }

  delay(500); // Check twice per second
}

void motorMove(bool forward) {
  digitalWrite(SLP, HIGH); // Wake driver
  if (forward) {
    digitalWrite(AIN1, HIGH);
    digitalWrite(AIN2, LOW);
  } else {
    digitalWrite(AIN1, LOW);
    digitalWrite(AIN2, HIGH);
  }
}

void motorStop() {
  digitalWrite(AIN1, LOW);
  digitalWrite(AIN2, LOW);
  digitalWrite(SLP, LOW); // Save Battery
}
