#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pin Definitions
#define PIN_TEMP A0
#define PIN_CURR A1
#define PIN_VIB  A2

#define PIN_BUZZER 8
#define PIN_LED_R  9
#define PIN_LED_G  10
#define PIN_LED_B  6   // Changed from 11 to 6 to avoid PWM conflict with tone()
#define PIN_BTN    2

// Thresholds
const float TEMP_WARN = 45.0;  // °C
const float TEMP_CRIT = 65.0;  // °C
const int VIB_WARN = 400;      // Raw analog units
const int VIB_CRIT = 750;

// LCD Setup (I2C Address 0x3F or 0x27)
LiquidCrystal_I2C lcd(0x20, 16, 2);

// Asset State Variables
float tempC = 0.0;
int vibration = 0;
float currentA = 0.0;
int healthIndex = 100;
long operatingSeconds = 0;
int estimatedRULHours = 500; // Baseline RUL

void setup() {
  Serial.begin(9600);
  
  pinMode(PIN_BUZZER, OUTPUT);
  pinMode(PIN_LED_R, OUTPUT);
  pinMode(PIN_LED_G, OUTPUT);
  pinMode(PIN_LED_B, OUTPUT);
  pinMode(PIN_BTN, INPUT_PULLUP);

  // Initialize I2C Display
  lcd.init();
  lcd.backlight();
  lcd.clear();
  
  lcd.setCursor(0, 0);
  lcd.print("ASSET SENTINEL");
  lcd.setCursor(0, 1);
  lcd.print("Initializing...");
  delay(1500);
  lcd.clear();
}

void loop() {
  readSensors();
  calculateHealth();
  updateOutputs();
  sendDigitalTwinTelemetry();
  
  delay(1000); // 1-second telemetry interval
}

void readSensors() {
  // 1. Read Temperature (TMP36)
  int rawTemp = analogRead(PIN_TEMP);
  float voltageTemp = rawTemp * (5.0 / 1023.0);
  tempC = (voltageTemp - 0.5) * 100.0;

  // 2. Read Vibration
  vibration = analogRead(PIN_VIB);

  // 3. Read Current (ACS712 - 5A model)
  int rawCurr = analogRead(PIN_CURR);
  float voltageCurr = rawCurr * (5.0 / 1023.0);
  currentA = abs((voltageCurr - 2.5) / 0.185);

  operatingSeconds++;
}

void calculateHealth() {
  int penalty = 0;

  // Thermal Penalty with constrain limits
  if (tempC > TEMP_WARN) {
    int pTemp = map((int)tempC, TEMP_WARN, TEMP_CRIT, 10, 40);
    penalty += constrain(pTemp, 10, 50);
  }
  
  // Vibration Penalty with constrain limits
  if (vibration > VIB_WARN) {
    int pVib = map(vibration, VIB_WARN, VIB_CRIT, 10, 40);
    penalty += constrain(pVib, 10, 50);
  }

  healthIndex = 100 - penalty;
  if (healthIndex < 0) healthIndex = 0;

  // Dynamic RUL Decay Acceleration based on strain
  if (healthIndex < 50) {
    estimatedRULHours -= 5; // Accelerated wear
  } else if (healthIndex < 80) {
    estimatedRULHours -= 2;
  }
  if (estimatedRULHours < 0) estimatedRULHours = 0;
}

void updateOutputs() {
  // Line 1 Update
  lcd.setCursor(0, 0);
  lcd.print("T:");
  lcd.print((int)tempC);
  lcd.print("C V:");
  lcd.print(vibration);
  lcd.print("    "); // Clear residual numbers

  // Line 2 Update
  lcd.setCursor(0, 1);
  lcd.print("HI:");
  lcd.print(healthIndex);
  lcd.print("% RUL:");
  lcd.print(estimatedRULHours);
  lcd.print("h   "); // Clear residual numbers

  // Status Indication
  if (healthIndex > 80) {
    // Normal / Healthy
    setRGB(0, 255, 0);
    noTone(PIN_BUZZER);
  } else if (healthIndex > 50) {
    // Warning State
    setRGB(255, 150, 0);
    tone(PIN_BUZZER, 1000, 100); // Intermittent beep
  } else {
    // Critical Fault State
    setRGB(255, 0, 0);
    tone(PIN_BUZZER, 2000); // Continuous alarm
  }
}

void setRGB(int r, int g, int b) {
  analogWrite(PIN_LED_R, r);
  analogWrite(PIN_LED_G, g);
  analogWrite(PIN_LED_B, b);
}

void sendDigitalTwinTelemetry() {
  // Serial JSON Stream for Node-RED / Python Dashboard
  Serial.print("{\"asset_id\":\"MTR_001\",");
  Serial.print("\"runtime_s\":"); Serial.print(operatingSeconds); Serial.print(",");
  Serial.print("\"temp_c\":"); Serial.print(tempC); Serial.print(",");
  Serial.print("\"vibration\":"); Serial.print(vibration); Serial.print(",");
  Serial.print("\"current_a\":"); Serial.print(currentA); Serial.print(",");
  Serial.print("\"health_index\":"); Serial.print(healthIndex); Serial.print(",");
  Serial.print("\"rul_hours\":"); Serial.print(estimatedRULHours);
  Serial.println("}");
}