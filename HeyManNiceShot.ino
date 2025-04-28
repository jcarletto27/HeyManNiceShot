#include <Arduino.h>
#include "driver/i2s.h"
#include <math.h>       // For sqrt, log10
#include <Wire.h>       // For I2C
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <esp_random.h> // For hardware RNG
#include <Preferences.h> // For Non-Volatile Storage
#include "espTone2x.h"     // Include the espTone library header

// --- Pin Definitions (Using XIAO ESP32C3 D# Aliases) ---
#define BUTTON1_PIN   D0  // D0 - Start / Reset / Settings Up / Next Setting Button (Black)
#define BUTTON2_PIN   D1  // D1 - Stop Timing / Settings Down / Prev Setting / Enter Settings Button (Green)
// Define pins for espTone
const int TONE_PIN_1 = D6; // D6 - Buzzer Pin 1 (GPIO_NUM_7)
const int TONE_PIN_1_2 = D7; 
const int TONE_PIN_2 = D3; // D3 - Buzzer Pin 2 (GPIO_NUM_4)
const int TONE_PIN_2_2 = D2; 

#define I2C_SDA_PIN   D4  // D4 - I2C Data (GPIO_NUM_5)
#define I2C_SCL_PIN   D5  // D5 - I2C Clock (GPIO_NUM_6)

#define I2S_WS_PIN    D9  // D9 - Word Select (LRCL) (GPIO_NUM_9)
#define I2S_SCK_PIN   D8  // D8 - Serial Clock (BCLK) (GPIO_NUM_8)
#define I2S_SD_PIN    D10 // D10 - Serial Data (DIN) (GPIO_NUM_10)

// --- Library Instance ---
// Create an instance of the espTone library
espTone2x myToneGenerator(TONE_PIN_1, TONE_PIN_1_2, TONE_PIN_2, TONE_PIN_2_2);

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
#define AMBIENT_CALIBRATION_DURATION_MS 5000 // Ambient noise calibration time
#define SHOT_LOCKOUT_MICROS 500000 // 500ms lockout after a shot detection
#define ABSOLUTE_MAX_SHOTS 20     // Max size of the timestamp array
#define RANDOM_DELAY_MIN_MS 1000
#define RANDOM_DELAY_MAX_MS 3000
#define BUTTON_DEBOUNCE_MS 50
#define LONG_PRESS_DURATION_MS 1000 // Duration for long press detection
#define SETTINGS_TIMEOUT_MS 5000  // Timeout to save settings
#define SETTINGS_CYCLE_DELAY_MS 750 // Delay between auto-cycles when button held (milliseconds)
#define TIMING_TIMEOUT_MS 30000   // 30 second timeout for TIMING state
#define SUCCESS_BEEP_DURATION 50  // Duration for each success beep
#define SUCCESS_BEEP_DELAY 100    // Delay between success beeps
#define SHOT_THRESHOLD_CALIBRATION_DURATION_MS 3000 // Duration to listen for peak RMS
#define MINIMUM_CALIBRATED_THRESHOLD_FACTOR 2.0 // New threshold must be at least this factor times ambient
#define DEFAULT_SHOT_THRESHOLD_RMS 27589306.85 // Default if NVS fails or first boot
#define SHOT_THRESHOLD_RMS_NVS_KEY "shot_thresh" // NVS Key for shot threshold
#define RMS_TO_DB_REFERENCE 117435.6 // Reference RMS value derived from user point (3888603469.0 RMS = 90.4 dB)
// #define TONEAC_VOLUME 10 // REMOVED - espTone doesn't use volume param

// --- Settings Limits ---
#define MAX_SHOTS_MIN 1
#define MAX_SHOTS_MAX ABSOLUTE_MAX_SHOTS
#define BEEP_DURATION_MIN_MS 50
#define BEEP_DURATION_MAX_MS 2000
#define BEEP_TONE_MIN_HZ 30
#define BEEP_TONE_MAX_HZ 4000 // espTone might have different practical limits
#define BEEP_TONE_STEP 100
#define SHOT_THRESHOLD_MIN 100000.0 // Absolute minimum allowed threshold (RMS)
#define SHOT_THRESHOLD_MAX 5000000000.0 // Absolute maximum allowed threshold (RMS)


// --- Global Variables ---
Preferences preferences; // NVS object
double ambientRmsLevel = 0.0;
double currentRms = 0.0;

// --- Runtime Settings (Loaded from NVS) ---
uint8_t currentMaxShots = 10;
uint16_t currentBeepDurationMs = 400;
uint16_t currentBeepToneHz = 2000;
double shotThresholdRms = DEFAULT_SHOT_THRESHOLD_RMS; // Default value, will be overwritten by NVS load

// State Machine
enum TimerState { IDLE, READY, WAITING, TIMING, DISPLAY_RESULT, SETTINGS, CALIBRATING_THRESHOLD, CALIBRATION_ERROR };
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
bool button1Pressed = false;
bool button2Pressed = false;
bool button1LongPressTriggered = false;
bool button2LongPressTriggered = false;
unsigned long lastButton1PressTime = 0;
unsigned long lastButton2PressTime = 0;
bool lastButton1State = LOW;
bool lastButton2State = LOW;
unsigned long button1PressStartTime = 0;
unsigned long button2PressStartTime = 0;
bool button1Held = false;
bool button2Held = false;
bool button1LongPressProcessed = false;
bool button2LongPressProcessed = false;


// Settings State Variables
int currentSettingIndex = 0; // 0: Max Shots, 1: Beep Duration, 2: Beep Tone
uint8_t maxShotsSetting = 10;
uint16_t beepDurationSetting = 400;
uint16_t beepToneSetting = 2000;
unsigned long lastSettingsInteractionTime = 0;
unsigned long nextCycleTime = 0;
bool cycleDelayActive = false;

// Threshold Calibration Variables
double peakRmsDuringCalibration = 0.0;
unsigned long thresholdCalibrationStartTime = 0;
bool thresholdCalibrationSuccess = false;

// Buzzer State
bool isBeeping = false; // Flag to indicate if *our logic* thinks a beep should be playing
unsigned long beepStopTime = 0; // Time when the current beep should stop


// --- Function Prototypes ---
void setupI2S();
void setupDisplay();
void setupPins();
bool calibrateAmbientNoise();
void readMicrophone();
void readButtons();
void updateDisplay();
void startBeep();
void stopBeep();
void changeState(TimerState newState);
void resetShotData();
void loadSettings();
void saveSettings(); // Saves regular settings
void saveThresholdSetting(); // Saves only the threshold
void playSuccessBeeps();
void playUnsuccessBeeps();
double rmsToDb(double rms); // Function to convert RMS to dB

// --- Setup Function ---
void setup() {
  Serial.begin(115200);
  Serial.println("ESP32C3 Shot Timer Starting...");

  loadSettings(); // Load saved settings from NVS (includes shot threshold)
  setupPins();    // Setup button pins ONLY
  myToneGenerator.begin(); // Initialize espTone library
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
  Serial.println("Displaying boot screen, starting ambient calibration...");

  // --- Calibrate Ambient Noise ---
  bool ambientCalibrationSuccess = calibrateAmbientNoise();

  // --- Handle Calibration Result ---
  if (!ambientCalibrationSuccess) {
    Serial.println("!!! AMBIENT NOISE CALIBRATION FAILED !!!");
    display.clearDisplay();
    display.setTextSize(2); display.setTextColor(SSD1306_WHITE);
    display.setCursor(10, 10); display.println("AMBIENT");
    display.setCursor(10, 35); display.println("FAILED!");
    display.display();
    changeState(CALIBRATION_ERROR); // Go to error state
    while (true) { delay(100); } // Halt on ambient calibration failure
  }

  // --- Calibration Succeeded ---
  Serial.println("Ambient Calibration Successful.");
  playSuccessBeeps(); // Play the success beep sequence

  // --- Show Ready Screen ---
  Serial.println("Entering IDLE state.");
  resetShotData();
  changeState(IDLE); // Set initial state and display ready screen via updateDisplay()
}

// --- Main Loop ---
void loop() {
    if (currentState == CALIBRATION_ERROR) { delay(100); return; } // Halt on error

  readButtons();
  readMicrophone(); // Continuously read microphone

  unsigned long now = millis();
  unsigned long now_micros = micros();

  // Check if a timed beep needs to be stopped (since espTone::tone is continuous)
  if (isBeeping && now >= beepStopTime) {
      stopBeep();
  }

  // --- State Machine Logic ---
  switch (currentState) {
    case IDLE:
      // Check for Button 1 SHORT press to start timing sequence
      if (button1Pressed) { changeState(READY); }

      // Check for Button 1 LONG press to start threshold calibration
      if (button1LongPressTriggered) {
          Serial.println("Button 1 Long Press Detected in IDLE - Entering Threshold Calibration");
          changeState(CALIBRATING_THRESHOLD);
      }

      // Check for Button 2 LONG press to enter settings
      if (button2LongPressTriggered) {
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

    case CALIBRATING_THRESHOLD:
      // Keep track of the peak RMS value during the calibration window
      if (currentRms > peakRmsDuringCalibration) {
          peakRmsDuringCalibration = currentRms;
      }

      // Check if the calibration duration has passed
      if (now - thresholdCalibrationStartTime >= SHOT_THRESHOLD_CALIBRATION_DURATION_MS) {
          Serial.printf("Threshold calibration finished. Peak RMS detected: %.2f (%.1f dB)\n", peakRmsDuringCalibration, rmsToDb(peakRmsDuringCalibration));

          // Validate the captured peak (using RMS values)
          if (peakRmsDuringCalibration > (ambientRmsLevel * MINIMUM_CALIBRATED_THRESHOLD_FACTOR) && peakRmsDuringCalibration > SHOT_THRESHOLD_MIN) {
              shotThresholdRms = peakRmsDuringCalibration; // Set the new threshold (still RMS)
              // Apply bounds check (RMS)
              if (shotThresholdRms < SHOT_THRESHOLD_MIN) shotThresholdRms = SHOT_THRESHOLD_MIN;
              if (shotThresholdRms > SHOT_THRESHOLD_MAX) shotThresholdRms = SHOT_THRESHOLD_MAX;

              Serial.printf("New shot threshold set to: %.2f RMS (%.1f dB)\n", shotThresholdRms, rmsToDb(shotThresholdRms));
              saveThresholdSetting(); // Save ONLY the new threshold (RMS) to NVS
              thresholdCalibrationSuccess = true; // Flag success for display/sound
              playSuccessBeeps();
          } else {
              Serial.println("!!! Threshold calibration failed: Peak RMS too low or indistinct.");
              thresholdCalibrationSuccess = false; // Flag failure
              playUnsuccessBeeps();
              // Keep the old threshold value
          }
          changeState(IDLE); // Return to IDLE state
      }

      // Allow cancellation with Button 2 short press
      if (button2Pressed) {
          Serial.println("Threshold calibration cancelled by user.");
          thresholdCalibrationSuccess = false; // Ensure failure indication if cancelled early
          changeState(IDLE);
      }
      break;

    case READY:
      // Immediately transition to WAITING after setting up delay
      randomDelayDuration = esp_random() % (RANDOM_DELAY_MAX_MS - RANDOM_DELAY_MIN_MS + 1) + RANDOM_DELAY_MIN_MS;
      Serial.printf("Random delay set to: %lu ms\n", randomDelayDuration);
      changeState(WAITING);
      break;

    case WAITING:
      if (now - stateEntryTime >= randomDelayDuration) { changeState(TIMING); }
      // Allow cancellation with Button 1 short press
      if (button1Pressed) { Serial.println("Start cancelled by user."); changeState(IDLE); }
      break;

    case TIMING:
      // Check for shot detection using the variable threshold (RMS)
      if (currentRms > shotThresholdRms && (now_micros - lastShotDetectTime > SHOT_LOCKOUT_MICROS)) {
        if (shotCount < currentMaxShots) {
          unsigned long currentShotTime = now_micros;
          shotTimestamps[shotCount] = currentShotTime;
          lastShotDetectTime = currentShotTime;

          if (shotCount == 0) {
            firstShotTime = (float)(currentShotTime - beepStartTime) / 1000000.0;
            lastSplitTime = firstShotTime;
            // Log RMS and threshold in RMS for clarity during detection
            Serial.printf("First Shot Detected! Time: %.3f s (RMS: %.2f, Threshold: %.2f)\n", firstShotTime, currentRms, shotThresholdRms);
          } else {
            lastSplitTime = (float)(currentShotTime - shotTimestamps[shotCount - 1]) / 1000000.0;
            Serial.printf("Split %d Detected! Time: %.3f s (RMS: %.2f, Threshold: %.2f)\n", shotCount + 1, lastSplitTime, currentRms, shotThresholdRms);
          }
          shotCount++;
          if (shotCount >= currentMaxShots) {
              Serial.println("Max shots reached, stopping timer.");
              stopBeep(); // Explicitly stop beep sound if max shots reached
              changeState(DISPLAY_RESULT); break;
          }
        } else { Serial.println("Max shots already recorded."); }
      }

      // Check for stop button (Button 2 short press)
      if (currentState == TIMING && button2Pressed) {
          Serial.println("Timing stopped by user (Button 2).");
          stopBeep(); // Stop beep sound
          changeState(DISPLAY_RESULT); break;
      }

      // Check for TIMING Timeout
      if (currentState == TIMING && beepStartTime > 0 && (now - (beepStartTime / 1000) > TIMING_TIMEOUT_MS)) {
          Serial.println("Timeout - Stopping timer.");
          stopBeep(); // Stop beep sound
          changeState(DISPLAY_RESULT); break;
      }
      break; // End of TIMING case

    case DISPLAY_RESULT:
      // Reset with Button 1 short press
      if (button1Pressed) { changeState(IDLE); }
      break;

    case SETTINGS:
      bool interactionOccurred = false;

      // --- Handle Value Adjustment (Short Presses) ---
      if (button1Pressed) { // Increase / Next
          switch (currentSettingIndex) {
              case 0: if (maxShotsSetting < MAX_SHOTS_MAX) maxShotsSetting++; break;
              case 1: if (beepDurationSetting < BEEP_DURATION_MAX_MS) beepDurationSetting += 50; if (beepDurationSetting > BEEP_DURATION_MAX_MS) beepDurationSetting = BEEP_DURATION_MAX_MS; break;
              case 2: if (beepToneSetting < BEEP_TONE_MAX_HZ) beepToneSetting += BEEP_TONE_STEP; if (beepToneSetting > BEEP_TONE_MAX_HZ) beepToneSetting = BEEP_TONE_MAX_HZ; break;
          }
          interactionOccurred = true; cycleDelayActive = false;
      }
      if (button2Pressed) { // Decrease / Previous
          switch (currentSettingIndex) {
              case 0: if (maxShotsSetting > MAX_SHOTS_MIN) maxShotsSetting--; break;
              case 1: if (beepDurationSetting > BEEP_DURATION_MIN_MS) beepDurationSetting -= 50; if (beepDurationSetting < BEEP_DURATION_MIN_MS) beepDurationSetting = BEEP_DURATION_MIN_MS; break;
              case 2: if (beepToneSetting > BEEP_TONE_MIN_HZ) beepToneSetting -= BEEP_TONE_STEP; if (beepToneSetting < BEEP_TONE_MIN_HZ) beepToneSetting = BEEP_TONE_MIN_HZ; break;
          }
          interactionOccurred = true; cycleDelayActive = false;
      }

      // --- Handle Setting Cycling (Long Presses - Initial Trigger) ---
      if (button1LongPressTriggered) { // Cycle Next Setting
          currentSettingIndex = (currentSettingIndex + 1) % 3;
          interactionOccurred = true;
          cycleDelayActive = true;
          nextCycleTime = now + SETTINGS_CYCLE_DELAY_MS;
      }
      if (button2LongPressTriggered) { // Cycle Previous Setting
          currentSettingIndex = (currentSettingIndex + 3 - 1) % 3;
          interactionOccurred = true;
          cycleDelayActive = true;
          nextCycleTime = now + SETTINGS_CYCLE_DELAY_MS;
      }

      // --- Handle Setting Cycling (Auto-Repeat while Held) ---
      if (cycleDelayActive && now >= nextCycleTime) {
          if (button1Held) { // Next
              currentSettingIndex = (currentSettingIndex + 1) % 3;
              interactionOccurred = true;
              nextCycleTime = now + SETTINGS_CYCLE_DELAY_MS;
          } else if (button2Held) { // Previous
              currentSettingIndex = (currentSettingIndex + 3 - 1) % 3;
              interactionOccurred = true;
              nextCycleTime = now + SETTINGS_CYCLE_DELAY_MS;
          } else {
              cycleDelayActive = false; // Stop auto-cycle if button released
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
          saveSettings(); // Save all temporary settings (max shots, beep) to NVS
          // Update runtime variables from temporary settings
          currentMaxShots = maxShotsSetting;
          currentBeepDurationMs = beepDurationSetting;
          currentBeepToneHz = beepToneSetting;
          playSuccessBeeps(); // Indicate save
          changeState(IDLE);
      }
      break; // End of SETTINGS case

    // CALIBRATION_ERROR state is handled at the top of loop()
  } // End of switch(currentState)

  updateDisplay();
  delay(5); // Small delay for stability
} // End of loop()

// --- State Change Helper ---
void changeState(TimerState newState) {
  // Prevent exiting CALIBRATION_ERROR state except maybe to IDLE if needed later
  if (currentState == CALIBRATION_ERROR && newState != IDLE) { return; }

  Serial.printf("Changing state from %d to: %d\n", currentState, newState);
  TimerState previousState = currentState;

  // --- Actions on Exiting Current State ---
  if (previousState == TIMING && newState != TIMING) {
      stopBeep(); // Ensure beep is stopped when leaving TIMING state
  }

  currentState = newState;
  stateEntryTime = millis(); // Record time of entering the new state (using millis)

  // --- Actions on Entering New State ---
  if (newState == TIMING) {
    beepStartTime = micros(); // Record precise start time for timing (using micros)
    lastShotDetectTime = beepStartTime; // Initialize lockout relative to beep start
    startBeep();
  }

  if (newState == IDLE) {
      resetShotData();
      // Display update will handle showing the IDLE screen
  }

  if (newState == CALIBRATING_THRESHOLD) {
      peakRmsDuringCalibration = 0.0; // Reset peak detector
      thresholdCalibrationStartTime = stateEntryTime; // Start the timer
      thresholdCalibrationSuccess = false; // Reset success flag
      // Display update will show "Listening..."
  }

  if (newState == DISPLAY_RESULT && previousState == TIMING) { // Only play sounds when coming from TIMING
      // Play sounds based on whether shots were recorded
      if (shotCount > 0) {
          playSuccessBeeps();
      } else {
          playUnsuccessBeeps();
      }
  }


  // Reset button states completely on state change to avoid carry-over issues
  button1Pressed = false; button1LongPressTriggered = false;
  button2Pressed = false; button2LongPressTriggered = false;
  button1Held = false; button1PressStartTime = 0; button1LongPressProcessed = false;
  button2Held = false; button2PressStartTime = 0; button2LongPressProcessed = false;
  cycleDelayActive = false; nextCycleTime = 0; // Reset settings cycle state too
}

// --- Reset Shot Data ---
void resetShotData() {
    shotCount = 0;
    firstShotTime = 0.0;
    lastSplitTime = 0.0;
    beepStartTime = 0;
    lastShotDetectTime = 0;
    beepStopTime = 0; // Reset beep stop time
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
    // Load shot threshold (RMS)
    shotThresholdRms = preferences.getDouble(SHOT_THRESHOLD_RMS_NVS_KEY, DEFAULT_SHOT_THRESHOLD_RMS);

    preferences.end();

    // --- Apply Bounds (RMS) ---
    if (currentMaxShots < MAX_SHOTS_MIN) currentMaxShots = MAX_SHOTS_MIN;
    if (currentMaxShots > MAX_SHOTS_MAX) currentMaxShots = MAX_SHOTS_MAX;
    if (currentBeepDurationMs < BEEP_DURATION_MIN_MS) currentBeepDurationMs = BEEP_DURATION_MIN_MS;
    if (currentBeepDurationMs > BEEP_DURATION_MAX_MS) currentBeepDurationMs = BEEP_DURATION_MAX_MS;
    if (currentBeepToneHz < BEEP_TONE_MIN_HZ) currentBeepToneHz = BEEP_TONE_MIN_HZ;
    if (currentBeepToneHz > BEEP_TONE_MAX_HZ) currentBeepToneHz = BEEP_TONE_MAX_HZ;
    // Apply bounds to loaded shot threshold (RMS)
    if (shotThresholdRms < SHOT_THRESHOLD_MIN) shotThresholdRms = SHOT_THRESHOLD_MIN;
    if (shotThresholdRms > SHOT_THRESHOLD_MAX) shotThresholdRms = SHOT_THRESHOLD_MAX;


    Serial.printf("Loaded Settings - Max Shots: %d, Beep Duration: %d ms, Beep Tone: %d Hz, Shot Threshold: %.2f RMS (%.1f dB)\n",
                  currentMaxShots, currentBeepDurationMs, currentBeepToneHz, shotThresholdRms, rmsToDb(shotThresholdRms));
}

// Saves regular settings (called from SETTINGS state timeout)
void saveSettings() {
    preferences.begin("shotTimer", false); // Start NVS, read/write
    preferences.putUChar("max_shots", maxShotsSetting);
    preferences.putUShort("beep_dur", beepDurationSetting);
    preferences.putUShort("beep_tone", beepToneSetting);
    // DO NOT save shotThresholdRms here, it's saved separately by saveThresholdSetting()
    preferences.end();
    Serial.printf("Saved Settings - Max Shots: %d, Beep Duration: %d ms, Beep Tone: %d Hz\n",
                  maxShotsSetting, beepDurationSetting, beepToneSetting);
}

// Saves ONLY the shot threshold setting (RMS value)
void saveThresholdSetting() {
    preferences.begin("shotTimer", false); // Start NVS, read/write
    preferences.putDouble(SHOT_THRESHOLD_RMS_NVS_KEY, shotThresholdRms);
    preferences.end();
    Serial.printf("Saved Setting - Shot Threshold: %.2f RMS (%.1f dB)\n", shotThresholdRms, rmsToDb(shotThresholdRms));
}


// --- Hardware Setup Functions ---
void setupPins() {
  pinMode(BUTTON1_PIN, INPUT_PULLDOWN);
  pinMode(BUTTON2_PIN, INPUT_PULLDOWN);
  // Buzzer pins are configured by espTone library via myToneGenerator.begin()
}

void setupDisplay() {
  Wire.setPins(I2C_SDA_PIN, I2C_SCL_PIN);
  delay(100);
  if(!display.begin(SSD1306_SWITCHCAPVCC, SCREEN_ADDRESS)) {
    Serial.println(F("SSD1306 allocation failed"));
    while(true) delay(1000); // Halt if display fails
  }
  display.clearDisplay();
  display.display();
  Serial.println("SSD1306 Display Initialized.");
}

void setupI2S() {
  Serial.println("Configuring I2S...");
  i2s_config_t i2s_config = {
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
  i2s_pin_config_t pin_config = {
    .bck_io_num = I2S_SCK_PIN,       // D8
    .ws_io_num = I2S_WS_PIN,        // D9
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_SD_PIN        // D10
  };

  esp_err_t err = i2s_driver_install(I2S_PORT, &i2s_config, 0, NULL);
  if (err != ESP_OK) { Serial.printf("!!! Failed installing I2S driver: %d\n", err); while (true); } // Halt
  err = i2s_set_pin(I2S_PORT, &pin_config);
  if (err != ESP_OK) { Serial.printf("!!! Failed setting I2S pins: %d\n", err); i2s_driver_uninstall(I2S_PORT); while (true); } // Halt
  Serial.println("I2S Driver installed.");
  delay(100);
}

// --- Ambient Noise Calibration ---
bool calibrateAmbientNoise() {
   unsigned long calibStartTime = millis();
   double totalRmsSum = 0.0;
   long rmsCount = 0;
   size_t bytesRead;
   Serial.println("Starting ambient noise calibration...");
   while (millis() - calibStartTime < AMBIENT_CALIBRATION_DURATION_MS) {
      esp_err_t result = i2s_read(I2S_PORT, i2s_read_buffer, sizeof(i2s_read_buffer), &bytesRead, pdMS_TO_TICKS(100));
      if (result == ESP_OK && bytesRead > 0) {
         int samplesRead = bytesRead / sizeof(int32_t);
         long long sumSq = 0;
         for (int i = 0; i < samplesRead; i++) {
             long long sampleValLL = (long long)i2s_read_buffer[i];
             sumSq += sampleValLL * sampleValLL;
         }
         if (samplesRead > 0) {
             double bufferRms = sqrt((double)sumSq / (double)samplesRead);
             totalRmsSum += bufferRms;
             rmsCount++;
         }
      } else if (result != ESP_ERR_TIMEOUT) {
          Serial.printf("!!! Ambient Calib I2S read error: %d\n", result);
      }
      delay(1); // Yield
   }

   if (rmsCount > 0) {
      ambientRmsLevel = totalRmsSum / rmsCount;
      Serial.printf("Ambient calibration complete. Average Ambient RMS: %.2f (%.1f dB) (%ld readings)\n",
                    ambientRmsLevel, rmsToDb(ambientRmsLevel), rmsCount);
      if (ambientRmsLevel < 1.0) ambientRmsLevel = 1.0; // Prevent zero ambient level
      return true;
   } else {
      Serial.println("!!! Ambient calibration failed: No valid RMS readings obtained.");
      ambientRmsLevel = 10000.0; // Fallback
      return false;
   }
}


// --- Core Functionality ---

void readMicrophone() {
  size_t bytes_read = 0;
  esp_err_t result = i2s_read(I2S_PORT, i2s_read_buffer, sizeof(i2s_read_buffer), &bytes_read, pdMS_TO_TICKS(5));
  if (result == ESP_OK && bytes_read > 0) {
    int samples_read = bytes_read / sizeof(int32_t);
    long long sum_sq = 0;
    for (int i = 0; i < samples_read; i++) {
        long long sample_val = (long long)i2s_read_buffer[i];
        sum_sq += sample_val * sample_val;
    }
    if (samples_read > 0) {
        currentRms = sqrt((double)sum_sq / (double)samples_read);
    }
  } else if (result != ESP_OK && result != ESP_ERR_TIMEOUT) {
       Serial.printf("!!! I2S read error in loop: %d\n", result);
       currentRms = 0; // Reset RMS on error
  }
  // No action needed for ESP_ERR_TIMEOUT, just means buffer wasn't full
}

// readButtons function remains the same
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
     }
  }
  // Check for LONG press while held down
  if (button1Held && !button1LongPressProcessed && (now - button1PressStartTime >= LONG_PRESS_DURATION_MS)) {
      button1LongPressTriggered = true; // Set the single-shot trigger flag
      button1LongPressProcessed = true; // Mark as processed for this hold duration
      Serial.println("Button 1 Long Press Triggered");
  }
  lastButton1State = currentButton1State;

  // --- Button 2 --- (Identical logic to Button 1)
  bool currentButton2State = digitalRead(BUTTON2_PIN);
  if (currentButton2State == HIGH && lastButton2State == LOW) { // Rising edge
    if (now - lastButton2PressTime > BUTTON_DEBOUNCE_MS) {
      button2PressStartTime = now;
      button2Held = true;
      button2LongPressProcessed = false;
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
     }
  }
  if (button2Held && !button2LongPressProcessed && (now - button2PressStartTime >= LONG_PRESS_DURATION_MS)) {
       button2LongPressTriggered = true;
       button2LongPressProcessed = true;
       Serial.println("Button 2 Long Press Triggered");
  }
  lastButton2State = currentButton2State;
}

// --- RMS to dB Conversion ---
// Converts an RMS value to a dB value based on a reference RMS.
// Handles potential log10(<=0) issues.
double rmsToDb(double rms) {
    // Prevent log10 of non-positive numbers
    if (rms <= 0.0 || RMS_TO_DB_REFERENCE <= 0.0) {
        return -999.0; // Return a very low dB value indicating an issue or silence
    }
    // Calculate dB relative to the reference RMS value
    return 20.0 * log10(rms / RMS_TO_DB_REFERENCE);
}


// updateDisplay function - Refactored needsUpdate logic
void updateDisplay() {
  // Static variables to track last displayed values for change detection
  static TimerState lastDisplayedState = (TimerState)-1;
  static int lastDisplayedShotCount = -1;
  static float lastDisplayedFirstTime = -1.0;
  static float lastDisplayedSplitTime = -1.0;
  static int lastDisplayedSettingIndex = -1;
  static uint8_t lastDispMaxShots = 0;
  static uint16_t lastDispBeepDur = 0;
  static uint16_t lastDispBeepTone = 0;
  static double lastDispPeakRms = -1.0; // For threshold calibration screen (RMS)
  static double lastDispThresholdRms = -1.0; // To update threshold on IDLE screen (RMS)

  bool needsUpdate = false;

  // 1. Always update if the state changed
  if (currentState != lastDisplayedState) {
      needsUpdate = true;
  }
  // 2. If state hasn't changed, check for updates within specific states
  else {
      switch (currentState) {
          case IDLE:
              // Update if the underlying threshold RMS value changed
              if (abs(shotThresholdRms - lastDispThresholdRms) > 0.1) {
                  needsUpdate = true;
              }
              break;
          case TIMING:
              static unsigned long lastTimingUpdate = 0;
              // Update if shot count changed or periodically for live timer
              if (shotCount != lastDisplayedShotCount || millis() - lastTimingUpdate > 200) {
                  needsUpdate = true;
                  lastTimingUpdate = millis();
              }
              break;
          case DISPLAY_RESULT:
              // Result screen is mostly static once entered
              if (shotCount != lastDisplayedShotCount || abs(firstShotTime - lastDisplayedFirstTime) > 0.001 || abs(lastSplitTime - lastDisplayedSplitTime) > 0.001) {
                  needsUpdate = true;
              }
              break;
          case SETTINGS:
              // Update if settings values change
              if (currentSettingIndex != lastDisplayedSettingIndex || maxShotsSetting != lastDispMaxShots || beepDurationSetting != lastDispBeepDur || beepToneSetting != lastDispBeepTone) {
                  needsUpdate = true;
              }
              break;
          case CALIBRATING_THRESHOLD:
              static unsigned long lastCalibUpdate = 0;
              // Update periodically or if peak RMS changes significantly
              if (abs(peakRmsDuringCalibration - lastDispPeakRms) > 1.0 || millis() - lastCalibUpdate > 250) {
                  needsUpdate = true;
                  lastCalibUpdate = millis();
              }
              break;
          // No dynamic updates needed for WAITING, READY, CALIBRATION_ERROR
          default: break;
      }
  }


  if (needsUpdate) {
      display.clearDisplay();
      display.setTextColor(SSD1306_WHITE);
      display.setTextSize(1); // Default text size

      switch (currentState) {
        case IDLE:
          display.setTextSize(2); display.setCursor(40, 5); display.println("READY");
          display.setTextSize(1); display.setCursor(0, 25); display.print("B1:Start|Hold:Calib");
          display.setCursor(0, 35); display.print("B2:----|Hold:Setup");
          // Display current threshold converted to dB
          display.setCursor(0, 50); display.printf("Thr: %.1f dB", rmsToDb(shotThresholdRms));
          lastDispThresholdRms = shotThresholdRms; // Store the RMS value that was displayed
          break;

        case READY:
          display.setTextSize(2); display.setCursor(10, 20); display.println("Get Ready");
          break;

        case WAITING:
          display.setTextSize(2); display.setCursor(5, 10); display.println("Waiting");
          display.setTextSize(1); display.setCursor(0, 40); display.printf("Delay: %lu ms", randomDelayDuration);
          display.setCursor(0, 55); display.print("B1: Cancel");
          break;

        case TIMING:
          display.setTextSize(2); display.setCursor(15, 5); display.println("Shoot!");
          display.setTextSize(1);
          display.setCursor(0, 30); display.printf("Shots: %d/%d", shotCount, currentMaxShots);
          display.setCursor(0, 42);
           if (shotCount > 0) {
               float displayTime = (shotCount == 1) ? firstShotTime : lastSplitTime;
               display.printf("Last: %.3fs", displayTime);
            } else {
                // Show running timer
                display.printf("T: %.3fs", (float)(micros() - beepStartTime) / 1000000.0);
            }
          display.setCursor(0, 55); display.print("B2: Stop");
          break;

        case DISPLAY_RESULT:
          if (shotCount > 0) {
              display.setTextSize(2); display.setCursor(0, 5); display.printf("1: %.3f", firstShotTime);
              display.setTextSize(1);
              if (shotCount > 1) {
                   display.setCursor(0, 30); display.printf("%d: %.3fs", shotCount, lastSplitTime);
              } else {
                   display.setCursor(0, 30); display.print("----------"); // Placeholder
              }
              display.setCursor(0, 42); display.printf("Total: %d", shotCount);
          } else {
              display.setTextSize(2); display.setCursor(5, 15); display.println("No Shots");
              display.setTextSize(1); display.setCursor(0, 40); display.print("Timer Stopped");
          }
          display.setTextSize(1); display.setCursor(0, 55); display.print("B1: Reset");
          lastDisplayedFirstTime = firstShotTime; // Store displayed values
          lastDisplayedSplitTime = lastSplitTime;
          break;

        case SETTINGS:
            display.setTextSize(1); display.setCursor(35, 0); display.println("Settings");
            display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);

            // Display settings with indicator
            display.setCursor(0, 15);
            display.print(currentSettingIndex == 0 ? ">" : " "); display.print("Max Shots:"); display.print(maxShotsSetting);
            display.setCursor(0, 25);
            display.print(currentSettingIndex == 1 ? ">" : " "); display.print("Beep MS:"); display.print(beepDurationSetting);
            display.setCursor(0, 35);
            display.print(currentSettingIndex == 2 ? ">" : " "); display.print("Beep Hz:"); display.print(beepToneSetting);

            display.setTextSize(1); display.setCursor(0, 55); display.print("B1/B2:Adj|Hold:Cyc");

            // Store displayed values for change detection
            lastDisplayedSettingIndex = currentSettingIndex;
            lastDispMaxShots = maxShotsSetting;
            lastDispBeepDur = beepDurationSetting;
            lastDispBeepTone = beepToneSetting;
            break;

        case CALIBRATING_THRESHOLD:
            display.setTextSize(1); display.setCursor(5, 0); display.println("Set Threshold");
            display.drawFastHLine(0, 10, SCREEN_WIDTH, SSD1306_WHITE);
            display.setCursor(0, 15); display.println("Point Mic & Fire!");
            display.setTextSize(1); display.setCursor(0, 30); display.println("Listening...");
            // Display Peak RMS detected so far, converted to dB
            display.setCursor(0, 42); display.printf("Peak: %.1f dB", rmsToDb(peakRmsDuringCalibration));
            display.setTextSize(1); display.setCursor(0, 55); display.print("B2: Cancel");
            lastDispPeakRms = peakRmsDuringCalibration; // Store RMS value that was displayed
            break;

        case CALIBRATION_ERROR: // Ambient noise calib error
            display.clearDisplay(); display.setTextSize(2); display.setTextColor(SSD1306_WHITE);
            display.setCursor(10, 10); display.println("AMBIENT");
            display.setCursor(10, 35); display.println("FAILED!");
            break; // Display is static
      }

      display.display();
      lastDisplayedState = currentState; // Mark state as displayed
      lastDisplayedShotCount = shotCount; // Update shot count always after display
  }
}


// --- Success/Unsuccess Beep Functions --- (Using espTone)
void playSuccessBeeps() {
	int octave = 6;
    int freq[] = {147*octave, 165*octave, 131*octave, 65*octave, 196*octave};
    for (int f : freq) {
        // Play tone continuously for the duration
        myToneGenerator.tone(f);
        delay(SUCCESS_BEEP_DURATION);
        myToneGenerator.noTone();
        delay(SUCCESS_BEEP_DELAY); // Wait for the silence duration
    }
    myToneGenerator.noTone(); // Ensure tone is off at the end
}

void playUnsuccessBeeps() {
    int freq = 65;
    int slowDelay = SUCCESS_BEEP_DELAY * 1.5;
    for (int i = 0; i < 3; i++) { // Play 3 times
        // Play tone continuously for the duration
        myToneGenerator.tone(freq);
        delay(SUCCESS_BEEP_DURATION * 1.5);
        myToneGenerator.noTone();
        delay(slowDelay); // Wait for the silence duration
    }
    myToneGenerator.noTone(); // Ensure tone is off at the end
}

// --- Start/Stop Beep Functions --- (Using espTone)
void startBeep() {
  if (!isBeeping) {
      Serial.printf("BEEP START (Tone: %d Hz, Duration: %d ms)\n", currentBeepToneHz, currentBeepDurationMs);
      // Play tone continuously
      myToneGenerator.tone(currentBeepToneHz);
      isBeeping = true;
      // Set the time when this beep should automatically stop
      beepStopTime = millis() + currentBeepDurationMs;
      // Note: We need to manually call stopBeep() or let the main loop check beepStopTime
  }
}

void stopBeep() {
   // Only call noTone if our logic thinks a beep should be playing.
   if (isBeeping) {
       Serial.println("BEEP STOP (Called - Interrupting with noTone)");
       myToneGenerator.noTone(); // Stop any active tone
       isBeeping = false;
       beepStopTime = 0; // Reset stop time
   }
}
