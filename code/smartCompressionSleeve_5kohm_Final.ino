/*
 * ESP32 Smart Sleeve - Auto-Adjust Logic 
 * Features: Averaging, Outlier Rejection, Sensitivity Boost, Discrete Pulsing, & Low-Pressure Debounce
 * Hardware: DRV8833 (25, 26, 27) + 4x FSR 406 (34, 35, 32, 33)
 */

// Pin Definitions
const int AIN1 = 25;
const int AIN2 = 26;
const int SLP  = 27;
const int fsrPins[] = {34, 35, 32, 33}; // 4 Signal Pins

// Target Range & Calibration
const float TARGET_MIN = 25.0;
const float TARGET_MAX = 35.0;
const float OUTLIER_THRESHOLD = 7.0;    // If a sensor is off by > 7 mmHg from the avg, drop it
const float SENSITIVITY_BOOST = 2;      // Calibration multiplier. 1.0 = raw calculation, 1.5 = 50% boost

// Motor Control & Pulsing Settings
const int MOTOR_SPEED = 255;      // Max torque to overcome spool tension
const int PWM_FREQ = 5000;        // 5 kHz frequency
const int PWM_RES = 8;            // 8-bit resolution (0-255)

// Timing Control for Pulsing
const int MOTOR_PULSE_MS = 75;    // Duration in milliseconds the motor runs per burst (TUNE THIS)
const int SETTLE_DELAY_MS = 400;  // Time to wait after moving before reading sensors again

// Debounce Tracking
int consecutiveLowReads = 0;      // Tracks how many times pressure was too low
const int REQUIRED_LOW_READS = 3; // How many times it must be low before tightening

void setup() {
  Serial.begin(115200);
  
  pinMode(SLP, OUTPUT);
  digitalWrite(SLP, LOW); // Start in Sleep

  // Set up ESP32 PWM using the new V3.x syntax for speed control
  ledcAttach(AIN1, PWM_FREQ, PWM_RES);
  ledcAttach(AIN2, PWM_FREQ, PWM_RES);

  analogReadResolution(12);
  Serial.println("System Initialized. Outlier Rejection, Pulsing Control, & Debounce Active.");
}

void loop() {
  float pressures[4];
  float totalPressure = 0;

  Serial.print("Raw ADCs -> ");

  // 1. Read all 4 Sensors and Store Pressures
  for (int i = 0; i < 4; i++) {
    int raw = analogRead(fsrPins[i]);
    
    Serial.print("FSR[");
    Serial.print(i);
    Serial.print("]: ");
    Serial.print(raw);
    Serial.print("  ");

    // Mapping 0-4095 to 0-50 mmHg (adjusted for 5.1k resistor), then applying sensitivity boost
    float p = ((raw - 162) * (50.0 / (3256.0 - 162.0))) * SENSITIVITY_BOOST;
    if (p < 0) p = 0;
    
    pressures[i] = p;         // Store individual pressure
    totalPressure += p;       // Add to total
  }

  // Calculate the initial average
  float avgPressure = totalPressure / 4.0;

  // 2. Outlier Detection
  float maxDeviation = 0;
  int outlierIndex = -1;

  for (int i = 0; i < 4; i++) {
    float deviation = abs(pressures[i] - avgPressure);
    if (deviation > maxDeviation) {
      maxDeviation = deviation;
      outlierIndex = i;
    }
  }

  // 3. Outlier Rejection
  if (maxDeviation > OUTLIER_THRESHOLD) {
    Serial.print("| Dropping FSR[");
    Serial.print(outlierIndex);
    Serial.print("] (Dev: ");
    Serial.print(maxDeviation, 1);
    Serial.print(") ");

    avgPressure = (totalPressure - pressures[outlierIndex]) / 3.0;
  }

  // Print final calculated average
  Serial.print("| Final Avg: ");
  Serial.print(avgPressure, 1);
  Serial.print(" mmHg | Status: ");

  // 4. Motor Control Logic with Debounce and Pulsing
  if (avgPressure < TARGET_MIN) {
    consecutiveLowReads++; // Increment the counter
    
    if (consecutiveLowReads >= REQUIRED_LOW_READS) {
      Serial.println("Tightening...");
      motorMove(true);       // Turn on clockwise
      delay(MOTOR_PULSE_MS); // Run for burst duration
      motorStop();           // Immediately stop
    } else {
      Serial.print("Low Pressure (Wait ");
      Serial.print(consecutiveLowReads);
      Serial.print("/");
      Serial.print(REQUIRED_LOW_READS);
      Serial.println(")...");
      motorStop(); 
    }
  } 
  else if (avgPressure > TARGET_MAX) {
    consecutiveLowReads = 0; // Reset counter because pressure is high
    Serial.println("Releasing...");
    motorMove(false);      // Turn on counter-clockwise
    delay(MOTOR_PULSE_MS); // Run for burst duration
    motorStop();           // Immediately stop
  } 
  else {
    consecutiveLowReads = 0; // Reset counter because target is met
    Serial.println("Target Met - Sleeping.");
    motorStop();
  }

  // Allow pressure to settle before the next reading
  delay(SETTLE_DELAY_MS); 
}

void motorMove(bool forward) {
  digitalWrite(SLP, HIGH); // Wake driver
  if (forward) {
    ledcWrite(AIN1, MOTOR_SPEED);
    ledcWrite(AIN2, 0);
  } else {
    ledcWrite(AIN1, 0);
    ledcWrite(AIN2, MOTOR_SPEED);
  }
}

void motorStop() {
digitalWrite(SLP, HIGH); // MUST stay awake to engage the brakes
  
  // Driving both inputs to maximum (255) puts the DRV8833 into Brake Mode
  ledcWrite(AIN1, 255); 
  ledcWrite(AIN2, 255); 
}