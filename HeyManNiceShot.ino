#include <Arduino.h>
#include "driver/i2s.h"
#include <math.h>       // For sqrt
#include <Wire.h>       // For I2C
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_random.h> // For hardware RNG
#include <Preferences.h> // For Non-Volatile Storage

// --- Pin Definitions (Using XIAO ESP32C3 D# Aliases) ---
#define BUTTON1_PIN   D0  // D0 - Start / Reset / Settings Up / Next Setting Button (Black)
#define BUTTON2_PIN   D1  // D1 - Stop Timing / Settings Down / Prev Setting / Enter Settings Button (Green)
#define BUZZER_PIN_1  D6  // D6 - Buzzer Pin 1 (Connect to tone/digital output) (GPIO_NUM_7)
#define BUZZER_PIN_2  D3  // D3 - Buzzer Pin 2 (Connect to GND or other digital output if driving differentially) (GPIO_NUM_4)

#define I2C_SDA_PIN   D4  // D4 - I2C Data (GPIO_NUM_5)
#define I2C_SCL_PIN   D5  // D5 - I2C Clock (GPIO_NUM_6)

#define I2S_WS_PIN    D9  // D9 - Word Select (LRCL) (GPIO_NUM_9)
#define I2S_SCK_PIN   D8  // D8 - Serial Clock (BCLK) (GPIO_NUM_8)
#define I2S_SD_PIN    D10 // D10 - Serial Data (DIN) (GPIO_NUM_10)

// --- Display Configuration ---
#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels
#define OLED_RESET    -1 // Reset pin # (or -1 if sharing Arduino reset pin)
#define SCREEN_ADDRESS 0x3C // Or 0x3D depending on your display module
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- I2S Configuration ---
#define I2S_PORT        (I2S_NUM_0)
#define I2S_SAMPLE_RATE (16000)
#define I2S_BITS_PER_SAMPLE (I2S_BITS_PER_SAMPLE_32BIT)
#define I2S_CHANNEL_FORMAT (I2S_CHANNEL_FMT_ONLY_LEFT)
#define I2S_READ_BUFFER_LENGTH (256)
#define I2S_DMA_BUFFER_COUNT   (8)
#define I2S_DMA_BUFFER_LENGTH  (64)
int32_t i2s_read_buffer[I2S_READ_BUFFER_LENGTH];

// --- Shot Timer Configuration ---
#define CALIBRATION_DURATION_MS 5000 // Calibration time
#define SHOT_THRESHOLD_RMS (27589306.85) // RMS value (NEEDS TESTING/TUNING) // for live rounds use (3888603469.0)
#define SHOT_LOCKOUT_MICROS 500000 // 500ms lockout after a shot detection
#define ABSOLUTE_MAX_SHOTS 20     // Max size of the timestamp array
#define RANDOM_DELAY_MIN_MS 1000
#define RANDOM_DELAY_MAX_MS 3000
#define BUTTON_DEBOUNCE_MS 50
#define LONG_PRESS_DURATION_MS 1000 // Duration for long press detection
#define SETTINGS_TIMEOUT_MS 5000  // Timeout to save settings
#define SETTINGS_CYCLE_DELAY_MS 750 // Delay between auto-cycles when button held (milliseconds)
#define TIMING_TIMEOUT_MS 30000   // 30 second timeout for TIMING state
#define SUCCESS_BEEP_DURATION 25  // Duration for each success beep
#define SUCCESS_BEEP_DELAY 25    // Delay between success beeps

// --- Settings Limits ---
#define MAX_SHOTS_MIN 1
#define MAX_SHOTS_MAX ABSOLUTE_MAX_SHOTS
#define BEEP_DURATION_MIN_MS 50
#define BEEP_DURATION_MAX_MS 2000
#define BEEP_TONE_MIN_HZ 500
#define BEEP_TONE_MAX_HZ 4000
#define BEEP_TONE_STEP 100

// --- Global Variables ---
Preferences preferences; // NVS object
double ambientRmsLevel = 0.0;
double currentRms = 0.0;

// --- Runtime Settings (Loaded from NVS) ---
uint8_t currentMaxShots = 10;
uint16_t currentBeepDurationMs = 400; // Default value
uint16_t currentBeepToneHz = 2000;    // Default value

// State Machine
enum TimerState { IDLE, READY, WAITING, TIMING, DISPLAY_RESULT, SETTINGS, CALIBRATION_ERROR }; // Renamed SETTINGS state
TimerState currentState = IDLE;

// Timing Variables
unsigned long stateEntryTime = 0;
unsigned long beepStartTime = 0;
unsigned long shotTimestamps[ABSOLUTE_MAX_SHOTS];
int shotCount = 0;
unsigned long lastShotDetectTime = 0;
unsigned long randomDelayDuration = 0;
float firstShotTime = 0.0;
float lastSplitTime = 0.0;

// Button States & Long Press Tracking
bool button1Pressed = false; // Short press event flag
bool button2Pressed = false; // Short press event flag
bool button1LongPressTriggered = false; // Single-shot long press event flag
bool button2LongPressTriggered = false; // Single-shot long press event flag
unsigned long lastButton1PressTime = 0; // For debounce
unsigned long lastButton2PressTime = 0; // For debounce
bool lastButton1State = LOW; // Start LOW for PULLDOWN
bool lastButton2State = LOW; // Start LOW for PULLDOWN
unsigned long button1PressStartTime = 0; // Time button press started
unsigned long button2PressStartTime = 0; // Time button press started
bool button1Held = false; // Is button currently held down?
bool button2Held = false; // Is button currently held down?
bool button1LongPressProcessed = false; // Has the long press event for this hold been processed?
bool button2LongPressProcessed = false; // Has the long press event for this hold been processed?


// Settings State Variables
int currentSettingIndex = 0; // 0: Max Shots, 1: Beep Duration, 2: Beep Tone
uint8_t maxShotsSetting = 10;
uint16_t beepDurationSetting = 400;
uint16_t beepToneSetting = 2000;
unsigned long lastSettingsInteractionTime = 0;
unsigned long nextCycleTime = 0; // Timestamp when the next auto-cycle is allowed
bool cycleDelayActive = false; // Flag indicating if we are waiting for the cycle delay

// Buzzer State
bool isBeeping = false;


// --- Function Prototypes ---
void setupI2S();
void setupDisplay();
void setupPins();
bool calibrateAmbientNoise();
void readMicrophone();
void readButtons(); // Updated to detect long presses
void updateDisplay();
void startBeep();
void stopBeep();
void changeState(TimerState newState);
void resetShotData();
void loadSettings();
void saveSettings();
void playSuccessBeeps(); // Added prototype for success beeps
void playUnsuccessBeeps();

// --- Setup Function ---
void setup() {
  Serial.begin(115200);
  //while (!Serial);
  Serial.println("ESP32C3 Shot Timer Starting...");

  loadSettings(); // Load saved settings from NVS
  setupPins();
  setupDisplay();
  setupI2S();

  // --- Display Boot Screen during Calibration ---
  display.clearDisplay();
  display.setTextSize(1); display.setTextColor(SSD1306_WHITE);
  display.setCursor(10, 10); display.println("Hey Man, Nice Shot");
  display.drawFastHLine(0, 22, SCREEN_WIDTH, SSD1306_WHITE);
  display.setCursor(10, 30); display.setTextSize(1);
  display.println("Quiet, calibrating");
  display.setCursor(12, 40); display.println("for ambient noise");
  display.display();
  Serial.println("Displaying boot screen, starting calibration...");

  // --- Calibrate Ambient Noise ---
  bool calibrationSuccess = calibrateAmbientNoise();

  // --- Handle Calibration Result ---
  if (!calibrationSuccess) {
    Serial.println("!!! AMBIENT NOISE CALIBRATION FAILED !!!");
    display.clearDisplay();
    display.setTextSize(2); display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10); display.println("CALIB");
    display.setCursor(10, 35); display.println("FAILED!");
    display.display();
    changeState(CALIBRATION_ERROR);
    while (true) { delay(100); }
  }

  // --- Calibration Succeeded ---
  Serial.println("Calibration Successful.");
  playSuccessBeeps(); // Play the success beep sequence

  // --- Show Ready Screen ---
  Serial.println("Entering IDLE state.");
  display.clearDisplay();
  display.setTextSize(2); display.setTextColor(SSD1306_WHITE);
  display.setCursor(40, 10); display.println("READY");
  display.setTextSize(1); display.setCursor(25, 35); display.println("Black - Start");
  display.setCursor(10, 50); display.println("Hold Green - Setup");
  display.display();

  resetShotData();
  changeState(IDLE); // Set initial state AFTER displaying ready screen
}

// --- Main Loop ---
void loop() {
    if (currentState == CALIBRATION_ERROR) { delay(100); return; }

  readButtons(); // Reads both short and long presses
  readMicrophone();

  unsigned long now = millis();
  unsigned long now_micros = micros(); // Use micros for timing critical checks

  // --- State Machine Logic ---
  switch (currentState) {
    case IDLE:
      if (button1Pressed) { changeState(READY); }
      // Check for Button 2 LONG press to enter settings
      if (button2LongPressTriggered) { // Use the single-shot trigger flag
          Serial.println("Button 2 Long Press Detected - Entering Settings");
          // Initialize temporary settings with current values
          maxShotsSetting = currentMaxShots;
          beepDurationSetting = currentBeepDurationMs;
          beepToneSetting = currentBeepToneHz;
          currentSettingIndex = 0; // Start at the first setting
          lastSettingsInteractionTime = now; // Start timeout timer
          cycleDelayActive = false; // Reset cycle delay flag
          nextCycleTime = 0;
          changeState(SETTINGS);
      }
      break;

    case READY:
      randomDelayDuration = esp_random() % (RANDOM_DELAY_MAX_MS - RANDOM_DELAY_MIN_MS + 1) + RANDOM_DELAY_MIN_MS;
      Serial.printf("Random delay set to: %lu ms\n", randomDelayDuration);
      changeState(WAITING);
      break;

    case WAITING:
      if (now - stateEntryTime >= randomDelayDuration) { changeState(TIMING); }
      if (button1Pressed) { Serial.println("Start cancelled by user."); changeState(IDLE); }
      break;

    case TIMING:
      // Stop beep if duration passed (tone() with duration handles this, but good backup)
      if (isBeeping && (now_micros - beepStartTime >= (unsigned long)currentBeepDurationMs * 1000)) {
          stopBeep();
      }

      // Check for shot detection
      if (currentRms > SHOT_THRESHOLD_RMS && (now_micros - lastShotDetectTime > SHOT_LOCKOUT_MICROS)) {
        if (shotCount < currentMaxShots) {
          unsigned long currentShotTime = now_micros;
          shotTimestamps[shotCount] = currentShotTime;
          lastShotDetectTime = currentShotTime;

          if (shotCount == 0) {
            firstShotTime = (float)(currentShotTime - beepStartTime) / 1000000.0;
            lastSplitTime = firstShotTime;
            Serial.printf("First Shot Detected! Time: %.3f s (RMS: %.2f)\n", firstShotTime, currentRms);
          } else {
            lastSplitTime = (float)(currentShotTime - shotTimestamps[shotCount - 1]) / 1000000.0;
            Serial.printf("Split %d Detected! Time: %.3f s (RMS: %.2f)\n", shotCount + 1, lastSplitTime, currentRms);
          }
          shotCount++;
          if (shotCount >= currentMaxShots) {
              Serial.println("Max shots reached, stopping timer.");
              stopBeep(); changeState(DISPLAY_RESULT); break;
          }
        } else { Serial.println("Max shots already recorded."); }
      }

      // Check for stop button
      if (currentState == TIMING && button2Pressed) {
          Serial.println("Timing stopped by user (Button 2).");
          stopBeep(); changeState(DISPLAY_RESULT); break;
      }

      // Check for TIMING Timeout
      if (currentState == TIMING && (now - (beepStartTime / 1000) > TIMING_TIMEOUT_MS)) {
          Serial.println("Timeout - Stopping timer.");
          stopBeep(); changeState(DISPLAY_RESULT); break;
      }
      break; // End of TIMING case

    case DISPLAY_RESULT:
      if (button1Pressed) { changeState(IDLE); }
      break;

    case SETTINGS:
      bool interactionOccurred = false;

      // --- Handle Value Adjustment (Short Presses) ---
      if (button1Pressed) {
          switch (currentSettingIndex) {
              case 0: if (maxShotsSetting < MAX_SHOTS_MAX) maxShotsSetting++; break;
              case 1: if (beepDurationSetting < BEEP_DURATION_MAX_MS) beepDurationSetting += 50; if (beepDurationSetting > BEEP_DURATION_MAX_MS) beepDurationSetting = BEEP_DURATION_MAX_MS; break;
              case 2: if (beepToneSetting < BEEP_TONE_MAX_HZ) beepToneSetting += BEEP_TONE_STEP; if (beepToneSetting > BEEP_TONE_MAX_HZ) beepToneSetting = BEEP_TONE_MAX_HZ; break;
          }
          interactionOccurred = true; cycleDelayActive = false;
          Serial.printf("Setting %d Increased\n", currentSettingIndex);
      }
      if (button2Pressed) {
          switch (currentSettingIndex) {
              case 0: if (maxShotsSetting > MAX_SHOTS_MIN) maxShotsSetting--; break;
              case 1: if (beepDurationSetting > BEEP_DURATION_MIN_MS) beepDurationSetting -= 50; if (beepDurationSetting < BEEP_DURATION_MIN_MS) beepDurationSetting = BEEP_DURATION_MIN_MS; break;
              case 2: if (beepToneSetting > BEEP_TONE_MIN_HZ) beepToneSetting -= BEEP_TONE_STEP; if (beepToneSetting < BEEP_TONE_MIN_HZ) beepToneSetting = BEEP_TONE_MIN_HZ; break;
          }
          interactionOccurred = true; cycleDelayActive = false;
          Serial.printf("Setting %d Decreased\n", currentSettingIndex);
      }

      // --- Handle Setting Cycling (Long Presses - Initial Trigger) ---
      // Use the single-shot buttonXLongPressTriggered flag
      if (button1LongPressTriggered) {
          currentSettingIndex = (currentSettingIndex + 1) % 3;
          interactionOccurred = true;
          cycleDelayActive = true; // Enable auto-cycle check
          nextCycleTime = now + SETTINGS_CYCLE_DELAY_MS; // Set time for *first* auto-repeat
          Serial.printf("Cycled to Setting Index: %d (Initial Long Press B1)\n", currentSettingIndex);
      }
      if (button2LongPressTriggered) {
          currentSettingIndex = (currentSettingIndex + 3 - 1) % 3;
          interactionOccurred = true;
          cycleDelayActive = true; // Enable auto-cycle check
          nextCycleTime = now + SETTINGS_CYCLE_DELAY_MS; // Set time for *first* auto-repeat
          Serial.printf("Cycled to Setting Index: %d (Initial Long Press B2)\n", currentSettingIndex);
      }

      // --- Handle Setting Cycling (Auto-Repeat while Held) ---
      // Only check for auto-repeat if the delay is active and the time has passed
      if (cycleDelayActive && now >= nextCycleTime) {
          // Check if the corresponding button is *still held down*
          if (button1Held) {
              currentSettingIndex = (currentSettingIndex + 1) % 3;
              interactionOccurred = true;
              nextCycleTime = now + SETTINGS_CYCLE_DELAY_MS; // Reset delay for next cycle
              Serial.printf("Cycled to Setting Index: %d (Auto-Repeat B1)\n", currentSettingIndex);
          } else if (button2Held) {
              currentSettingIndex = (currentSettingIndex + 3 - 1) % 3;
              interactionOccurred = true;
              nextCycleTime = now + SETTINGS_CYCLE_DELAY_MS; // Reset delay for next cycle
              Serial.printf("Cycled to Setting Index: %d (Auto-Repeat B2)\n", currentSettingIndex);
          } else {
              // Button released, stop auto-cycle
              cycleDelayActive = false;
          }
      }

      // If neither button is held anymore, disable the cycle delay check
      if (!button1Held && !button2Held) {
          cycleDelayActive = false;
      }

      // Reset timeout timer if any interaction occurred
      if (interactionOccurred) {
          lastSettingsInteractionTime = now;
      }

      // Check for timeout to save and exit
      if (now - lastSettingsInteractionTime >= SETTINGS_TIMEOUT_MS) {
          Serial.println("Settings timeout - Saving and exiting.");
          saveSettings(); // Save all temporary settings to NVS
          // Update runtime variables from temporary settings
          currentMaxShots = maxShotsSetting;
          currentBeepDurationMs = beepDurationSetting;
          currentBeepToneHz = beepToneSetting;
          changeState(IDLE);
      }
      break; // End of SETTINGS case

    // CALIBRATION_ERROR state is handled at the top of loop()
  } // End of switch(currentState)

  updateDisplay();
  delay(5);
} // End of loop()

// --- State Change Helper ---
void changeState(TimerState newState) {
  if (currentState == CALIBRATION_ERROR && newState != IDLE) { return; }

  Serial.printf("Changing state from %d to: %d\n", currentState, newState);
  currentState = newState;
  stateEntryTime = millis(); // Record time of entering the new state (using millis)

  if (newState == TIMING) {
    beepStartTime = micros(); // Record precise start time for timing (using micros)
    lastShotDetectTime = beepStartTime; // Initialize lockout relative to beep start
    startBeep();
  } else {
      // Ensure beep is stopped when leaving TIMING state or entering others
      if(isBeeping) stopBeep();
  }

  if (newState == IDLE) {
      resetShotData();
  }
  // Reset button states completely on state change
  button1Held = false; button1PressStartTime = 0; button1LongPressTriggered = false; button1LongPressProcessed = false;
  button2Held = false; button2PressStartTime = 0; button2LongPressTriggered = false; button2LongPressProcessed = false;
  cycleDelayActive = false; nextCycleTime = 0;
}

// --- Reset Shot Data ---
void resetShotData() {
    shotCount = 0;
    firstShotTime = 0.0;
    lastSplitTime = 0.0;
    beepStartTime = 0;
    lastShotDetectTime = 0;
    for (int i = 0; i < ABSOLUTE_MAX_SHOTS; i++) {
        shotTimestamps[i] = 0;
    }
    Serial.println("Shot data reset.");
}

// --- NVS Settings Functions ---
void loadSettings() {
    preferences.begin("shotTimer", true); // Start NVS, read-only
    currentMaxShots = preferences.getUChar("max_shots", 10);
    currentBeepDurationMs = preferences.getUShort("beep_dur", 400);
    currentBeepToneHz = preferences.getUShort("beep_tone", 2000);
    preferences.end();

    // --- Apply Bounds ---
    if (currentMaxShots < MAX_SHOTS_MIN) currentMaxShots = MAX_SHOTS_MIN;
    if (currentMaxShots > MAX_SHOTS_MAX) currentMaxShots = MAX_SHOTS_MAX;
    if (currentBeepDurationMs < BEEP_DURATION_MIN_MS) currentBeepDurationMs = BEEP_DURATION_MIN_MS;
    if (currentBeepDurationMs > BEEP_DURATION_MAX_MS) currentBeepDurationMs = BEEP_DURATION_MAX_MS;
    if (currentBeepToneHz < BEEP_TONE_MIN_HZ) currentBeepToneHz = BEEP_TONE_MIN_HZ;
    if (currentBeepToneHz > BEEP_TONE_MAX_HZ) currentBeepToneHz = BEEP_TONE_MAX_HZ;

    Serial.printf("Loaded Settings - Max Shots: %d, Beep Duration: %d ms, Beep Tone: %d Hz\n",
                  currentMaxShots, currentBeepDurationMs, currentBeepToneHz);
}

void saveSettings() {
    preferences.begin("shotTimer", false); // Start NVS, read/write
    preferences.putUChar("max_shots", maxShotsSetting);
    preferences.putUShort("beep_dur", beepDurationSetting);
    preferences.putUShort("beep_tone", beepToneSetting);
    preferences.end();
    Serial.printf("Saved Settings - Max Shots: %d, Beep Duration: %d ms, Beep Tone: %d Hz\n",
                  maxShotsSetting, beepDurationSetting, beepToneSetting);
}


// --- Hardware Setup Functions ---
void setupPins() {
  pinMode(BUTTON1_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON2_PIN, INPUT_PULLDOWN);
  pinMode(BUZZER_PIN_1, OUTPUT);
  pinMode(BUZZER_PIN_2, OUTPUT);
  digitalWrite(BUZZER_PIN_1, LOW);
  digitalWrite(BUZZER_PIN_2, LOW);
}

void setupDisplay() {
  Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(1000); // Small delay before Wire.begin
  Wire.begin();
  delay(1000); // Small delay after Wire.begin
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(true);
  }
  display.clearDisplay();
  display.display();
  Serial.println("SSD1306 Display Initialized.");
}

void setupI2S() {
  Serial.println("Configuring I2S...");
  i2s_config_t i2s_config = { /* ... I2S config ... */
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = I2S_SAMPLE_RATE,
    .bits_per_sample = I2S_BITS_PER_SAMPLE,
    .channel_format = I2S_CHANNEL_FORMAT,
    .communication_format = I2S_COMM_FORMAT_STAND_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = I2S_DMA_BUFFER_COUNT,
    .dma_buf_len = I2S_DMA_BUFFER_LENGTH,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0
  };
  i2s_pin_config_t pin_config = { /* ... I2S pins ... */
    .bck_io_num = I2S_SCK_PIN,       // D8
    .ws_io_num = I2S_WS_PIN,        // D9
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN        // D10
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) { Serial.printf("!!! Failed installing I2S driver: %d\n", err); while (true); }
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) { Serial.printf("!!! Failed setting I2S pins: %d\n", err); i2s_driver_uninstall(I2S_PORT); while (true); }
  Serial.println("I2S Driver installed.");
  delay(100);
}

// --- Ambient Noise Calibration ---
bool calibrateAmbientNoise() {
   unsigned long calibStartTime = millis();
   double totalRmsSum = 0.0;
   long rmsCount = 0;
   size_t bytesRead;
   while (millis() - calibStartTime < CALIBRATION_DURATION_MS) {
      esp_err_t result = i2s_read(I2S_PORT, i2s_read_buffer, sizeof(i2s_read_buffer), &bytesRead, pdMS_TO_TICKS(100));
      if (result == ESP_OK && bytesRead > 0) {
         int samplesRead = bytesRead / sizeof(int32_t);
         long long sumSq = 0;
         for (int i = 0; i < samplesRead; i++) { long long sampleVal = (long long)i2s_read_buffer[i]; sumSq += sampleVal * sampleVal; }
         if (samplesRead > 0) { totalRmsSum += sqrt((double)sumSq / (double)samplesRead); rmsCount++; }
      } else if (result != ESP_ERR_TIMEOUT) { Serial.printf("!!! Calib I2S read error: %d\n", result); }
      delay(1);
   }
   if (rmsCount > 0) {
      ambientRmsLevel = totalRmsSum / rmsCount;
      Serial.printf("Calibration complete. Ambient RMS: %.2f (%ld readings)\n", ambientRmsLevel, rmsCount);
      return true;
   } else {
      Serial.println("!!! Calibration failed: No valid RMS readings.");
      ambientRmsLevel = 1000.0; // Fallback
      return false;
   }
}


// --- Core Functionality ---

void readMicrophone() {
  size_t bytes_read = 0;
  esp_err_t result = i2s_read(I2S_PORT, i2s_read_buffer, sizeof(i2s_read_buffer), &bytes_read, 0); // Non-blocking
  if (result == ESP_OK && bytes_read > 0) {
    int samples_read = bytes_read / sizeof(int32_t);
    long long sum_sq = 0;
    for (int i = 0; i < samples_read; i++) { long long sample_val = (long long)i2s_read_buffer[i]; sum_sq += sample_val * sample_val; }
    if (samples_read > 0) { currentRms = sqrt((double)sum_sq / (double)samples_read); }
  } else if (result != ESP_OK && result != ESP_ERR_TIMEOUT) { /* Log errors? */ }
}

// Updated readButtons function with single-shot long press trigger
void readButtons() {
  unsigned long now = millis();
  // Reset transient press flags
  button1Pressed = false;
  button2Pressed = false;
  button1LongPressTriggered = false; // Reset trigger flag each loop
  button2LongPressTriggered = false; // Reset trigger flag each loop

  // --- Button 1 ---
  bool currentButton1State = digitalRead(BUTTON1_PIN);
  if (currentButton1State == HIGH && lastButton1State == LOW) { // Rising edge (Press started)
    if (now - lastButton1PressTime > BUTTON_DEBOUNCE_MS) {
      button1PressStartTime = now; // Start timing for long press
      button1Held = true;
      button1LongPressProcessed = false; // Reset processed flag for new press
      // Serial.println("B1 Press Start");
    }
  } else if (currentButton1State == LOW && lastButton1State == HIGH) { // Falling edge (Released)
     if (button1Held) { // Only register press if held flag was set
        if (!button1LongPressProcessed) { // Only trigger short press if long press wasn't processed
            button1Pressed = true; // Register SHORT press on release
            Serial.println("Button 1 Short Press Event");
        }
        lastButton1PressTime = now; // Debounce for next press start
        button1Held = false;
        button1LongPressProcessed = false; // Reset processed flag on release
        // Serial.println("B1 Released");
     }
  }
  // Check for LONG press while held down
  if (button1Held && !button1LongPressProcessed && (now - button1PressStartTime >= LONG_PRESS_DURATION_MS)) {
      button1LongPressTriggered = true; // Set the single-shot trigger flag
      button1LongPressProcessed = true; // Mark as processed for this hold duration
      Serial.println("Button 1 Long Press Triggered");
      // Note: button1Held remains true until release
  }
  lastButton1State = currentButton1State;

  // --- Button 2 --- (Identical logic to Button 1)
  bool currentButton2State = digitalRead(BUTTON2_PIN);
  if (currentButton2State == HIGH && lastButton2State == LOW) { // Rising edge
    if (now - lastButton2PressTime > BUTTON_DEBOUNCE_MS) {
      button2PressStartTime = now;
      button2Held = true;
      button2LongPressProcessed = false;
      // Serial.println("B2 Press Start");
    }
  } else if (currentButton2State == LOW && lastButton2State == HIGH) { // Falling edge
     if (button2Held) {
        if (!button2LongPressProcessed) {
            button2Pressed = true;
            Serial.println("Button 2 Short Press Event");
        }
        lastButton2PressTime = now;
        button2Held = false;
        button2LongPressProcessed = false;
        // Serial.println("B2 Released");
     }
  }
  if (button2Held && !button2LongPressProcessed && (now - button2PressStartTime >= LONG_PRESS_DURATION_MS)) {
       button2LongPressTriggered = true;
       button2LongPressProcessed = true;
       Serial.println("Button 2 Long Press Triggered");
  }
  lastButton2State = currentButton2State;
}


void updateDisplay() {
  static TimerState lastDisplayedState = (TimerState)-1;
  static int lastDisplayedShotCount = -1;
  static float lastDisplayedFirstTime = -1.0;
  static float lastDisplayedSplitTime = -1.0;
  static int lastDisplayedSettingIndex = -1; // For settings screen
  static uint8_t lastDispMaxShots = 0;
  static uint16_t lastDispBeepDur = 0;
  static uint16_t lastDispBeepTone = 0;


  if (currentState == CALIBRATION_ERROR) {
      if (lastDisplayedState != CALIBRATION_ERROR) {
          display.clearDisplay(); display.setTextSize(2); display.setTextColor(SSD1306_WHITE);
          display.setCursor(10, 10); display.println("CALIB"); display.setCursor(10, 35); display.println("FAILED!");
          display.display(); lastDisplayedState = CALIBRATION_ERROR;
      } return;
  }

  bool needsUpdate = false;
  if (currentState != lastDisplayedState) needsUpdate = true;
  else if (currentState == DISPLAY_RESULT && (shotCount != lastDisplayedShotCount || abs(firstShotTime - lastDisplayedFirstTime) > 0.001 || abs(lastSplitTime - lastDisplayedSplitTime) > 0.001)) needsUpdate = true;
  else if (currentState == TIMING && shotCount != lastDisplayedShotCount) needsUpdate = true;
  else if (currentState == SETTINGS && (currentSettingIndex != lastDisplayedSettingIndex || maxShotsSetting != lastDispMaxShots || beepDurationSetting != lastDispBeepDur || beepToneSetting != lastDispBeepTone )) needsUpdate = true;
  else if (currentState == WAITING) { static unsigned long lu=0; if(millis()-lu > 250){ needsUpdate=true; lu=millis(); }}

  if (needsUpdate) {
      display.clearDisplay(); display.setTextColor(SSD1306_WHITE);

      switch (currentState) {
        case IDLE:
          display.setTextSize(2); display.setCursor(40, 10); display.println("READY");
          display.setTextSize(1); display.setCursor(25, 35); display.println("Black - Start");
          display.setCursor(10, 50); display.println("Hold Green - Setup");
          break;
        case READY:
          display.setTextSize(2); display.setCursor(0, 10); display.println("Ready!");
          break;
        case WAITING:
          display.setTextSize(2); display.setCursor(10, 0); display.println("Shooter");display.println(" Ready!");
          display.setTextSize(1); display.setCursor(0, 55); display.printf("Delay: %lu ms", randomDelayDuration);
          break;
        case TIMING:
          display.setTextSize(2); display.setCursor(15, 10); display.println("Shoot!");
          display.setTextSize(1); display.setCursor(0, 35); display.printf("Shots: %d/%d", shotCount, currentMaxShots);
          display.setCursor(0, 48);
           if (shotCount > 0) { float dt = (shotCount == 1) ? firstShotTime : lastSplitTime; display.printf("Last: %.3fs", dt); }
           else { display.printf("T: %.3fs", (float)(micros() - beepStartTime) / 1000000.0); }
          break;
        case DISPLAY_RESULT:
          if (shotCount > 0) {
              display.setTextSize(2); display.setCursor(0, 5); display.printf("1st:%.3f", firstShotTime);
              display.setTextSize(1); display.setCursor(0, 30);
              if (shotCount > 1) display.printf("Split %d: %.3fs", shotCount, lastSplitTime);
              display.setCursor(0, 42); display.printf("Total Shots: %d", shotCount);
              playSuccessBeeps();
          } else {
              display.setTextSize(2); display.setCursor(5, 10); display.println("No Shots");
              display.setTextSize(1); display.setCursor(0, 35); display.print("Timer Stopped");
              playUnsuccessBeeps();
          }
           display.setTextSize(1); display.setCursor(10, 55); display.print("Black - Reset");
          lastDisplayedFirstTime = firstShotTime; lastDisplayedSplitTime = lastSplitTime; lastDisplayedShotCount = shotCount;
          break;
        case SETTINGS:
            display.setTextSize(1); display.setCursor(35, 0); display.println("Settings");
            display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

            // Display Setting Name
            display.setCursor(0, 15);
            display.print(">"); // Indicator for current setting
            switch(currentSettingIndex) {
                case 0: display.print("Max Shots:"); break;
                case 1: display.print("Beep MS:"); break;
                case 2: display.print("Beep Hz:"); break;
            }

            // Display Setting Value
            display.setTextSize(2); // Larger text for value
            display.setCursor(30, 30);
             switch(currentSettingIndex) {
                case 0: display.print(maxShotsSetting); break;
                case 1: display.print(beepDurationSetting); break;
                case 2: display.print(beepToneSetting); break;
            }

            display.setTextSize(1); display.setCursor(0, 55); display.print("B1/B2:Adj|Hold:Cycle");

            // Store displayed values for change detection
            lastDisplayedSettingIndex = currentSettingIndex;
            lastDispMaxShots = maxShotsSetting;
            lastDispBeepDur = beepDurationSetting;
            lastDispBeepTone = beepToneSetting;
            break;
      }
      display.display();
      lastDisplayedState = currentState;
  }
}

// --- Success Beep Function ---
void playSuccessBeeps() {
    Serial.println("Playing success beeps...");
    // Define frequencies for the 3 beeps
    int freq1 = 147*2;
    int freq2 = 165*2;
    int freq3 = 131*2;
    int freq4 = 65*2;
    int freq5 = 196*2;

    tone(BUZZER_PIN_1, freq1, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SUCCESS_BEEP_DELAY); // Wait for beep + delay

    tone(BUZZER_PIN_1, freq2, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SUCCESS_BEEP_DELAY); // Wait for beep + delay

    tone(BUZZER_PIN_1, freq3, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SUCCESS_BEEP_DELAY); // Wait for beep + delay
    // noTone(BUZZER_PIN_1); // Ensure tone is off (though timed tone should handle it)
    tone(BUZZER_PIN_1, freq4, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SUCCESS_BEEP_DELAY); // Wait for beep + delay
    // noTone(BUZZER_PIN_1); // Ensure tone is off (though timed tone should handle it)
    tone(BUZZER_PIN_1, freq5, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SUCCESS_BEEP_DELAY); // Wait for beep + delay
    // noTone(BUZZER_PIN_1); // Ensure tone is off (though timed tone should handle it)
}


void playUnsuccessBeeps() {
    Serial.println("Playing success beeps...");
    // Define frequencies for the 3 beeps
    int freq1 = 65;
    int freq2 = 65;
    int freq3 = 65;
    int freq4 = 65;
    int freq5 = 65;
    int SlowDelay = SUCCESS_BEEP_DELAY * 2 ;
    tone(BUZZER_PIN_1, freq1, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SlowDelay); // Wait for beep + delay

    tone(BUZZER_PIN_1, freq2, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SlowDelay); // Wait for beep + delay

    tone(BUZZER_PIN_1, freq3, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SlowDelay); // Wait for beep + delay
    // noTone(BUZZER_PIN_1); // Ensure tone is off (though timed tone should handle it)
    tone(BUZZER_PIN_1, freq4, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SlowDelay); // Wait for beep + delay
    // noTone(BUZZER_PIN_1); // Ensure tone is off (though timed tone should handle it)
    tone(BUZZER_PIN_1, freq5, SUCCESS_BEEP_DURATION);
    delay(SUCCESS_BEEP_DURATION + SlowDelay); // Wait for beep + delay
    // noTone(BUZZER_PIN_1); // Ensure tone is off (though timed tone should handle it)
}


void startBeep() {
  if (!isBeeping) {
      Serial.printf("BEEP START (Tone: %d Hz, Duration: %d ms)\n", currentBeepToneHz, currentBeepDurationMs);
      // Use tone with loaded/set frequency and duration
      tone(BUZZER_PIN_1, currentBeepToneHz, currentBeepDurationMs);
      isBeeping = true;
      // Note: The tone() function with duration handles turning itself off.
      // We still use stopBeep() in case the state changes before the duration ends.
  }
}

void stopBeep() {
   // Only call noTone if the beep was started using tone() without duration,
   // or if we need to interrupt a timed tone.
   if (isBeeping) {
       Serial.println("BEEP STOP (Called)");
       noTone(BUZZER_PIN_1); // Ensure tone is stopped regardless of timed completion
       isBeeping = false;
   }
}
