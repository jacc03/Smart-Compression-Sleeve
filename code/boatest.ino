/*
 * ESP32 Smart Sleeve - MANUAL BOA TEST MODE (WITH SPEED CONTROL)
 * Hardware: DRV8833 (25, 26, 27) + 4x FSR 406 (34, 35, 32, 33)
 * Controls: Send 't' (tighten), 'l' (loosen), 's' (stop) via Serial Monitor
 * Updated for ESP32 Core V3.x API
 */

// Pin Definitions
const int AIN1 = 25;
const int AIN2 = 26;
const int SLP  = 27;
const int fsrPins[] = {34, 35, 32, 33}; // 4 Signal Pins

// PWM Speed Control Settings
const int MOTOR_SPEED = 200;    // Set speed from 0 (stop) to 255 (max speed)
const int PWM_FREQ = 5000;      // 5 kHz frequency is standard for small DC motors
const int PWM_RES = 8;          // 8-bit resolution (0-255)
// Note: Channels are no longer manually assigned in ESP32 Core V3.x

// Calibration
const float OUTLIER_THRESHOLD = 7.0;    
const float SENSITIVITY_BOOST = 1.5;    

void setup() {
  Serial.begin(115200);
  
  // Set up the SLP pin normally
  pinMode(SLP, OUTPUT);
  digitalWrite(SLP, LOW); // Start with motor off/asleep

  // Set up ESP32 PWM using the new V3.x syntax
  ledcAttach(AIN1, PWM_FREQ, PWM_RES);
  ledcAttach(AIN2, PWM_FREQ, PWM_RES);

  analogReadResolution(12);
  
  // Print Instructions
  Serial.println("\n=================================");
  Serial.println(" MANUAL BOA SPOOL TEST (PWM MODE)");
  Serial.println("=================================");
  Serial.print("  Motor Speed set to: ");
  Serial.print(MOTOR_SPEED);
  Serial.println(" / 255");
  Serial.println("---------------------------------");
  Serial.println("Commands (type and press Enter):");
  Serial.println("  't' -> TIGHTEN (Clockwise)");
  Serial.println("  'l' -> LOOSEN  (Counter-Clockwise)");
  Serial.println("  's' -> STOP    (Motor Sleep)");
  Serial.println("=================================\n");
  
  delay(2000); 
}

void loop() {
  // 1. Check for manual commands from the Serial Monitor
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    
    if (cmd == 't' || cmd == 'T') {
      Serial.println("\n>>> COMMAND: TIGHTENING SPOOL <<<");
      motorMove(true);
    } 
    else if (cmd == 'l' || cmd == 'L') {
      Serial.println("\n>>> COMMAND: LOOSENING SPOOL <<<");
      motorMove(false);
    } 
    else if (cmd == 's' || cmd == 'S') {
      Serial.println("\n>>> COMMAND: SPOOL STOPPED <<<");
      motorStop();
    }
  }

  // 2. Read Sensors and Calculate Pressures
  float pressures[4];
  float totalPressure = 0;

  Serial.print("Data: ");

  for (int i = 0; i < 4; i++) {
    int raw = analogRead(fsrPins[i]);
    
    Serial.print("FSR[");
    Serial.print(i);
    Serial.print("]:");
    Serial.print(raw);
    Serial.print("  ");

    // Mapping 0-4095 to 0-50 mmHg, applying sensitivity boost
    float p = ((raw - 162) * (50.0 / (3256.0 - 162.0))) * SENSITIVITY_BOOST;
    if (p < 0) p = 0;
    
    pressures[i] = p;         
    totalPressure += p;       
  }

  // Calculate the initial average
  float avgPressure = totalPressure / 4.0;

  // 3. Outlier Detection & Rejection
  float maxDeviation = 0;
  int outlierIndex = -1;

  for (int i = 0; i < 4; i++) {
    float deviation = abs(pressures[i] - avgPressure);
    if (deviation > maxDeviation) {
      maxDeviation = deviation;
      outlierIndex = i;
    }
  }

  if (maxDeviation > OUTLIER_THRESHOLD) {
    Serial.print("| Drop FSR[");
    Serial.print(outlierIndex);
    Serial.print("] ");
    avgPressure = (totalPressure - pressures[outlierIndex]) / 3.0;
  }

  // Print final calculated average
  Serial.print("| Avg: ");
  Serial.print(avgPressure, 1);
  Serial.println(" mmHg");

  delay(250); 
}

void motorMove(bool forward) {
  digitalWrite(SLP, HIGH); // Wake driver
  if (forward) {
    // In V3.x, you write directly to the pin instead of the channel
    ledcWrite(AIN1, MOTOR_SPEED); 
    ledcWrite(AIN2, 0);           
  } else {
    ledcWrite(AIN1, 0);           
    ledcWrite(AIN2, MOTOR_SPEED); 
  }
}

void motorStop() {
  // Stop both pins
  ledcWrite(AIN1, 0);
  ledcWrite(AIN2, 0);
  digitalWrite(SLP, LOW); // Put driver to sleep to save battery
}