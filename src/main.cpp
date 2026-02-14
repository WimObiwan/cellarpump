// =============================================================================
// Cellar Pump Controller
// =============================================================================
// Controls a submersible pump via a relay on a timed schedule.
// Displays temperature, humidity, and pump status on an LCD.
// See README.md for full requirements and equipment details.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>

// Feature toggles — comment out to disable
#define ENABLE_SERIAL_LOGGING
#define ENABLE_DISPLAY
#define ENABLE_TEMP_HUMIDITY_SENSOR

// =============================================================================
// Pin Configuration
// =============================================================================

const int RELAY_PIN = 4; // Grove Relay on digital pin 4

// =============================================================================
// Timing Configuration (in milliseconds)
// =============================================================================

const unsigned long PUMP_ON_DURATION    = 60UL * 1000;  // 60 seconds on
const unsigned long PUMP_CYCLE_INTERVAL = 30UL * 60 * 1000; // 10 minutes between activations
const unsigned long DISPLAY_UPDATE_INTERVAL = 500; // Update display every 500ms
const unsigned long SENSOR_READ_INTERVAL = 2000; // Read sensor every 2 seconds

// =============================================================================
// Backlight threshold: show green when less than 5 minutes remain
// =============================================================================

const unsigned long GREEN_THRESHOLD = 5UL * 60 * 1000; // 5 minutes

// =============================================================================
// Global State
// =============================================================================

bool pumpRunning = false;
unsigned long pumpStartTime = 0;    // When the pump was last turned on
unsigned long pumpStopTime = 0;     // When the pump was last turned off
unsigned long lastDisplayUpdate = 0;
unsigned long lastSensorRead = 0;

float temperature = 0.0f;
float humidity = 0.0f;

// =============================================================================
// SERIAL LOGGING
// =============================================================================

#ifdef ENABLE_SERIAL_LOGGING

void initSerial() {
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port (needed for some boards)
  }
  Serial.println(F("Cellar Pump Controller started"));
}

void logPumpOn() {
  Serial.print(F("Pump ON  | Temp: "));
  Serial.print(temperature, 1);
  Serial.print(F("C | Hum: "));
  Serial.print(humidity, 1);
  Serial.println(F("%"));
}

void logPumpOff() {
  Serial.print(F("Pump OFF | Temp: "));
  Serial.print(temperature, 1);
  Serial.print(F("C | Hum: "));
  Serial.print(humidity, 1);
  Serial.println(F("%"));
}

#endif // ENABLE_SERIAL_LOGGING

// =============================================================================
// TEMPERATURE & HUMIDITY SENSOR (Grove DHT20, I2C)
// =============================================================================

#ifdef ENABLE_TEMP_HUMIDITY_SENSOR

#include "DHT.h"

DHT dht(DHT20);

void initSensor() {
  dht.begin();
}

// Reads temperature and humidity into global variables.
void readSensor() {
  float values[2];
  if (!dht.readTempAndHumidity(values)) {
    // values[0] = humidity, values[1] = temperature (DHT20 convention)
    humidity = values[0];
    temperature = values[1];
  }
  // On failure, keep previous values
}

#endif // ENABLE_TEMP_HUMIDITY_SENSOR

// =============================================================================
// DISPLAY (Grove LCD RGB Backlight 16x2)
// =============================================================================

#ifdef ENABLE_DISPLAY

#include "rgb_lcd.h"

rgb_lcd lcd;

void initDisplay() {
  lcd.begin(16, 2);
  lcd.setRGB(0, 0, 0);
  lcd.print("Initializing...");
}

// Set backlight to red (pump is on)
void setBacklightRed() {
  lcd.setRGB(100, 0, 0);
}

// Set backlight to green (pump off, activation soon)
void setBacklightGreen() {
  lcd.setRGB(0, 100, 0);
}

// Set backlight off (pump off, not near activation)
void setBacklightOff() {
  lcd.setRGB(0, 0, 0);
}

// Update the LCD with current status.
void updateDisplay() {
  lcd.clear();
  delay(2); // LCD needs brief delay after clear

  // --- Line 1: Temperature & Humidity ---
#ifdef ENABLE_TEMP_HUMIDITY_SENSOR
  char line1[17];
  // Format: "T:xx.xC H:xx.x%"
  dtostrf(temperature, 4, 1, line1);
  char humStr[6];
  dtostrf(humidity, 4, 1, humStr);

  char buf1[17];
  snprintf(buf1, sizeof(buf1), "T:%sC H:%s%%", line1, humStr);
  lcd.print(buf1);
#else
  lcd.print("No sensor");
#endif

  // --- Line 2: Pump status & countdown ---
  lcd.setCursor(0, 1);
  char line2[17];

  if (pumpRunning) {
    // Show seconds remaining until pump turns off
    unsigned long elapsed = millis() - pumpStartTime;
    unsigned long remaining = 0;
    if (elapsed < PUMP_ON_DURATION) {
      remaining = (PUMP_ON_DURATION - elapsed) / 1000;
    }
    snprintf(line2, sizeof(line2), "Pump on %lus", remaining);
  } else {
    // Show minutes remaining until next activation
    unsigned long elapsed = millis() - pumpStopTime;
    unsigned long remaining = 0;
    if (elapsed < PUMP_CYCLE_INTERVAL) {
      remaining = (PUMP_CYCLE_INTERVAL - elapsed) / 1000 / 60;
    }
    snprintf(line2, sizeof(line2), "Pump off %lum", remaining);
  }
  lcd.print(line2);

  // --- Backlight color ---
  if (pumpRunning) {
    setBacklightRed();
  } else {
    unsigned long elapsed = millis() - pumpStopTime;
    if (elapsed >= PUMP_CYCLE_INTERVAL - GREEN_THRESHOLD) {
      setBacklightGreen();
    } else {
      setBacklightOff();
    }
  }
}

#endif // ENABLE_DISPLAY

// =============================================================================
// PUMP / RELAY CONTROL
// =============================================================================

void initRelay() {
  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
}

// Activate the pump (relay on)
void pumpOn() {
  if (pumpRunning) return; // Already on

  digitalWrite(RELAY_PIN, HIGH);
  pumpRunning = true;
  pumpStartTime = millis();

#ifdef ENABLE_SERIAL_LOGGING
  logPumpOn();
#endif
}

// Deactivate the pump (relay off)
void pumpOff() {
  if (!pumpRunning) return; // Already off

  digitalWrite(RELAY_PIN, LOW);
  pumpRunning = false;
  pumpStopTime = millis();

#ifdef ENABLE_SERIAL_LOGGING
  logPumpOff();
#endif
}

// Non-blocking pump state machine.
// Call this every loop iteration.
void updatePump() {
  unsigned long now = millis();

  if (pumpRunning) {
    // Turn off after PUMP_ON_DURATION
    if (now - pumpStartTime >= PUMP_ON_DURATION) {
      pumpOff();
    }
  } else {
    // Turn on after PUMP_CYCLE_INTERVAL since last stop
    if (now - pumpStopTime >= PUMP_CYCLE_INTERVAL) {
      pumpOn();
    }
  }
}

// =============================================================================
// SETUP
// =============================================================================

void setup() {
#ifdef ENABLE_SERIAL_LOGGING
  initSerial();
#endif

  Wire.begin();

#ifdef ENABLE_TEMP_HUMIDITY_SENSOR
  initSensor();
#endif

#ifdef ENABLE_DISPLAY
  initDisplay();
#endif

  initRelay();

  // Upon startup, turn the pump on immediately.
  // pumpStopTime is 0 so the first cycle triggers right away,
  // but we explicitly call pumpOn() for clarity.
  pumpOn();
}

// =============================================================================
// MAIN LOOP
// =============================================================================

void loop() {
  unsigned long now = millis();

  // --- Update pump state (non-blocking) ---
  updatePump();

  // --- Read sensor periodically ---
#ifdef ENABLE_TEMP_HUMIDITY_SENSOR
  if (now - lastSensorRead >= SENSOR_READ_INTERVAL) {
    lastSensorRead = now;
    readSensor();
  }
#endif

  // --- Update display periodically ---
#ifdef ENABLE_DISPLAY
  if (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL) {
    lastDisplayUpdate = now;
    updateDisplay();
  }
#endif
}
