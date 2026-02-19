// =============================================================================
// Cellar Pump Controller
// =============================================================================
// Controls a submersible pump via a relay on a timed schedule.
// Displays temperature, humidity, and pump status on an LCD.
// See README.md for full requirements and equipment details.
// =============================================================================

#include <Arduino.h>
#include <Wire.h>
#include <EEPROM.h>

// Feature toggles — comment out to disable
#define ENABLE_SERIAL_LOGGING
#define ENABLE_DISPLAY
#define ENABLE_DISPLAY_RGB
#define ENABLE_TEMP_HUMIDITY_SENSOR
#define ENABLE_PRESET_BUTTON

// ENABLE_DISPLAY_RGB implies ENABLE_DISPLAY
#ifdef ENABLE_DISPLAY_RGB
  #ifndef ENABLE_DISPLAY
    #define ENABLE_DISPLAY
  #endif
#endif

// =============================================================================
// Pin Configuration
// =============================================================================

const int RELAY_PIN = 4; // Grove Relay on digital pin 4

#ifdef ENABLE_PRESET_BUTTON
const int BUTTON_PIN = 3; // Grove Button on digital pin 3
#endif

// =============================================================================
// Timing Configuration (in milliseconds)
// =============================================================================

// Default durations (used when preset button is disabled, or as preset 0)
const unsigned long DEFAULT_PUMP_ON_DURATION    = 1000UL * 60;  // seconds on
const unsigned long DEFAULT_PUMP_CYCLE_INTERVAL = 1000UL * 60 * 5; // minutes between activations
const unsigned long DISPLAY_UPDATE_INTERVAL     = 500; // ms between display updates
const unsigned long SENSOR_READ_INTERVAL        = 1000UL * 2; // seconds between sensor reads

// Active durations — set from preset or defaults
unsigned long pumpOnDuration    = DEFAULT_PUMP_ON_DURATION;
unsigned long pumpCycleInterval = DEFAULT_PUMP_CYCLE_INTERVAL;

// =============================================================================
// Backlight threshold: show green when less than 5 minutes remain
// =============================================================================

const unsigned long GREEN_THRESHOLD = 1000UL * 60 * 5; // minutes

// =============================================================================
// Preset Profiles (button cycles through these)
// =============================================================================

#ifdef ENABLE_PRESET_BUTTON

struct Preset {
  unsigned long onDuration;     // ms
  unsigned long cycleInterval;  // ms
  const char*   label;          // shown on LCD (max 16 chars)
};

const Preset PRESETS[] = {
  { 60UL * 1000,  30UL * 60 * 1000,       "1: 60s / 30min"   }, // 0 — default
  { 60UL * 1000,   2UL * 60 * 60 * 1000,  "2: 60s / 2h"      }, // 1
  { 60UL * 1000,   6UL * 60 * 60 * 1000,  "3: 60s / 6h"      }, // 2
  { 60UL * 1000,  24UL * 60 * 60 * 1000,  "4: 60s / 1day"    }, // 3
  { 60UL * 1000,   1UL * 60 * 1000,       "5: 60s / 1min"    }, // 4
  { 60UL * 1000,   4UL * 60 * 1000,       "6: 60s / 4min"    }, // 5
  { 60UL * 1000,  10UL * 60 * 1000,       "7: 60s / 10min"   }, // 6
};
const uint8_t PRESET_COUNT = sizeof(PRESETS) / sizeof(PRESETS[0]);

// EEPROM layout
const int EEPROM_ADDR_MAGIC  = 0; // 1 byte: validity marker
const int EEPROM_ADDR_PRESET = 1; // 1 byte: preset index
const uint8_t EEPROM_MAGIC   = 0xC7; // arbitrary marker

// Button state
const unsigned long DEBOUNCE_MS = 50;
const unsigned long OVERLAY_DISPLAY_MS = 1000UL * 2; // show preset name for 1 s

uint8_t currentPreset = 0;
bool    lastButtonState = LOW;
unsigned long lastDebounceTime = 0;
unsigned long overlayStartTime = 0; // when overlay was triggered
bool overlayShowing = false;        // true while overlay is on screen

void applyPreset(uint8_t idx) {
  if (idx >= PRESET_COUNT) idx = 0;
  currentPreset    = idx;
  pumpOnDuration   = PRESETS[idx].onDuration;
  pumpCycleInterval = PRESETS[idx].cycleInterval;
}

void savePresetToEEPROM(uint8_t idx) {
  EEPROM.update(EEPROM_ADDR_MAGIC, EEPROM_MAGIC);
  EEPROM.update(EEPROM_ADDR_PRESET, idx);
}

uint8_t loadPresetFromEEPROM() {
  if (EEPROM.read(EEPROM_ADDR_MAGIC) != EEPROM_MAGIC) return 0;
  uint8_t idx = EEPROM.read(EEPROM_ADDR_PRESET);
  return (idx < PRESET_COUNT) ? idx : 0;
}

#endif // ENABLE_PRESET_BUTTON

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
#ifdef ENABLE_DISPLAY_RGB
  lcd.setRGB(0, 0, 0);
#endif
  lcd.print("Initializing...");
}

#ifdef ENABLE_DISPLAY_RGB

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

#endif // ENABLE_DISPLAY_RGB

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
    if (elapsed < pumpOnDuration) {
      remaining = (pumpOnDuration - elapsed) / 1000;
    }
    snprintf(line2, sizeof(line2), "Pump on %lus", remaining);
  } else {
    // Show time remaining until next activation
    unsigned long elapsed = millis() - pumpStopTime;
    unsigned long remainingMs = 0;
    if (elapsed < pumpCycleInterval) {
      remainingMs = pumpCycleInterval - elapsed;
    }
    unsigned long remainingSec = remainingMs / 1000;
    if (remainingSec <= 120) {
      snprintf(line2, sizeof(line2), "Pump off %lus", remainingSec);
    } else {
      unsigned long remainingMin = (remainingSec + 30) / 60;
      if (remainingMin > 120) {
        unsigned long remainingHours = (remainingMin + 30) / 60;
        snprintf(line2, sizeof(line2), "Pump off %luh", remainingHours);
      } else {
        snprintf(line2, sizeof(line2), "Pump off %lum", remainingMin);
      }
    }
  }
  lcd.print(line2);

  // --- Backlight color ---
#ifdef ENABLE_DISPLAY_RGB
  if (pumpRunning) {
    setBacklightRed();
  } else {
    unsigned long elapsed = millis() - pumpStopTime;
    unsigned long remainingMs = 0;
    if (elapsed < pumpCycleInterval) {
      remainingMs = pumpCycleInterval - elapsed;
    }
    if (remainingMs < GREEN_THRESHOLD) {
      setBacklightGreen();
    } else {
      setBacklightOff();
    }
  }
#endif // ENABLE_DISPLAY_RGB
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
    // Turn off after pumpOnDuration
    if (now - pumpStartTime >= pumpOnDuration) {
      pumpOff();
    }
  } else {
    // Turn on after pumpCycleInterval since last stop
    if (now - pumpStopTime >= pumpCycleInterval) {
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

#ifdef ENABLE_PRESET_BUTTON
  pinMode(BUTTON_PIN, INPUT);
  applyPreset(loadPresetFromEEPROM());
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

  // --- Check preset button ---
#ifdef ENABLE_PRESET_BUTTON
  {
    bool reading = digitalRead(BUTTON_PIN);
    if (reading != lastButtonState) {
      lastDebounceTime = now;
    }
    if ((now - lastDebounceTime) >= DEBOUNCE_MS) {
      // Stable reading — detect rising edge (press)
      static bool stableState = LOW;
      if (reading != stableState) {
        stableState = reading;
        if (stableState == HIGH) {
          // Button just pressed — cycle to next preset
          uint8_t next = (currentPreset + 1) % PRESET_COUNT;
          applyPreset(next);
          savePresetToEEPROM(next);

          // Reset pump timers: turn pump off and restart countdown
          if (pumpRunning) {
            digitalWrite(RELAY_PIN, LOW);
            pumpRunning = false;
          }
          pumpStopTime = now;

          // Show overlay on LCD
          overlayStartTime = now;
          overlayShowing = true;
#ifdef ENABLE_DISPLAY
          lcd.clear();
          delay(2);
          lcd.setRGB(0, 0, 100); // blue during overlay
          lcd.print(F("Preset:"));
          lcd.setCursor(0, 1);
          lcd.print(PRESETS[currentPreset].label);
#endif

#ifdef ENABLE_SERIAL_LOGGING
          Serial.print(F("Preset -> "));
          Serial.println(PRESETS[currentPreset].label);
#endif
        }
      }
    }
    lastButtonState = reading;
  }
#endif // ENABLE_PRESET_BUTTON

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
  // Skip normal display refresh while overlay is shown
#ifdef ENABLE_PRESET_BUTTON
  if (overlayShowing && (now - overlayStartTime >= OVERLAY_DISPLAY_MS)) {
    overlayShowing = false; // overlay expired — resume normal display
  }
  bool overlayActive = overlayShowing;
#else
  bool overlayActive = false;
#endif
  if (!overlayActive && (now - lastDisplayUpdate >= DISPLAY_UPDATE_INTERVAL)) {
    lastDisplayUpdate = now;
    updateDisplay();
  }
#endif
}
