#include <M5StickCPlus2.h> // Use M5StickCPlus2 library (includes M5GFX)
#include "M5MicPeakRMS.h" // Include the custom library header
#include <cmath>          // Still needed for abs() potentially
#include <float.h>        // Include for FLT_MAX
#include <Preferences.h>  // For Non-Volatile Storage
#include <LittleFS.h>     // For LittleFS File System

// --- Button Aliases (No longer used for B/PWR) ---
// Using direct references now

// --- Configuration Constants ---
const unsigned long LONG_PRESS_DURATION_MS = 750; // Hold duration for settings/return
const unsigned long SHOT_REFRACTORY_MS = 150;     // Min time (ms) between detected shots
const unsigned long TIMEOUT_DURATION_MS = 15000;  // 15 seconds timeout
const unsigned long BEEP_NOTE_DURATION_MS = 150;  // Duration for beep notes
const unsigned long BEEP_NOTE_DELAY_MS = 50;      // Delay between beep notes
const unsigned long BATTERY_CHECK_INTERVAL_MS = 60000; // Check battery every 60 seconds
const float BATTERY_LOW_PERCENTAGE = 0.78;        // 78% threshold for low battery warning
const int MAX_SHOTS_LIMIT = 20; // Absolute maximum array size
const int MENU_ITEM_HEIGHT_LANDSCAPE = 25;
const int MENU_ITEM_HEIGHT_PORTRAIT = 18; // Smaller height for portrait text size 1
const int MENU_ITEMS_PER_SCREEN_LANDSCAPE = 3; // Adjusted for potentially longer setting names
const int MENU_ITEMS_PER_SCREEN_PORTRAIT = 4; // Adjusted for potentially longer setting names
const unsigned long POST_BEEP_DELAY_MS = 350; // Small delay after start beep to prevent false trigger
const int MAX_FILES_LIST = 20; // Max files to list on status screen
const unsigned long BOOT_JPG_FRAME_DELAY_MS = 100; // Delay between JPG frames
const int MAX_BOOT_JPG_FRAMES = 150; // Stop checking after this many frames
const unsigned long MESSAGE_DISPLAY_MS = 2000; // How long to show format success/fail message
const unsigned long DRY_FIRE_RANDOM_DELAY_MIN_MS = 2000; // Min random delay for Dry Fire Par
const unsigned long DRY_FIRE_RANDOM_DELAY_MAX_MS = 5000; // Max random delay for Dry Fire Par
const int MAX_PAR_BEEPS = 10; // Maximum number of par beeps/times we can configure
const unsigned long RECOIL_DETECTION_WINDOW_MS = 100; // Time (ms) after sound peak to check for recoil
const unsigned long MIN_FIRST_SHOT_TIME_MS = 100; // <-- NEW: Minimum time for first shot registration

// --- NVS Keys ---
const char* NVS_NAMESPACE = "ShotTimer";
const char* KEY_MAX_SHOTS = "maxShots";
const char* KEY_BEEP_DUR = "beepDur";
const char* KEY_BEEP_HZ = "beepHz";
const char* KEY_SHOT_THRESH = "shotThresh";
const char* KEY_DF_BEEP_CNT = "dfBeepCnt";
// Dynamic keys for par times will be like "dfParT_0", "dfParT_1", etc.
const char* KEY_NR_RECOIL = "nrRecoil";
const char* KEY_PEAK_BATT = "peakBatt";
const char* KEY_ROTATION = "rotation";
const char* KEY_BOOT_ANIM = "bootAnim";

// --- Timer States ---
enum TimerState {
    BOOT_SCREEN,
    BOOT_JPG_SEQUENCE,
    MODE_SELECTION,
    LIVE_FIRE_READY,
    LIVE_FIRE_GET_READY,
    LIVE_FIRE_TIMING,
    LIVE_FIRE_STOPPED,
    DRY_FIRE_READY,
    DRY_FIRE_RUNNING,
    NOISY_RANGE_READY,
    NOISY_RANGE_GET_READY,
    NOISY_RANGE_TIMING,
    SETTINGS_MENU_MAIN,
    SETTINGS_MENU_GENERAL,
    SETTINGS_MENU_BEEP,
    SETTINGS_MENU_DRYFIRE,
    SETTINGS_MENU_NOISY,
    DEVICE_STATUS,
    LIST_FILES,
    EDIT_SETTING,
    CALIBRATE_THRESHOLD,
    CALIBRATE_RECOIL
};

// --- Operating Modes ---
enum OperatingMode {
    MODE_LIVE_FIRE,
    MODE_DRY_FIRE,
    MODE_NOISY_RANGE
};

// --- Editable Settings Enum ---
enum EditableSetting {
    EDIT_NONE,
    EDIT_MAX_SHOTS,
    EDIT_BEEP_DURATION,
    EDIT_BEEP_TONE,
    EDIT_SHOT_THRESHOLD,
    EDIT_PAR_BEEP_COUNT,
    EDIT_PAR_TIME_ARRAY,
    EDIT_RECOIL_THRESHOLD,
    EDIT_ROTATION,
    EDIT_BOOT_ANIM
};

// --- Global Variables ---
TimerState currentState = BOOT_SCREEN;
TimerState previousState = BOOT_SCREEN;
TimerState stateBeforeEdit = SETTINGS_MENU_MAIN;
OperatingMode currentMode = MODE_LIVE_FIRE;
unsigned long startTime = 0;
unsigned long lastDisplayUpdateTime = 0;
const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 100;

// Settings Variables
int currentMaxShots = 10;
unsigned long currentBeepDuration = 150;
int currentBeepToneHz = 2000;
int shotThresholdRms = 15311;
int dryFireParBeepCount = 3;
float dryFireParTimesSec[MAX_PAR_BEEPS];
float recoilThreshold = 1.5;
int screenRotationSetting = 3;
bool playBootAnimation = true;

// Shot Data Arrays
int shotCount = 0;
unsigned long shotTimestamps[MAX_SHOTS_LIMIT];
float splitTimes[MAX_SHOTS_LIMIT];
unsigned long lastShotTimestamp = 0;
unsigned long lastDetectionTime = 0;

// Menu Variables
int currentMenuSelection = 0;
int menuScrollOffset = 0;
int settingsMenuLevel = 0; // 0=Main, 1=General, 2=DryFire, 3=Noisy, 4=Beep
unsigned long btnTopPressTime = 0;
bool btnTopHeld = false;
bool redrawMenu = true;

// Editing Variables
EditableSetting settingBeingEdited = EDIT_NONE;
int editingIntValue = 0;
unsigned long editingULongValue = 0;
float editingFloatValue = 0.0;
bool editingBoolValue = false;
const char* editingSettingName = "";

// File List Variables
String fileListNames[MAX_FILES_LIST];
size_t fileListSizes[MAX_FILES_LIST];
int fileListCount = 0;
int fileListScrollOffset = 0;

// Audio Level Data
float currentCyclePeakRMS = 0.0;
float peakRMSOverall = 0.0;

// Battery Monitoring
Preferences preferences;
float peakBatteryVoltage = 4.2;
float currentBatteryVoltage = 0.0;
bool lowBatteryWarning = false;
unsigned long lastBatteryCheckTime = 0;

// Create an instance of the library
M5MicPeakRMS micPeakRMS;

// Buzzer Pins (External)
#define BUZZER_PIN 25
#define BUZZER_PIN_2 2

// Boot Sequence Variables
int currentJpgFrame = 1;
bool filesystem_ok_for_boot = false;
unsigned long lastFrameTime = 0;

// Dry Fire Par Variables
unsigned long randomDelayStartMs = 0;
unsigned long parTimerStartTime = 0;
unsigned long beepSequenceStartTime = 0;
int beepsPlayed = 0;
unsigned long nextBeepTime = 0;
unsigned long lastBeepTime = 0;

// Noisy Range Variables
unsigned long lastSoundPeakTime = 0;
bool checkingForRecoil = false;
float peakRecoilValue = 0.0;

// --- Function Prototypes ---
void displayBootScreen(const char* line1a, const char* line1b, const char* line2);
void displayMenu(const char* title, const char* items[], int count, int selection, int scrollOffset);
void displayTimingScreen(float elapsedTime, int count, float lastSplit);
void displayStoppedScreen();
void displayEditScreen();
void displayCalibrationScreen(const char* title, float peakValue, const char* unit);
void displayDeviceStatusScreen();
void displayListFilesScreen();
void displayDryFireReadyScreen();
void displayDryFireRunningScreen(bool waiting, int beepNum, int totalBeeps);
void resetShotData();
void loadSettings();
void saveSettings();
void savePeakVoltage(float voltage);
void playSuccessBeeps();
void playUnsuccessBeeps();
void playTone(int freq, int duration);
void checkBattery();
void drawLowBatteryIndicator();
void handleModeSelectionInput();
void handleSettingsInput();
void handleEditSettingInput();
void handleDeviceStatusInput();
void handleListFilesInput();
void handleCalibrationInput(TimerState calibrationType);
void handleDryFireReadyInput();
void handleDryFireRunning();
void handleNoisyRangeReadyInput();
void handleNoisyRangeGetReady();
void handleNoisyRangeTiming();
void setState(TimerState newState);
String getUpButtonLabel();
String getDownButtonLabel();


// --- Setup ---
void setup() {
    Serial.begin(115200);
    Serial.println("\n=====================================");
    Serial.println(" Hey Man, Nice Shot... Timer - Booting");
    Serial.println("=====================================");

    StickCP2.begin(); // Initialize StickCP2 object

    Serial.println("Initializing Preferences (NVS)...");
    preferences.begin(NVS_NAMESPACE, false);
    loadSettings(); // Load saved settings

    // Apply loaded rotation setting
    StickCP2.Lcd.setRotation(screenRotationSetting);
    Serial.printf("Screen rotation set to: %d\n", screenRotationSetting);

    StickCP2.Lcd.setTextColor(WHITE, BLACK);
    StickCP2.Lcd.setTextDatum(MC_DATUM);
    StickCP2.Lcd.setTextFont(0);

    Serial.println("Initializing StickCP2 Core...");
    Serial.println("StickCP2 Initialized.");

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUZZER_PIN_2, OUTPUT);
    digitalWrite(BUZZER_PIN_2, LOW);

    Serial.println("Disabling Internal Speaker...");
    StickCP2.Speaker.end();
    Serial.println("Internal Speaker disabled.");

    Serial.println("Initializing M5MicPeakRMS library...");
    if (!micPeakRMS.begin(StickCP2)) {
        Serial.println("!!! Library initialization failed!");
        displayBootScreen("ERROR", "", "Mic Init Failed!");
        playUnsuccessBeeps();
        while(true);
    }
    Serial.println("Library Initialized.");
    micPeakRMS.resetPeak();

    Serial.println("Initializing IMU...");
    if (StickCP2.Imu.begin()) {
        Serial.println("IMU Initialized.");
    } else {
        Serial.println("!!! IMU Initialization Failed!");
        displayBootScreen("WARNING", "", "IMU Init Failed!");
        playUnsuccessBeeps();
        delay(2000);
    }

    // --- Initialize LittleFS (Required for JPG Sequence and File List) ---
    Serial.println("Initializing LittleFS...");
    if(!LittleFS.begin()){
        Serial.println("!!! LittleFS Mount Failed! Check partition scheme or format manually.");
        displayBootScreen("ERROR", "", "FS Failed!");
        playUnsuccessBeeps();
        delay(2000); // Show error briefly
        filesystem_ok_for_boot = false;
    } else {
        Serial.println("LittleFS Mounted.");
        filesystem_ok_for_boot = true;
    }

    peakBatteryVoltage = preferences.getFloat(KEY_PEAK_BATT, 4.2);
    Serial.printf("Loaded Peak Battery Voltage: %.2fV\n", peakBatteryVoltage);
    checkBattery(); // Initial battery check

    StickCP2.Lcd.fillScreen(BLACK); // Clear screen after potential warnings

    displayBootScreen("Hey Man, Nice Shot...", "Timer", "Initialization Complete!");
    Serial.println("--- Setup Complete ---");
    playSuccessBeeps();
    delay(500);

    // --- Start Boot Sequence (JPG or direct to Mode Select) ---
    if (filesystem_ok_for_boot && playBootAnimation) {
        Serial.println("Starting JPG Boot Sequence...");
        setState(BOOT_JPG_SEQUENCE);
        currentJpgFrame = 1; // Start from frame 1.jpg
        lastFrameTime = 0; // Initialize frame timer
        StickCP2.Lcd.fillScreen(BLACK); // Clear for sequence
    } else {
        if (!playBootAnimation) Serial.println("Boot animation disabled in settings.");
        else Serial.println("Filesystem failed, skipping boot sequence.");
        delay(1000); // Show "Init Complete" longer if no sequence
        setState(MODE_SELECTION); // Go directly to mode selection
        currentMenuSelection = (int)currentMode;
        menuScrollOffset = 0;
        StickCP2.Lcd.fillScreen(BLACK); // Clear screen
    }
}

// --- Main Loop ---
void loop() {
    StickCP2.update(); // Update StickCP2 components (critical for button reads)

    unsigned long currentTime = millis();

    // --- Update Mic Library (if needed by current state) ---
    // Needed for Live Fire, Noisy Range (sound trigger), and Threshold Cal
    bool micUpdateNeeded = (currentState == LIVE_FIRE_TIMING ||
                            currentState == NOISY_RANGE_TIMING ||
                            currentState == CALIBRATE_THRESHOLD);
    if (micUpdateNeeded) {
        micPeakRMS.update();
    }

    // --- Periodic Battery Check ---
    if (currentTime - lastBatteryCheckTime > BATTERY_CHECK_INTERVAL_MS) {
        checkBattery();
        // Force redraw if status screen is active and battery warning state changed
        if (currentState == DEVICE_STATUS || currentState == LIST_FILES || currentState == MODE_SELECTION) { // Redraw mode select too for battery %
             redrawMenu = true; // Force redraw on interval for status/list screens
        }
    }

    // --- Global Button Checks ---
    // Check for settings menu entry (Long press Top button - StickCP2.BtnB)
    if (StickCP2.BtnB.isPressed()) { // Use direct reference
        if (btnTopPressTime == 0) {
            btnTopPressTime = currentTime;
        } else if (!btnTopHeld && (currentTime - btnTopPressTime > LONG_PRESS_DURATION_MS)) {
            // Only enter settings if not already in a settings-related state or boot sequence
            if (currentState != SETTINGS_MENU_MAIN && currentState != SETTINGS_MENU_GENERAL &&
                currentState != SETTINGS_MENU_BEEP &&
                currentState != SETTINGS_MENU_DRYFIRE && currentState != SETTINGS_MENU_NOISY &&
                currentState != DEVICE_STATUS && currentState != LIST_FILES &&
                currentState != EDIT_SETTING && currentState != CALIBRATE_THRESHOLD &&
                currentState != CALIBRATE_RECOIL &&
                currentState != BOOT_JPG_SEQUENCE &&
                currentState != DRY_FIRE_READY && currentState != DRY_FIRE_RUNNING &&
                currentState != NOISY_RANGE_READY && currentState != NOISY_RANGE_GET_READY && currentState != NOISY_RANGE_TIMING) // Don't interrupt active timers
            {
                btnTopHeld = true;
                Serial.println("Top Button Long Press Detected - Entering Settings");
                setState(SETTINGS_MENU_MAIN);
                StickCP2.Lcd.fillScreen(BLACK);
                settingsMenuLevel = 0;
                currentMenuSelection = 0;
                menuScrollOffset = 0;
            }
        }
    } else {
        btnTopPressTime = 0;
        btnTopHeld = false;
    }

    // --- State Machine ---
    switch (currentState) {
        case BOOT_SCREEN: break; // Handled in setup

        case BOOT_JPG_SEQUENCE:
            {
                if (currentState != BOOT_JPG_SEQUENCE) break; // Exit if state changed mid-logic

                // --- Check for skip button press (runs every loop) ---
                if (StickCP2.BtnA.wasClicked()) {
                    Serial.println("Boot sequence skipped by user.");
                    setState(MODE_SELECTION);
                    currentMenuSelection = (int)currentMode;
                    menuScrollOffset = 0;
                    StickCP2.Lcd.fillScreen(BLACK);
                    break; // Exit case
                }

                // --- Check if it's time to display the next frame ---
                if (currentTime - lastFrameTime >= BOOT_JPG_FRAME_DELAY_MS) {

                    // Construct filename (e.g., /1.jpg, /2.jpg)
                    char jpgFilename[12];
                    sprintf(jpgFilename, "/%d.jpg", currentJpgFrame);

                    if (LittleFS.exists(jpgFilename) && currentJpgFrame <= MAX_BOOT_JPG_FRAMES) {
                        File jpgFile = LittleFS.open(jpgFilename, FILE_READ);
                        if (!jpgFile) {
                            Serial.printf("!!! Failed to open %s\n", jpgFilename);
                            setState(MODE_SELECTION); // Go to mode selection on error
                            break;
                        }
                        bool success = StickCP2.Lcd.drawJpg(&jpgFile, 0, 0, StickCP2.Lcd.width(), StickCP2.Lcd.height(), 0, 0, 0.0f, 0.0f, datum_t::middle_center);
                        jpgFile.close();

                        if (!success) {
                            Serial.printf("!!! Failed to draw %s\n", jpgFilename);
                            setState(MODE_SELECTION); // Go to mode selection on error
                            break;
                        }
                        currentJpgFrame++;
                        lastFrameTime = currentTime;
                    } else {
                        // File doesn't exist or max frames reached
                        if (currentJpgFrame <= MAX_BOOT_JPG_FRAMES) { // Only print "not found" if we didn't hit the max
                           Serial.printf("%s not found.\n", jpgFilename);
                        } else {
                           Serial.println("Max boot frames reached.");
                        }
                        Serial.println("Boot sequence finished.");
                        setState(MODE_SELECTION);
                    }
                } // End time check
                 if (currentState == MODE_SELECTION) { // If state changed, clear screen
                    currentMenuSelection = (int)currentMode;
                    menuScrollOffset = 0;
                    StickCP2.Lcd.fillScreen(BLACK);
                 }
            }
            break;

        case MODE_SELECTION:
            handleModeSelectionInput();
            break;

        // --- Live Fire Mode States ---
        case LIVE_FIRE_READY:
            if (redrawMenu) {
                displayTimingScreen(0.0, 0, 0.0); // Use the standard timing screen display
                redrawMenu = false;
            }
            if (StickCP2.BtnA.wasClicked()) {
                Serial.println("Front Button pressed - Starting Live Fire");
                setState(LIVE_FIRE_GET_READY);
                StickCP2.Lcd.fillScreen(BLACK);
                StickCP2.Lcd.setTextDatum(MC_DATUM);
                StickCP2.Lcd.setTextFont(0);
                StickCP2.Lcd.setTextSize(3);
                StickCP2.Lcd.drawString("Ready...", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()/2);
                delay(1000); // Short delay for user readiness
            }
            break;

        case LIVE_FIRE_GET_READY:
            Serial.println("Generating start beep...");
            playTone(currentBeepToneHz, currentBeepDuration);
            delay(POST_BEEP_DELAY_MS); // Add delay after beep
            micPeakRMS.resetPeak();    // Reset peak immediately after beep/delay
            Serial.println("Beep finished, peak reset. Starting timer.");
            resetShotData(); // Resets counts and overall peak level
            startTime = millis(); // Set start time AFTER reset and delay
            lastDisplayUpdateTime = 0;
            StickCP2.Lcd.fillScreen(BLACK); // <-- Explicit clear before timing starts
            setState(LIVE_FIRE_TIMING); // redrawMenu is set by setState
            break;

        case LIVE_FIRE_TIMING:
            {
                if (currentState != LIVE_FIRE_TIMING) break;

                float currentElapsedTime = (currentTime - startTime) / 1000.0;
                currentCyclePeakRMS = micPeakRMS.getPeakRMS();

                if (currentCyclePeakRMS > peakRMSOverall) {
                    peakRMSOverall = currentCyclePeakRMS;
                }

                if (redrawMenu || currentTime - lastDisplayUpdateTime >= DISPLAY_UPDATE_INTERVAL_MS) { // Check redrawMenu here too
                    float lastSplit = (shotCount > 0) ? splitTimes[shotCount - 1] : 0.0;
                    displayTimingScreen(currentElapsedTime, shotCount, lastSplit);
                    lastDisplayUpdateTime = currentTime;
                }

                // Shot Detection Logic
                if (currentCyclePeakRMS > shotThresholdRms &&
                    currentTime - lastDetectionTime > SHOT_REFRACTORY_MS &&
                    shotCount < currentMaxShots)
                {
                    unsigned long shotTimeMillis = currentTime;

                    // --- Ignore first shot if too soon ---
                    if (shotCount == 0 && (shotTimeMillis - startTime) <= MIN_FIRST_SHOT_TIME_MS) {
                        Serial.printf("Ignoring early first shot detected at %lu ms (Start: %lu ms)\n", shotTimeMillis, startTime);
                        lastDetectionTime = currentTime; // Still update refractory timer
                        micPeakRMS.resetPeak(); // Reset peak as it was likely the beep
                    } else {
                        // --- Register the shot ---
                        lastDetectionTime = shotTimeMillis;
                        shotTimestamps[shotCount] = shotTimeMillis;

                        float currentSplit;
                        if (shotCount == 0) {
                            currentSplit = (shotTimeMillis - startTime) / 1000.0;
                        } else {
                            if (lastShotTimestamp > 0) {
                               currentSplit = (shotTimeMillis - lastShotTimestamp) / 1000.0;
                            } else { currentSplit = 0.0; }
                        }
                        lastShotTimestamp = shotTimeMillis;
                        splitTimes[shotCount] = currentSplit;

                        Serial.printf("Shot %d Detected! Cycle PeakRMS: %.0f, Time: %.2fs, Split: %.2fs\n",
                                      shotCount + 1, currentCyclePeakRMS, (shotTimeMillis - startTime)/1000.0, currentSplit);
                        shotCount++;

                        // Force immediate update of timing screen after shot
                        redrawMenu = true;
                        displayTimingScreen(currentElapsedTime, shotCount, currentSplit);
                        lastDisplayUpdateTime = currentTime;

                        if (shotCount >= currentMaxShots) {
                            Serial.println("Max shots reached. Stopping timer.");
                            setState(LIVE_FIRE_STOPPED);
                            StickCP2.Lcd.fillScreen(BLACK);
                            displayStoppedScreen();
                            if (shotCount > 0) playSuccessBeeps(); else playUnsuccessBeeps();
                        }
                        micPeakRMS.resetPeak(); // Reset peak after processing a valid shot
                    }
                } else {
                     micPeakRMS.resetPeak(); // Reset peak if no shot detected this cycle
                }


                // Check for Stop Button Press (Manual Stop - Use wasClicked)
                if (currentState == LIVE_FIRE_TIMING && StickCP2.BtnA.wasClicked()) {
                    Serial.println("Stop button pressed manually.");
                    setState(LIVE_FIRE_STOPPED);
                    StickCP2.Lcd.fillScreen(BLACK);
                    displayStoppedScreen();
                    if (shotCount > 0) playSuccessBeeps(); else playUnsuccessBeeps();
                }

                // Check for Timeout
                if (currentState == LIVE_FIRE_TIMING) {
                    unsigned long timeSinceEvent = (shotCount == 0) ? (currentTime - startTime) : (currentTime - lastShotTimestamp);
                    bool hasStarted = (startTime > 0); // Timer considered started if beep happened

                    if (hasStarted && timeSinceEvent > TIMEOUT_DURATION_MS) {
                        Serial.printf("Timeout reached (%s). Stopping timer.\n", (shotCount == 0) ? "no shots" : "after last shot");
                        setState(LIVE_FIRE_STOPPED);
                        StickCP2.Lcd.fillScreen(BLACK);
                        displayStoppedScreen();
                         if (shotCount > 0) playSuccessBeeps(); else playUnsuccessBeeps();
                    }
                }
            } // End scope for LIVE_FIRE_TIMING case
            break;

        case LIVE_FIRE_STOPPED:
            if (redrawMenu) {
                displayStoppedScreen();
                redrawMenu = false;
            }
            if (StickCP2.BtnA.wasClicked()) {
                Serial.println("Resetting timer...");
                // Determine which ready state to return to based on previous state
                if (previousState == NOISY_RANGE_TIMING || previousState == NOISY_RANGE_GET_READY) {
                     setState(NOISY_RANGE_READY);
                } else {
                     setState(LIVE_FIRE_READY);
                }
                StickCP2.Lcd.fillScreen(BLACK);
            }
            break;

        // --- Dry Fire Par Mode States ---
        case DRY_FIRE_READY:
            handleDryFireReadyInput();
            break;

        case DRY_FIRE_RUNNING:
            handleDryFireRunning();
            break;

        // --- Noisy Range Mode States ---
        case NOISY_RANGE_READY:
            handleNoisyRangeReadyInput();
            break;
        case NOISY_RANGE_GET_READY:
            handleNoisyRangeGetReady();
            break;
        case NOISY_RANGE_TIMING:
            handleNoisyRangeTiming();
            break;

        // --- Settings States ---
        case SETTINGS_MENU_MAIN:
        case SETTINGS_MENU_GENERAL:
        case SETTINGS_MENU_BEEP: // <-- Added case
        case SETTINGS_MENU_DRYFIRE:
        case SETTINGS_MENU_NOISY:
            handleSettingsInput();
            break;

        case EDIT_SETTING:
            handleEditSettingInput();
            break;

        case DEVICE_STATUS:
            handleDeviceStatusInput();
            break;

        case LIST_FILES:
            handleListFilesInput();
            break;

        case CALIBRATE_THRESHOLD:
            handleCalibrationInput(CALIBRATE_THRESHOLD);
            break;

        case CALIBRATE_RECOIL:
            handleCalibrationInput(CALIBRATE_RECOIL);
            break;

        // case CONFIRM_FORMAT_LITTLEFS: // <-- REMOVED case
        //     handleConfirmFormatInput();
        //     break;

    } // End switch(currentState)

    delay(1); // Small delay for stability
}

// --- ==================== ---
// --- Helper Functions     ---
// --- ==================== ---

/**
 * @brief Sets the current state and flags for redraw.
 */
void setState(TimerState newState) {
    if (currentState != newState) {
        previousState = currentState;
        currentState = newState;
        redrawMenu = true; // Always flag redraw on state change
        Serial.printf("State changed from %d to %d\n", previousState, currentState);
    }
}


/**
 * @brief Displays the boot screen message with line break.
 */
void displayBootScreen(const char* line1a, const char* line1b, const char* line2) {
    StickCP2.Lcd.fillScreen(BLACK);
    StickCP2.Lcd.setTextDatum(MC_DATUM); // Middle Center
    StickCP2.Lcd.setTextFont(0);
    StickCP2.Lcd.setTextSize(2);
    // Draw first part of line 1, move down, draw second part
    StickCP2.Lcd.drawString(line1a, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2 - 25);
    StickCP2.Lcd.drawString(line1b, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2 - 5);
    // Draw line 2 below that
    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.drawString(line2, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2 + 25);
}

/**
 * @brief Gets the physical label for the button used for "Up" action based on rotation.
 */
String getUpButtonLabel() {
    int rotation = StickCP2.Lcd.getRotation();
    switch (rotation) {
        case 0: return "Right"; // BtnB is on the right
        case 1: return "Top";   // BtnB is on the top
        case 2: return "Left";  // BtnB is on the left
        case 3: return "Bottom";// BtnPWR is on the top (physically bottom)
        default: return "Top";
    }
}

/**
 * @brief Gets the physical label for the button used for "Down" action based on rotation.
 */
String getDownButtonLabel() {
    int rotation = StickCP2.Lcd.getRotation();
    switch (rotation) {
        case 0: return "Left";  // BtnPWR is on the left
        case 1: return "Bottom";// BtnPWR is on the bottom
        case 2: return "Right"; // BtnPWR is on the right
        case 3: return "Top";   // BtnB is on the bottom (physically top)
        default: return "Bottom";
    }
}


/**
 * @brief Generic function to display a menu with scrolling.
 */
void displayMenu(const char* title, const char* items[], int count, int selection, int scrollOffset) {
    StickCP2.Lcd.fillScreen(BLACK);
    StickCP2.Lcd.setTextDatum(TC_DATUM);
    StickCP2.Lcd.setTextFont(0);
    StickCP2.Lcd.setTextSize(2);
    StickCP2.Lcd.drawString(title, StickCP2.Lcd.width() / 2, 10);

    // --- Draw Battery Percentage (Only on Mode Select Screen) ---
    if (strcmp(title, "Select Mode") == 0) {
        StickCP2.Lcd.setTextDatum(TR_DATUM); // Top Right alignment
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(1);
        int batt_pct = StickCP2.Power.getBatteryLevel();
        StickCP2.Lcd.setTextColor(WHITE, BLACK); // Ensure correct color
        StickCP2.Lcd.drawString(String(batt_pct) + "%", StickCP2.Lcd.width() - 5, 5);
    }

    StickCP2.Lcd.setTextDatum(TL_DATUM); // Reset datum for menu items
    int y_pos = 45;
    int rotation = StickCP2.Lcd.getRotation();
    int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;
    int itemHeight = (rotation % 2 == 0) ? MENU_ITEM_HEIGHT_PORTRAIT : MENU_ITEM_HEIGHT_LANDSCAPE;
    int textSize = (rotation % 2 == 0) ? 1 : 2;

    StickCP2.Lcd.setTextSize(textSize);

    int startIdx = scrollOffset;
    int endIdx = min(scrollOffset + itemsPerScreen, count);

    for (int i = startIdx; i < endIdx; ++i) {
        int display_y = y_pos + (i - startIdx) * itemHeight;

        String itemText = items[i]; // Get the base item text

        // --- Display current value next to the setting name ---
        bool isNavOrAction = (strcmp(items[i], "Back") == 0 ||
                              strcmp(items[i], "Calibrate Thresh.") == 0 ||
                              strcmp(items[i], "Calibrate Recoil") == 0 ||
                              strcmp(items[i], "Device Status") == 0 ||
                              strcmp(items[i], "List Files") == 0 ||
                              strcmp(items[i], "Beep Settings") == 0); // Don't show value for sub-menu entry

        bool isParTimeSetting = (settingsMenuLevel == 2 && strncmp(items[i], "Par Time", 8) == 0);

        // Show value only if it's a setting in a sub-menu (level > 0) AND not a navigation/action item
        if (settingsMenuLevel > 0 && !isNavOrAction && !isParTimeSetting) {
            itemText += ": ";
            if (strcmp(items[i], "Max Shots") == 0) itemText += currentMaxShots;
            else if (strcmp(items[i], "Beep Duration") == 0) itemText += currentBeepDuration;
            else if (strcmp(items[i], "Beep Tone") == 0) itemText += currentBeepToneHz;
            else if (strcmp(items[i], "Shot Threshold") == 0) itemText += shotThresholdRms;
            else if (strcmp(items[i], "Par Beep Count") == 0) itemText += dryFireParBeepCount;
            else if (strcmp(items[i], "Recoil Threshold") == 0) itemText += String(recoilThreshold, 1);
            else if (strcmp(items[i], "Screen Rotation") == 0) itemText += screenRotationSetting;
            else if (strcmp(items[i], "Boot Animation") == 0) itemText += (playBootAnimation ? "On" : "Off");
        }
        // Note: The value for Par Time X is already included in the dynamically generated itemText

        // --- Highlight and Draw ---
        if (i == selection) {
            StickCP2.Lcd.setTextColor(BLACK, WHITE);
            StickCP2.Lcd.fillRect(5, display_y - 2, StickCP2.Lcd.width() - 10, (textSize == 1 ? 14 : 20), WHITE);
            StickCP2.Lcd.drawString(itemText, 15, display_y);
            StickCP2.Lcd.setTextColor(WHITE, BLACK);
        } else {
            StickCP2.Lcd.drawString(itemText, 15, display_y);
        }
    }

    // Scroll indicators
    if (scrollOffset > 0) {
        StickCP2.Lcd.fillTriangle(StickCP2.Lcd.width() / 2, 35, StickCP2.Lcd.width() / 2 - 5, 40, StickCP2.Lcd.width() / 2 + 5, 40, WHITE);
    }
    if (endIdx < count) {
        StickCP2.Lcd.fillTriangle(StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 5, StickCP2.Lcd.width() / 2 - 5, StickCP2.Lcd.height() - 10, StickCP2.Lcd.width() / 2 + 5, StickCP2.Lcd.height() - 10, WHITE);
    }

    drawLowBatteryIndicator(); // Draw low battery indicator if needed (separate from percentage)
    StickCP2.Lcd.setTextDatum(TL_DATUM); // Reset datum just in case
}

/**
 * @brief Handles input for the Mode Selection screen with scrolling.
 */
void handleModeSelectionInput() {
    const char* modeItems[] = {"Live Fire", "Dry Fire Par", "Noisy Range"};
    int modeCount = sizeof(modeItems) / sizeof(modeItems[0]);
    int rotation = StickCP2.Lcd.getRotation();
    int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;


    // Adjust scroll offset based on selection
    if (currentMenuSelection < menuScrollOffset) {
        menuScrollOffset = currentMenuSelection; redrawMenu = true;
    } else if (currentMenuSelection >= menuScrollOffset + itemsPerScreen) {
        menuScrollOffset = currentMenuSelection - itemsPerScreen + 1; redrawMenu = true;
    }

    if (redrawMenu) {
        displayMenu("Select Mode", modeItems, modeCount, currentMenuSelection, menuScrollOffset);
        redrawMenu = false;
    }

    // Use wasClicked for Up/Down, swapping buttons based on rotation 3
    bool upPressed = (rotation == 3) ? M5.BtnPWR.wasClicked() : StickCP2.BtnB.wasClicked();
    bool downPressed = (rotation == 3) ? StickCP2.BtnB.wasClicked() : M5.BtnPWR.wasClicked();

    if (upPressed) { // Up Action
        currentMenuSelection = (currentMenuSelection - 1 + modeCount) % modeCount; redrawMenu = true;
        if(rotation == 3) Serial.println("Bottom (BtnPWR) Click Detected for Menu Up (Rotation 3)");
    }
    if (downPressed) { // Down Action
        currentMenuSelection = (currentMenuSelection + 1) % modeCount; redrawMenu = true;
         if(rotation != 3) Serial.println("Bottom (BtnPWR) Click Detected for Menu Down");
         else Serial.println("Top (BtnB) Click Detected for Menu Down (Rotation 3)");
    }

    // Use wasClicked for Select (Short Press Front)
    if (StickCP2.BtnA.wasClicked()) {
        currentMode = (OperatingMode)currentMenuSelection;
        Serial.printf("Mode Selected: %d\n", currentMode);
        switch (currentMode) {
            case MODE_LIVE_FIRE:   setState(LIVE_FIRE_READY); break;
            case MODE_DRY_FIRE:    setState(DRY_FIRE_READY); break;
            case MODE_NOISY_RANGE: setState(NOISY_RANGE_READY); break;
        }
        StickCP2.Lcd.fillScreen(BLACK);
        menuScrollOffset = 0;
    }
}

/**
 * @brief Handles input for all levels of the Settings menu with scrolling.
 */
void handleSettingsInput() {
    const char* title = "Settings";
    const char** items = nullptr; // This will point to a temporary buffer for dynamic menus
    int itemCount = 0;
    int rotation = StickCP2.Lcd.getRotation();
    int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;

    // --- Static Menu Definitions ---
    static const char* mainItems[] = {"General", "Dry Fire", "Noisy Range", "Device Status", "List Files", "Save & Exit"};
    static const char* generalItems[] = {"Max Shots", "Beep Settings", "Shot Threshold", "Screen Rotation", "Boot Animation", "Calibrate Thresh.", "Back"}; // <-- Changed Beep options
    static const char* beepItems[] = {"Beep Duration", "Beep Tone", "Back"}; // <-- NEW Beep Submenu
    static const char* noisyItems[] = {"Recoil Threshold", "Calibrate Recoil", "Back"};

    // --- Dynamic Menu Buffer (for Dry Fire) ---
    const int maxDryFireItems = 1 + MAX_PAR_BEEPS + 1;
    static const char* dryFireItemsBuffer[maxDryFireItems];
    static String dryFireItemStrings[MAX_PAR_BEEPS];

    switch (settingsMenuLevel) {
        case 0: // Main Menu
            items = mainItems;
            itemCount = sizeof(mainItems) / sizeof(mainItems[0]);
            title = "Settings";
            break;
        case 1: // General Settings
            items = generalItems;
            itemCount = sizeof(generalItems) / sizeof(generalItems[0]);
            title = "General Settings";
            break;
        case 2: // Dry Fire Menu (Dynamic)
            title = "Dry Fire Settings";
            itemCount = 0; // Reset count
            // 1. Add "Par Beep Count"
            dryFireItemsBuffer[itemCount++] = "Par Beep Count";
            // 2. Add "Par Time X" for each beep up to the current count
            for (int i = 0; i < dryFireParBeepCount && i < MAX_PAR_BEEPS; ++i) {
                dryFireItemStrings[i] = "Par Time " + String(i + 1) + ": " + String(dryFireParTimesSec[i], 1) + "s";
                dryFireItemsBuffer[itemCount++] = dryFireItemStrings[i].c_str();
            }
            // 3. Add "Back"
            dryFireItemsBuffer[itemCount++] = "Back";
            items = dryFireItemsBuffer;
            break;
        case 3: // Noisy Range Settings
            items = noisyItems;
            itemCount = sizeof(noisyItems) / sizeof(noisyItems[0]);
            title = "Noisy Range Settings";
            break;
        case 4: // Beep Settings <-- NEW Level
             items = beepItems;
             itemCount = sizeof(beepItems) / sizeof(beepItems[0]);
             title = "Beep Settings";
             break;
    }

    // Adjust scroll offset
    if (currentMenuSelection >= itemCount) {
        currentMenuSelection = max(0, itemCount - 1);
        redrawMenu = true;
    }
    if (currentMenuSelection < menuScrollOffset) {
        menuScrollOffset = currentMenuSelection; redrawMenu = true;
    } else if (currentMenuSelection >= menuScrollOffset + itemsPerScreen) {
        menuScrollOffset = currentMenuSelection - itemsPerScreen + 1; redrawMenu = true;
    }

    if (redrawMenu) {
        if (items) { displayMenu(title, items, itemCount, currentMenuSelection, menuScrollOffset); }
        else { /* Error */ }
        redrawMenu = false;
    }

    // --- Input Handling ---
    bool upPressed = (rotation == 3) ? M5.BtnPWR.wasClicked() : StickCP2.BtnB.wasClicked();
    bool downPressed = (rotation == 3) ? StickCP2.BtnB.wasClicked() : M5.BtnPWR.wasClicked();

    if (upPressed) { // Up Action
        currentMenuSelection = (currentMenuSelection - 1 + itemCount) % itemCount; redrawMenu = true;
        if(rotation == 3) Serial.println("Bottom (BtnPWR) Click Detected for Menu Up (Rotation 3)");
    }
    if (downPressed) { // Down Action
        currentMenuSelection = (currentMenuSelection + 1) % itemCount; redrawMenu = true;
        if(rotation != 3) Serial.println("Bottom (BtnPWR) Click Detected for Menu Down");
        else Serial.println("Top (BtnB) Click Detected for Menu Down (Rotation 3)");
    }

    // Return (Hold Front)
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
         if (settingsMenuLevel == 0) {
             Serial.println("Returning to Mode Selection from Settings");
             setState(MODE_SELECTION); currentMenuSelection = (int)currentMode; menuScrollOffset = 0;
             StickCP2.Lcd.fillScreen(BLACK);
         } else if (settingsMenuLevel == 1 || settingsMenuLevel == 2 || settingsMenuLevel == 3) { // General, Dry, Noisy return to Main
             Serial.println("Returning to Main Settings Menu");
             settingsMenuLevel = 0; currentMenuSelection = 0; menuScrollOffset = 0; redrawMenu = true;
         } else if (settingsMenuLevel == 4) { // Beep Settings return to General
             Serial.println("Returning to General Settings Menu");
             settingsMenuLevel = 1; currentMenuSelection = 1; /* Index of Beep Settings */ menuScrollOffset = 0; redrawMenu = true;
         }
         return;
    }

    // Select (Short Press Front)
    if (StickCP2.BtnA.wasClicked()) {
        Serial.printf("Settings Select: Level=%d, Selection=%d (%s)\n", settingsMenuLevel, currentMenuSelection, items[currentMenuSelection]);
        bool needsActionRedraw = true;

        // --- Handle Selection Action ---
        if (settingsMenuLevel == 0) { // Main Menu
            if (strcmp(items[currentMenuSelection], "General") == 0) settingsMenuLevel = 1;
            else if (strcmp(items[currentMenuSelection], "Dry Fire") == 0) settingsMenuLevel = 2;
            else if (strcmp(items[currentMenuSelection], "Noisy Range") == 0) settingsMenuLevel = 3;
            else if (strcmp(items[currentMenuSelection], "Device Status") == 0) {
                setState(DEVICE_STATUS); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            }
            else if (strcmp(items[currentMenuSelection], "List Files") == 0) {
                setState(LIST_FILES); fileListScrollOffset = 0; needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            }
            // else if (strcmp(items[currentMenuSelection], "Format LittleFS") == 0) { // REMOVED
            //     setState(CONFIRM_FORMAT_LITTLEFS); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            // }
            else if (strcmp(items[currentMenuSelection], "Save & Exit") == 0) {
                saveSettings(); playSuccessBeeps(); setState(MODE_SELECTION);
                currentMenuSelection = (int)currentMode; menuScrollOffset = 0; needsActionRedraw = false;
                StickCP2.Lcd.fillScreen(BLACK);
            }
            currentMenuSelection = 0; menuScrollOffset = 0;
        }
        else if (settingsMenuLevel == 1) { // General Settings
            editingSettingName = items[currentMenuSelection];
            stateBeforeEdit = SETTINGS_MENU_GENERAL;
            if (strcmp(editingSettingName, "Max Shots") == 0) {
                settingBeingEdited = EDIT_MAX_SHOTS; editingIntValue = currentMaxShots; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Beep Settings") == 0) { // <-- Navigate to Beep Menu
                settingsMenuLevel = 4; currentMenuSelection = 0; menuScrollOffset = 0;
            } else if (strcmp(editingSettingName, "Shot Threshold") == 0) {
                settingBeingEdited = EDIT_SHOT_THRESHOLD; editingIntValue = shotThresholdRms; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Screen Rotation") == 0) {
                settingBeingEdited = EDIT_ROTATION; editingIntValue = screenRotationSetting; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Boot Animation") == 0) {
                settingBeingEdited = EDIT_BOOT_ANIM; editingBoolValue = playBootAnimation; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Calibrate Thresh.") == 0) {
                setState(CALIBRATE_THRESHOLD); peakRMSOverall = 0; micPeakRMS.resetPeak(); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Back") == 0) {
                settingsMenuLevel = 0; currentMenuSelection = 0; menuScrollOffset = 0;
            }
        }
        else if (settingsMenuLevel == 2) { // Dry Fire Settings
            stateBeforeEdit = SETTINGS_MENU_DRYFIRE;
            if (strcmp(items[currentMenuSelection], "Par Beep Count") == 0) {
                editingSettingName = items[currentMenuSelection];
                settingBeingEdited = EDIT_PAR_BEEP_COUNT; editingIntValue = dryFireParBeepCount; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strncmp(items[currentMenuSelection], "Par Time", 8) == 0) { // Check if it's a "Par Time X" item
                int parTimeIndex = currentMenuSelection - 1; // Index in the array (0-based)
                if (parTimeIndex >= 0 && parTimeIndex < dryFireParBeepCount) {
                    String tempTitle = "Par Time " + String(parTimeIndex + 1);
                    static char parTimeEditTitle[20];
                    strncpy(parTimeEditTitle, tempTitle.c_str(), sizeof(parTimeEditTitle) - 1);
                    parTimeEditTitle[sizeof(parTimeEditTitle) - 1] = '\0';
                    editingSettingName = parTimeEditTitle;
                    settingBeingEdited = EDIT_PAR_TIME_ARRAY;
                    editingIntValue = parTimeIndex;
                    editingFloatValue = dryFireParTimesSec[parTimeIndex];
                    setState(EDIT_SETTING);
                    needsActionRedraw = false;
                    StickCP2.Lcd.fillScreen(BLACK);
                }
            } else if (strcmp(items[currentMenuSelection], "Back") == 0) {
                settingsMenuLevel = 0; currentMenuSelection = 0; menuScrollOffset = 0;
            }
        }
        else if (settingsMenuLevel == 3) { // Noisy Range Settings
            editingSettingName = items[currentMenuSelection];
            stateBeforeEdit = SETTINGS_MENU_NOISY;
            if (strcmp(editingSettingName, "Recoil Threshold") == 0) {
                settingBeingEdited = EDIT_RECOIL_THRESHOLD; editingFloatValue = recoilThreshold; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Calibrate Recoil") == 0) {
                setState(CALIBRATE_RECOIL); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK); peakRecoilValue = 0; // Reset peak for calibration
            } else if (strcmp(editingSettingName, "Back") == 0) {
                settingsMenuLevel = 0; currentMenuSelection = 0; menuScrollOffset = 0;
            }
        }
        else if (settingsMenuLevel == 4) { // Beep Settings <-- NEW
            editingSettingName = items[currentMenuSelection];
            stateBeforeEdit = SETTINGS_MENU_BEEP; // Set correct return state
             if (strcmp(editingSettingName, "Beep Duration") == 0) {
                settingBeingEdited = EDIT_BEEP_DURATION; editingULongValue = currentBeepDuration; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Beep Tone") == 0) {
                settingBeingEdited = EDIT_BEEP_TONE; editingIntValue = currentBeepToneHz; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Back") == 0) {
                settingsMenuLevel = 1; currentMenuSelection = 1; /* Index of Beep Settings */ menuScrollOffset = 0;
            }
        }
        if (needsActionRedraw) redrawMenu = true;
    }
}

/**
 * @brief Handles input and display for the EDIT_SETTING state.
 */
void handleEditSettingInput() {
    bool valueChanged = false;
    int rotation = StickCP2.Lcd.getRotation();

    // --- Input Handling ---
    bool upPressed = (rotation == 3) ? M5.BtnPWR.wasClicked() : StickCP2.BtnB.wasClicked();
    bool downPressed = (rotation == 3) ? StickCP2.BtnB.wasClicked() : M5.BtnPWR.wasClicked();

    if (upPressed || downPressed) { // Handle both up and down presses
        valueChanged = true;
        if (upPressed) {
           if(rotation == 3) Serial.println("Bottom (BtnPWR) Click Detected for Edit Up (Rotation 3)");
        } else {
           if(rotation != 3) Serial.println("Bottom (BtnPWR) Click Detected for Edit Down");
           else Serial.println("Top (BtnB) Click Detected for Edit Down (Rotation 3)");
        }

        switch(settingBeingEdited) {
            case EDIT_MAX_SHOTS: editingIntValue = min(editingIntValue + (upPressed ? 1 : -1), MAX_SHOTS_LIMIT); if(editingIntValue < 1) editingIntValue=1; break;
            case EDIT_BEEP_DURATION: editingULongValue = min(editingULongValue + (upPressed ? 50 : -50), 2000UL); if(editingULongValue < 50) editingULongValue=50; break;
            case EDIT_BEEP_TONE: editingIntValue = min(editingIntValue + (upPressed ? 100 : -100), 8000); if(editingIntValue < 500) editingIntValue=500; break;
            case EDIT_SHOT_THRESHOLD: editingIntValue = min(editingIntValue + (upPressed ? 500 : -500), 32000); if(editingIntValue < 100) editingIntValue=100; break;
            case EDIT_PAR_BEEP_COUNT:
                editingIntValue = min(editingIntValue + (upPressed ? 1 : -1), MAX_PAR_BEEPS); // Use MAX_PAR_BEEPS as limit
                if(editingIntValue < 1) editingIntValue=1;
                break;
            case EDIT_PAR_TIME_ARRAY: editingFloatValue = min(editingFloatValue + (upPressed ? 0.1f : -0.1f), 10.0f); if(editingFloatValue < 0.1f) editingFloatValue=0.1f; break; // Edit Par Time
            case EDIT_RECOIL_THRESHOLD: editingFloatValue = min(editingFloatValue + (upPressed ? 0.1f : -0.1f), 5.0f); if(editingFloatValue < 0.5f) editingFloatValue=0.5f; break;
            case EDIT_ROTATION: editingIntValue = (editingIntValue + (upPressed ? 1 : -1) + 4) % 4; break; // Cycle 0-3
            case EDIT_BOOT_ANIM: editingBoolValue = !editingBoolValue; break; // Toggle boolean
            default: valueChanged = false; break;
        }
        // Apply rotation change immediately
        if (settingBeingEdited == EDIT_ROTATION) {
            StickCP2.Lcd.setRotation(editingIntValue);
            redrawMenu = true; // Force redraw after rotation change
        }
    }


    // Return/Cancel (Hold Front)
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
        Serial.println("Edit cancelled.");
        // Restore original rotation if cancelling rotation edit
        if (settingBeingEdited == EDIT_ROTATION) {
             StickCP2.Lcd.setRotation(screenRotationSetting);
        }
        setState(stateBeforeEdit); // Return to the menu we came from
        StickCP2.Lcd.fillScreen(BLACK);
        settingBeingEdited = EDIT_NONE;
        playUnsuccessBeeps();
        return;
    }

    // Confirm (Short Press Front)
    if (StickCP2.BtnA.wasClicked()) {
        Serial.println("Edit confirmed.");
        switch(settingBeingEdited) {
            case EDIT_MAX_SHOTS: currentMaxShots = editingIntValue; break;
            case EDIT_BEEP_DURATION: currentBeepDuration = editingULongValue; break;
            case EDIT_BEEP_TONE: currentBeepToneHz = editingIntValue; break;
            case EDIT_SHOT_THRESHOLD: shotThresholdRms = editingIntValue; break;
            case EDIT_PAR_BEEP_COUNT: dryFireParBeepCount = editingIntValue; break;
            case EDIT_PAR_TIME_ARRAY:
                if (editingIntValue >= 0 && editingIntValue < MAX_PAR_BEEPS) { // editingIntValue holds the index
                    dryFireParTimesSec[editingIntValue] = editingFloatValue;
                }
                break;
            case EDIT_RECOIL_THRESHOLD: recoilThreshold = editingFloatValue; break;
            case EDIT_ROTATION: screenRotationSetting = editingIntValue; break; // Save confirmed rotation
            case EDIT_BOOT_ANIM: playBootAnimation = editingBoolValue; break;
            default: break;
        }
        setState(stateBeforeEdit);
        StickCP2.Lcd.fillScreen(BLACK);
        settingBeingEdited = EDIT_NONE;
        return;
    }

    // --- Display ---
    if (redrawMenu || valueChanged) {
        displayEditScreen();
        redrawMenu = false;
    }
}

/**
 * @brief Displays the screen for editing a setting value.
 */
void displayEditScreen() {
    // Optimized: Only clear the value area if not a full redraw
    if (!redrawMenu) {
         // Clear larger area for Font 7 or On/Off text
         StickCP2.Lcd.fillRect(0, StickCP2.Lcd.height()/2 - 25, StickCP2.Lcd.width(), 50, BLACK);
    } else {
        StickCP2.Lcd.fillScreen(BLACK);
        StickCP2.Lcd.setTextDatum(TC_DATUM);
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(2);
        // Use specific title for Par Time array editing
        if (settingBeingEdited == EDIT_PAR_TIME_ARRAY) {
             String titleStr = "Par Time " + String(editingIntValue + 1); // editingIntValue holds index (0-based)
             StickCP2.Lcd.drawString(titleStr, StickCP2.Lcd.width() / 2, 15);
        } else {
            StickCP2.Lcd.drawString(editingSettingName, StickCP2.Lcd.width() / 2, 15);
        }
        // Instructions only need to be drawn once
        StickCP2.Lcd.setTextDatum(BC_DATUM);
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(1);
        // Use helper functions for dynamic button labels
        if (settingBeingEdited == EDIT_BOOT_ANIM) { // Special instructions for toggle
            StickCP2.Lcd.drawString(getUpButtonLabel() + " or " + getDownButtonLabel() + " = Toggle", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 25);
        } else {
            StickCP2.Lcd.drawString(getUpButtonLabel() + "=Up / " + getDownButtonLabel() + "=Down", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 25);
        }
        StickCP2.Lcd.drawString("Press=OK / Hold=Cancel", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 10);
    }

    StickCP2.Lcd.setTextDatum(MC_DATUM);
    StickCP2.Lcd.setTextFont(7);
    StickCP2.Lcd.setTextSize(1);

    // Display the value being edited
    switch(settingBeingEdited) {
        case EDIT_MAX_SHOTS: StickCP2.Lcd.drawNumber(editingIntValue, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
        case EDIT_BEEP_DURATION: StickCP2.Lcd.drawNumber(editingULongValue, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
        case EDIT_BEEP_TONE: StickCP2.Lcd.drawNumber(editingIntValue, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
        case EDIT_SHOT_THRESHOLD: StickCP2.Lcd.drawNumber(editingIntValue, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
        case EDIT_PAR_BEEP_COUNT: StickCP2.Lcd.drawNumber(editingIntValue, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
        case EDIT_PAR_TIME_ARRAY: StickCP2.Lcd.drawFloat(editingFloatValue, 1, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break; // Show Par Time value
        case EDIT_RECOIL_THRESHOLD: StickCP2.Lcd.drawFloat(editingFloatValue, 1, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
        case EDIT_ROTATION: StickCP2.Lcd.drawNumber(editingIntValue, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
        case EDIT_BOOT_ANIM: StickCP2.Lcd.drawString(editingBoolValue ? "On" : "Off", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break; // Show Boot Anim
        default: StickCP2.Lcd.drawString("ERROR", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
    }

    drawLowBatteryIndicator(); // Draw indicator last
}


/**
 * @brief Handles input and display for Calibration screens.
 */
void handleCalibrationInput(TimerState calibrationType) {
    static float prevPeakValue = -1.0; // Track previous peak to reduce redraws
    float currentValue = 0.0;
    // float peakValue = 0.0; // Use peakRMSOverall or peakRecoilValue directly
    const char* title = "Calibrating...";
    const char* unit = "";
    bool valueChanged = false; // Flag to force redraw if value changes
    int rotation = StickCP2.Lcd.getRotation(); // Get rotation for menu item calculation
    int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;
    float accX, accY, accZ; // For IMU readings

    // --- Get Current Values ---
    if (calibrationType == CALIBRATE_THRESHOLD) {
        title = "Calibrate Threshold";
        unit = "RMS";
        currentValue = micPeakRMS.getPeakRMS();
        if (currentValue > peakRMSOverall) {
            peakRMSOverall = currentValue;
            valueChanged = true; // Update peak value display
            }
        // peakValue = peakRMSOverall; // Use peakRMSOverall directly
        micPeakRMS.resetPeak(); // Reset for next cycle
    } else if (calibrationType == CALIBRATE_RECOIL) {
        title = "Calibrate Recoil";
        unit = "G";
        StickCP2.Imu.getAccelData(&accX, &accY, &accZ);
        currentValue = abs(accZ); // Use absolute Z-axis acceleration
        if (currentValue > peakRecoilValue) {
            peakRecoilValue = currentValue;
            valueChanged = true; // Update peak value display
        }
        // peakValue = peakRecoilValue; // Use peakRecoilValue directly
    }

    // --- Redraw Screen Only If Needed ---
    if (redrawMenu || valueChanged) {
        // Only clear the value area if just the value changed
        if (!redrawMenu && valueChanged) {
             StickCP2.Lcd.fillRect(0, StickCP2.Lcd.height() / 2 - 25, StickCP2.Lcd.width(), 50, BLACK); // Clear old value area
        } else { // Full redraw on entering state (redrawMenu == true)
            StickCP2.Lcd.fillScreen(BLACK);
            StickCP2.Lcd.setTextDatum(TC_DATUM); StickCP2.Lcd.setTextFont(0); StickCP2.Lcd.setTextSize(2);
            StickCP2.Lcd.drawString(title, StickCP2.Lcd.width() / 2, 10);
            // Simplified Instructions
            StickCP2.Lcd.setTextDatum(BC_DATUM); StickCP2.Lcd.setTextFont(0); StickCP2.Lcd.setTextSize(1);
            StickCP2.Lcd.drawString("Press Front=Save Peak", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 25);
            StickCP2.Lcd.drawString("Hold Front=Cancel", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 10);
            if (calibrationType == CALIBRATE_RECOIL) {
                 StickCP2.Lcd.drawString("Trigger Recoil", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()-45);
            } else {
                 //StickCP2.Lcd.drawString("Make Loud Noise", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()-45);
            }
        }

        // Draw Current and Peak Values
        StickCP2.Lcd.setTextDatum(MC_DATUM);     
        StickCP2.Lcd.setTextFont(1); // Use large font for peak
        StickCP2.Lcd.setTextSize(3);
        String peakStr = "PEAK: " + String((calibrationType == CALIBRATE_RECOIL ? peakRecoilValue : peakRMSOverall), (calibrationType == CALIBRATE_RECOIL ? 2 : 0));
        StickCP2.Lcd.drawString(peakStr, StickCP2.Lcd.width() / 2, (StickCP2.Lcd.height()/2)); // Centered vertically

        drawLowBatteryIndicator();
        redrawMenu = false; // Redraw done
    }

    // Input Handling
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) { // Cancel
        Serial.println("Calibration cancelled.");
        stateBeforeEdit = (calibrationType == CALIBRATE_THRESHOLD) ? SETTINGS_MENU_GENERAL : SETTINGS_MENU_NOISY;
        setState(stateBeforeEdit);
        // Adjust selection index based on Boot Animation item
        currentMenuSelection = (calibrationType == CALIBRATE_THRESHOLD) ? 6 : 1; // Index of Calibrate items
        menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
        StickCP2.Lcd.fillScreen(BLACK);
        playUnsuccessBeeps();
    } else if (StickCP2.BtnA.wasClicked()) { // Save
        if (calibrationType == CALIBRATE_THRESHOLD) {
            Serial.printf("Saving peak threshold value: %.0f\n", peakRMSOverall);
            shotThresholdRms = (int)peakRMSOverall;
            stateBeforeEdit = SETTINGS_MENU_GENERAL;
            setState(stateBeforeEdit);
            currentMenuSelection = 6; // Index of Calibrate Thresh
            menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
            StickCP2.Lcd.fillScreen(BLACK);
            playSuccessBeeps();
        } else if (calibrationType == CALIBRATE_RECOIL) {
            Serial.printf("Saving peak recoil value: %.2f\n", peakRecoilValue);
            recoilThreshold = peakRecoilValue; // Save the captured peak
            stateBeforeEdit = SETTINGS_MENU_NOISY;
            setState(stateBeforeEdit);
            currentMenuSelection = 1; // Index of Calibrate Recoil
             menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
             StickCP2.Lcd.fillScreen(BLACK);
            playSuccessBeeps();
        }
    }
}

/**
 * @brief Handles input and display for the Device Status screen.
 */
void handleDeviceStatusInput() {
    // Update display only when redraw is flagged (entering state or battery check)
    if (redrawMenu) {
        displayDeviceStatusScreen();
        redrawMenu = false;
    }

    // Check for return (Hold Front)
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
        Serial.println("Returning to Main Settings Menu from Status");
        setState(SETTINGS_MENU_MAIN); // Go back to main settings
        currentMenuSelection = 3; // Index of "Device Status"
        int rotation = StickCP2.Lcd.getRotation();
        int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;
        menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
        StickCP2.Lcd.fillScreen(BLACK);
    }
}

/**
 * @brief Displays the Device Status screen.
 */
void displayDeviceStatusScreen() {
    StickCP2.Lcd.fillScreen(BLACK); // Full redraw is okay here as it's infrequent
    StickCP2.Lcd.setTextDatum(TC_DATUM);
    StickCP2.Lcd.setTextFont(0);
    StickCP2.Lcd.setTextSize(2);
    StickCP2.Lcd.drawString("Device Status", StickCP2.Lcd.width() / 2, 10);

    StickCP2.Lcd.setTextDatum(TL_DATUM);
    StickCP2.Lcd.setTextSize(1); // Use smaller text size for all items
    int y_pos = 35;
    int line_h = 12;

    // Battery Info
    float batt_v = StickCP2.Power.getBatteryVoltage() / 1000.0;
    int batt_pct = StickCP2.Power.getBatteryLevel();
    bool charging = StickCP2.Power.isCharging();

    StickCP2.Lcd.setCursor(10, y_pos);
    StickCP2.Lcd.printf("Batt: %.2fV (%d%%) %s", batt_v, batt_pct, charging ? "Chg" : "");
    y_pos += line_h;
    StickCP2.Lcd.setCursor(10, y_pos);
    StickCP2.Lcd.printf("Peak V: %.2fV", peakBatteryVoltage);
    y_pos += line_h + 5; // Extra space

    // IMU Info
    float accX, accY, accZ, gyroX, gyroY, gyroZ, temp;
    StickCP2.Imu.getAccelData(&accX, &accY, &accZ);

    StickCP2.Lcd.setCursor(10, y_pos);
    StickCP2.Lcd.print("IMU Acc (G):");
    y_pos += line_h;
    StickCP2.Lcd.setCursor(15, y_pos);
    StickCP2.Lcd.printf("X:%.2f, Y:%.2f, Z:%.2f", accX, accY, accZ);
    y_pos += line_h + 5; // Extra space

    // Storage Info (Requires LittleFS to be mounted)
    StickCP2.Lcd.setCursor(10, y_pos);
    if (LittleFS.begin()) { // Check if mounted (don't format)
        size_t totalBytes = LittleFS.totalBytes();
        size_t usedBytes = LittleFS.usedBytes();
        StickCP2.Lcd.printf("LittleFS: %u/%u B used", usedBytes, totalBytes);
    } else {
        StickCP2.Lcd.print("LittleFS: Not Mounted!");
    }
    y_pos += line_h;


    // Instructions
    StickCP2.Lcd.setTextDatum(BC_DATUM);
    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.drawString("Hold Front to Return", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 10);

    drawLowBatteryIndicator(); // Draw battery indicator last
}


/**
 * @brief Resets all shot-related data AND library peak.
 */
void resetShotData() {
    shotCount = 0;
    lastShotTimestamp = 0;
    lastDetectionTime = 0;
    currentCyclePeakRMS = 0.0;
    peakRMSOverall = 0.0;
    micPeakRMS.resetPeak();
    checkingForRecoil = false; // Reset recoil check flag
    lastSoundPeakTime = 0;
    Serial.println("Shot data and library peak reset.");
    for (int i = 0; i < MAX_SHOTS_LIMIT; ++i) {
        shotTimestamps[i] = 0;
        splitTimes[i] = 0.0;
    }
}

/**
 * @brief Updates the display during the TIMING state (Cleaned up).
 * Used by both Live Fire and Noisy Range modes.
 */
void displayTimingScreen(float elapsedTime, int count, float lastSplit) {
    static float prevElapsedTime = -1.0;
    static int prevCount = -1;
    static float prevLastSplit = -1.0;
    static bool prevLowBattery = false;
    int rotation = StickCP2.Lcd.getRotation();

    bool updateNeeded = redrawMenu ||
                        abs(elapsedTime - prevElapsedTime) > 0.01 ||
                        count != prevCount ||
                        abs(lastSplit - prevLastSplit) > 0.01 ||
                        lowBatteryWarning != prevLowBattery;

    if (redrawMenu) { // If full redraw needed, clear screen first
        StickCP2.Lcd.fillScreen(BLACK);
    }

    if (!updateNeeded && !redrawMenu) { // Skip if no update needed and not a forced redraw
        return;
    }

    // --- Perform Selective Redraw ---
    StickCP2.Lcd.setTextColor(WHITE, BLACK);
    StickCP2.Lcd.setTextDatum(TL_DATUM);

    // Update Time
    if (redrawMenu || abs(elapsedTime - prevElapsedTime) > 0.01) {
        StickCP2.Lcd.setTextFont(7); // <-- Reverted to Font 7
        StickCP2.Lcd.setTextSize(1);
        int time_y = (rotation % 2 == 0) ? 20 : 15; // Adjust Y position if needed
        // Clear slightly larger area than text to handle different lengths
        StickCP2.Lcd.fillRect(5, time_y, StickCP2.Lcd.width() - 10 , StickCP2.Lcd.fontHeight(7) + 4, BLACK); // Use Font 7 height
        StickCP2.Lcd.setCursor(10, time_y);
        StickCP2.Lcd.printf("%.2f", elapsedTime);
        prevElapsedTime = elapsedTime;
    }

    // Update Shot Count & Split Time
    int shots_y = (rotation % 2 == 0) ? 80 : 75; // Adjusted Y position
    int split_y = shots_y + ((rotation % 2 == 0) ? 20 : 25); // Adjust Y spacing
    int text_size = (rotation % 2 == 0) ? 1 : 2; // <-- Reverted text size logic
    int line_h = (text_size == 1) ? 14 : 20; // Adjust clear height

    if (redrawMenu || count != prevCount) {
        StickCP2.Lcd.setTextFont(0); // Reset font to default
        StickCP2.Lcd.setTextSize(text_size);
        StickCP2.Lcd.fillRect(10, shots_y, StickCP2.Lcd.width() - 20, line_h, BLACK); // Clear shot count line
        StickCP2.Lcd.setCursor(10, shots_y);
        StickCP2.Lcd.printf("Shots: %d/%d", count, currentMaxShots);
        prevCount = count;
    }

    if (redrawMenu || abs(lastSplit - prevLastSplit) > 0.01 || count != prevCount) {
        StickCP2.Lcd.setTextFont(0); // Reset font to default
        StickCP2.Lcd.setTextSize(text_size);
        StickCP2.Lcd.fillRect(10, split_y, StickCP2.Lcd.width() - 20, line_h, BLACK); // Clear split time line
        StickCP2.Lcd.setCursor(10, split_y);
        if (count > 0) {
            StickCP2.Lcd.printf("Split: %.2fs", lastSplit);
        } else {
            StickCP2.Lcd.print("Split: ---");
        }
         prevLastSplit = lastSplit;
    }

    // Update Battery Indicator
    if (redrawMenu || lowBatteryWarning != prevLowBattery) { // Redraw if battery state changed or full redraw needed
        StickCP2.Lcd.fillRect(StickCP2.Lcd.width() - 40, 5, 35, 10, BLACK); // Clear area first
        drawLowBatteryIndicator(); // Draw if needed
        prevLowBattery = lowBatteryWarning;
    }

    redrawMenu = false; // Mark redraw as done
}


/**
 * @brief Displays the summary screen in the STOPPED state.
 * Used by both Live Fire and Noisy Range modes.
 */
void displayStoppedScreen() {
    StickCP2.Lcd.fillScreen(BLACK); // Full redraw for summary is acceptable
    StickCP2.Lcd.setTextFont(0);
    StickCP2.Lcd.setTextColor(WHITE, BLACK);
    StickCP2.Lcd.setTextDatum(TL_DATUM);
    int rotation = StickCP2.Lcd.getRotation();
    int text_size = (rotation % 2 == 0) ? 1 : 2; // <-- Reverted text size logic
    int line_h = (text_size == 1) ? 18 : 30; // <-- Reverted line height logic
    int y_pos = 15;

    StickCP2.Lcd.setTextSize(text_size);

    StickCP2.Lcd.setCursor(10, y_pos);
    StickCP2.Lcd.printf("Total Shots: %d", shotCount);
    y_pos += line_h;

    StickCP2.Lcd.setCursor(10, y_pos);
    if (shotCount > 0) { StickCP2.Lcd.printf("First: %.2fs", splitTimes[0]); }
    else { StickCP2.Lcd.print("First: ---s"); }
    y_pos += line_h;

    StickCP2.Lcd.setCursor(10, y_pos);
     if (shotCount > 1) { StickCP2.Lcd.printf("Last Split: %.2fs", splitTimes[shotCount - 1]); }
     else if (shotCount == 1) { StickCP2.Lcd.print("Last Split: N/A"); }
     else { StickCP2.Lcd.print("Last Split: ---s"); }
    y_pos += line_h;

    // Find and Display Fastest Split
    if (shotCount > 1) {
        float fastestSplit = FLT_MAX;
        int fastestSplitIndex = -1;
        for (int i = 1; i < shotCount; ++i) {
            if (splitTimes[i] < fastestSplit && splitTimes[i] > 0.0) {
                fastestSplit = splitTimes[i];
                fastestSplitIndex = i;
            }
        }
        StickCP2.Lcd.setCursor(10, y_pos);
        if(fastestSplitIndex != -1) {
             StickCP2.Lcd.printf("Fastest: %.2fs (S%d)", fastestSplit, fastestSplitIndex + 1);
        } else {
             StickCP2.Lcd.print("Fastest: N/A");
        }
        y_pos += line_h;
    } else {
        StickCP2.Lcd.setCursor(10, y_pos);
        StickCP2.Lcd.print("Fastest: N/A");
        y_pos += line_h;
    }

    StickCP2.Lcd.setTextSize(1); // Reset size for instruction
    StickCP2.Lcd.setCursor(30, StickCP2.Lcd.height() - 20); // Position instruction lower
    StickCP2.Lcd.print("Press Front to Reset");

    drawLowBatteryIndicator();
}

/**
 * @brief Plays a tone on the external buzzer pins.
 */
void playTone(int freq, int duration) {
    if (freq > 0) {
        tone(BUZZER_PIN, freq, duration);
        tone(BUZZER_PIN_2, freq, duration); // <-- Play on second buzzer too
    } else {
        // If freq is 0, just delay without playing tone
        delay(duration);
    }
    // Note: noTone is called after sequences in playSuccess/UnsuccessBeeps
    // or implicitly handled by the tone() function's duration parameter
}

/**
 * @brief Plays a success sound sequence.
 */
void playSuccessBeeps() {
    Serial.println("Playing Success Beeps...");
    int octave = 6;
    int freq[] = {262 * octave / 4, 392 * octave / 4, 523 * octave / 4};
    for (int f : freq) {
        playTone(f, BEEP_NOTE_DURATION_MS);
        delay(BEEP_NOTE_DELAY_MS);
    }
    noTone(BUZZER_PIN);
    noTone(BUZZER_PIN_2); // <-- Stop second buzzer
}

/**
 * @brief Plays an unsuccess/error sound sequence.
 */
void playUnsuccessBeeps() {
    Serial.println("Playing Unsuccess Beeps...");
    int freq = 150;
    int repeatTone = 3;
    for (int i = 0; i < repeatTone; ++i) {
        playTone(freq, BEEP_NOTE_DURATION_MS * 1.5);
        delay(BEEP_NOTE_DELAY_MS * 2);
    }
    noTone(BUZZER_PIN);
    noTone(BUZZER_PIN_2); // <-- Stop second buzzer
}

/**
 * @brief Checks battery status and updates peak/warning flag.
 */
void checkBattery() {
    currentBatteryVoltage = StickCP2.Power.getBatteryVoltage() / 1000.0;
    Serial.printf("Current Battery Voltage: %.2fV\n", currentBatteryVoltage);

    if (currentBatteryVoltage > peakBatteryVoltage && currentBatteryVoltage > 3.0) {
        peakBatteryVoltage = currentBatteryVoltage;
        savePeakVoltage(peakBatteryVoltage);
        Serial.printf("New Peak Battery Voltage Saved: %.2fV\n", peakBatteryVoltage);
    }

    bool previousWarningState = lowBatteryWarning;
    if (peakBatteryVoltage > 0) {
       lowBatteryWarning = (currentBatteryVoltage < (peakBatteryVoltage * BATTERY_LOW_PERCENTAGE));
    } else {
       lowBatteryWarning = (currentBatteryVoltage < (4.2 * BATTERY_LOW_PERCENTAGE)); // Fallback if peak is bad
    }

    if (lowBatteryWarning && !previousWarningState) {
        Serial.println("!!! Low Battery Warning Triggered !!!");
        playUnsuccessBeeps();
        redrawMenu = true; // Force redraw on warning state change
    } else if (!lowBatteryWarning && previousWarningState) {
         Serial.println("Battery level recovered above threshold.");
         redrawMenu = true; // Force redraw on warning state change
    }

    lastBatteryCheckTime = millis();
}

/**
 * @brief Draws the low battery indicator if needed.
 */
void drawLowBatteryIndicator() {
    if (lowBatteryWarning) {
        StickCP2.Lcd.setTextDatum(TR_DATUM);
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(1);
        StickCP2.Lcd.setTextColor(RED, BLACK);
        StickCP2.Lcd.drawString("(Bat)", StickCP2.Lcd.width() - 5, 5);
        StickCP2.Lcd.setTextColor(WHITE, BLACK);
        StickCP2.Lcd.setTextDatum(TL_DATUM); // Reset datum
    }
}

/**
 * @brief Loads settings from NVS or uses defaults.
 */
void loadSettings() {
    Serial.println("Loading settings from NVS...");
    currentMaxShots = preferences.getInt(KEY_MAX_SHOTS, 10);
    if (currentMaxShots > MAX_SHOTS_LIMIT) {
        currentMaxShots = MAX_SHOTS_LIMIT;
        Serial.printf("Warning: Loaded max shots (%d) exceeds limit (%d). Using limit.\n", preferences.getInt(KEY_MAX_SHOTS, 10), MAX_SHOTS_LIMIT);
    } else if (currentMaxShots <= 0) {
        currentMaxShots = 1;
        Serial.println("Warning: Loaded max shots was <= 0. Setting to 1.");
    }

    currentBeepDuration = preferences.getULong(KEY_BEEP_DUR, 150);
    currentBeepToneHz = preferences.getInt(KEY_BEEP_HZ, 2000);
    shotThresholdRms = preferences.getInt(KEY_SHOT_THRESH, 15311);
    dryFireParBeepCount = preferences.getInt(KEY_DF_BEEP_CNT, 3);
    // Ensure beep count is within valid range for the array
    if (dryFireParBeepCount < 1) dryFireParBeepCount = 1;
    if (dryFireParBeepCount > MAX_PAR_BEEPS) dryFireParBeepCount = MAX_PAR_BEEPS;

    // Load individual par times
    for (int i = 0; i < MAX_PAR_BEEPS; ++i) {
        char key[12];
        sprintf(key, "dfParT_%d", i); // Create keys like dfParT_0, dfParT_1,...
        dryFireParTimesSec[i] = preferences.getFloat(key, 1.0f); // Default to 1.0s
    }

    recoilThreshold = preferences.getFloat(KEY_NR_RECOIL, 1.5);
    screenRotationSetting = preferences.getInt(KEY_ROTATION, 3); // Default to 3
    if (screenRotationSetting < 0 || screenRotationSetting > 3) screenRotationSetting = 3; // Validate
    playBootAnimation = preferences.getBool(KEY_BOOT_ANIM, true);

    Serial.println("Settings loaded.");
}

/**
 * @brief Saves current settings to NVS.
 */
void saveSettings() {
    Serial.println("Saving settings to NVS...");
    preferences.putInt(KEY_MAX_SHOTS, currentMaxShots);
    preferences.putULong(KEY_BEEP_DUR, currentBeepDuration);
    preferences.putInt(KEY_BEEP_HZ, currentBeepToneHz);
    preferences.putInt(KEY_SHOT_THRESH, shotThresholdRms);
    preferences.putInt(KEY_DF_BEEP_CNT, dryFireParBeepCount);
    // Save individual par times
    for (int i = 0; i < MAX_PAR_BEEPS; ++i) {
         char key[12];
         sprintf(key, "dfParT_%d", i);
         preferences.putFloat(key, dryFireParTimesSec[i]);
    }
    preferences.putFloat(KEY_NR_RECOIL, recoilThreshold);
    preferences.putInt(KEY_ROTATION, screenRotationSetting);
    preferences.putBool(KEY_BOOT_ANIM, playBootAnimation);
    Serial.println("Settings saved.");
}

/**
 * @brief Saves the peak battery voltage to NVS.
 */
void savePeakVoltage(float voltage) {
    preferences.putFloat(KEY_PEAK_BATT, voltage);
}

// --- Function to handle LIST_FILES state ---
void handleListFilesInput() {
    int rotation = StickCP2.Lcd.getRotation();
    int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT + 2 : MENU_ITEMS_PER_SCREEN_LANDSCAPE + 1;

    // Populate file list only when entering the state
    if (redrawMenu) {
        fileListCount = 0;
        File root = LittleFS.open("/");
        if (!root) {
            Serial.println("Failed to open root directory");
        } else if (!root.isDirectory()) {
             Serial.println("Root is not a directory");
        } else {
            File file = root.openNextFile();
            while(file && fileListCount < MAX_FILES_LIST){
                if(!file.isDirectory()){ // List only files
                    fileListNames[fileListCount] = String(file.name());
                    fileListSizes[fileListCount] = file.size();
                    fileListCount++;
                }
                file = root.openNextFile();
            }
            root.close(); // Close the root directory handle
        }
    }

    // Handle scrolling input
    bool upPressed = (rotation == 3) ? M5.BtnPWR.wasClicked() : StickCP2.BtnB.wasClicked();
    bool downPressed = (rotation == 3) ? StickCP2.BtnB.wasClicked() : M5.BtnPWR.wasClicked();

    if (upPressed) { // Scroll Up
        if (fileListScrollOffset > 0) {
            fileListScrollOffset--;
            redrawMenu = true;
        }
         if(rotation == 3) Serial.println("Bottom (BtnPWR) Click Detected for File List Up (Rotation 3)");
    }
    if (downPressed) { // Scroll Down
        if (fileListScrollOffset + itemsPerScreen < fileListCount) {
            fileListScrollOffset++;
            redrawMenu = true;
        }
         if(rotation != 3) Serial.println("Bottom (BtnPWR) Click Detected for File List Down");
         else Serial.println("Top (BtnB) Click Detected for File List Down (Rotation 3)");
    }

    if (redrawMenu) {
        displayListFilesScreen();
        redrawMenu = false;
    }

    // Check for return (Hold Front)
     if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
         Serial.println("Returning to Main Settings Menu from File List");
         setState(SETTINGS_MENU_MAIN); // Go back to main settings
         currentMenuSelection = 4; // Index of "List Files"
         menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
         StickCP2.Lcd.fillScreen(BLACK);
     }
}

// --- Function to display LIST_FILES screen ---
void displayListFilesScreen() {
    StickCP2.Lcd.fillScreen(BLACK);
    StickCP2.Lcd.setTextDatum(TC_DATUM);
    StickCP2.Lcd.setTextFont(0);
    StickCP2.Lcd.setTextSize(2);
    StickCP2.Lcd.drawString("LittleFS Files", StickCP2.Lcd.width() / 2, 10);

    StickCP2.Lcd.setTextDatum(TL_DATUM);
    StickCP2.Lcd.setTextSize(1); // Use small text for file list
    int y_pos = 35;
    int line_h = 12; // Smaller line height for text size 1
    int rotation = StickCP2.Lcd.getRotation();
    int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT + 2 : MENU_ITEMS_PER_SCREEN_LANDSCAPE + 1;

    if (fileListCount == 0) {
        StickCP2.Lcd.setCursor(10, y_pos);
        StickCP2.Lcd.print("No files found or");
        y_pos += line_h;
        StickCP2.Lcd.setCursor(10, y_pos);
        StickCP2.Lcd.print("LittleFS error.");
    } else {
        int startIdx = fileListScrollOffset;
        int endIdx = min(fileListScrollOffset + itemsPerScreen, fileListCount);

        for (int i = startIdx; i < endIdx; ++i) {
            int display_y = y_pos + (i - startIdx) * line_h;
            StickCP2.Lcd.setCursor(5, display_y);
            // Truncate long filenames if needed
            String displayName = fileListNames[i];
            if (displayName.length() > 20) { // Adjust max length as needed
                displayName = displayName.substring(0, 17) + "...";
            }
            StickCP2.Lcd.printf("%-20s %6d B", displayName.c_str(), (int)fileListSizes[i]); // Cast size_t
        }

        // Scroll indicators
        if (fileListScrollOffset > 0) {
             StickCP2.Lcd.fillTriangle(StickCP2.Lcd.width() / 2, 28, StickCP2.Lcd.width() / 2 - 4, 33, StickCP2.Lcd.width() / 2 + 4, 33, WHITE);
        }
        if (endIdx < fileListCount) {
             StickCP2.Lcd.fillTriangle(StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 15, StickCP2.Lcd.width() / 2 - 4, StickCP2.Lcd.height() - 20, StickCP2.Lcd.width() / 2 + 4, StickCP2.Lcd.height() - 20, WHITE);
        }
    }

    // Instructions
    StickCP2.Lcd.setTextDatum(BC_DATUM);
    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.drawString("Hold Front to Return", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 5);

    drawLowBatteryIndicator();
}

// --- Dry Fire Par Mode Functions ---

/**
 * @brief Displays the initial screen for Dry Fire Par mode.
 */
void displayDryFireReadyScreen() {
    StickCP2.Lcd.fillScreen(BLACK);
    StickCP2.Lcd.setTextDatum(MC_DATUM);
    StickCP2.Lcd.setTextFont(0);
    StickCP2.Lcd.setTextSize(2);
    StickCP2.Lcd.drawString("Dry Fire Par", StickCP2.Lcd.width() / 2, 30);

    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.drawString("Press Front to Start", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2 + 10);
    StickCP2.Lcd.drawString("Hold Front to Exit", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 20);

    drawLowBatteryIndicator();
}

/**
 * @brief Displays the screen while Dry Fire Par is running (delay or beeping).
 */
void displayDryFireRunningScreen(bool waiting, int beepNum, int totalBeeps) {
    // Only redraw if necessary (usually handled by redrawMenu flag)
    if (!redrawMenu) return;

    StickCP2.Lcd.fillScreen(BLACK);
    StickCP2.Lcd.setTextDatum(MC_DATUM);
    StickCP2.Lcd.setTextFont(0);

    if (waiting) {
        StickCP2.Lcd.setTextSize(3);
        StickCP2.Lcd.drawString("Waiting...", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2);
    } else {
        StickCP2.Lcd.setTextSize(7); // Use large font for beep count
        StickCP2.Lcd.drawString(String(beepNum), StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2 - 10);
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(1);
        StickCP2.Lcd.drawString("Beep / " + String(totalBeeps), StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2 + 35);
    }

    StickCP2.Lcd.setTextDatum(BC_DATUM);
    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.drawString("Hold Front to Cancel", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 10);

    drawLowBatteryIndicator();
    redrawMenu = false; // Mark redraw as done
}

/**
 * @brief Handles input for the DRY_FIRE_READY state.
 */
void handleDryFireReadyInput() {
    if (redrawMenu) {
        displayDryFireReadyScreen();
        redrawMenu = false;
    }

    // Start sequence (Short Press Front)
    if (StickCP2.BtnA.wasClicked()) {
        Serial.println("Starting Dry Fire Par sequence...");
        randomSeed(micros()); // Seed random number generator
        unsigned long randomDelay = random(DRY_FIRE_RANDOM_DELAY_MIN_MS, DRY_FIRE_RANDOM_DELAY_MAX_MS + 1);
        Serial.printf("Random Delay: %lu ms\n", randomDelay);

        randomDelayStartMs = millis();
        parTimerStartTime = randomDelayStartMs + randomDelay; // Time when first beep should occur
        beepSequenceStartTime = 0; // Will be set after first beep
        beepsPlayed = 0;
        nextBeepTime = 0; // Reset next beep time
        lastBeepTime = 0; // Reset last beep time

        setState(DRY_FIRE_RUNNING);
        // displayDryFireRunningScreen will be called because redrawMenu is true
    }

    // Exit (Hold Front)
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
        Serial.println("Exiting Dry Fire Par mode.");
        setState(MODE_SELECTION);
        currentMenuSelection = (int)MODE_DRY_FIRE; // Highlight Dry Fire on return
        int rotation = StickCP2.Lcd.getRotation();
        int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;
        menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1); // Adjust scroll
        StickCP2.Lcd.fillScreen(BLACK);
    }
}

/**
 * @brief Handles the running state (delay and beeps) for Dry Fire Par mode.
 */
void handleDryFireRunning() {
    unsigned long currentTime = millis();

    // Check for cancel (Hold Front)
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
        Serial.println("Dry Fire Par sequence cancelled.");
        setState(DRY_FIRE_READY); // Go back to ready state
        playUnsuccessBeeps();
        return; // Exit handler immediately
    }

    // --- Phase 1: Random Delay ---
    if (beepSequenceStartTime == 0) {
        if (redrawMenu) {
            displayDryFireRunningScreen(true, 0, 0); // Show "Waiting..."
        }
        // Check if delay is over
        if (currentTime >= parTimerStartTime) {
            Serial.println("Random delay finished. Playing first beep.");
            playTone(currentBeepToneHz, currentBeepDuration);
            beepSequenceStartTime = currentTime; // Record time of first beep
            lastBeepTime = beepSequenceStartTime; // The first beep is the 'last' beep initially
            beepsPlayed = 1; // Count the first beep
            // Calculate time for the next beep (if any) based on the first par time
            if (beepsPlayed < dryFireParBeepCount && dryFireParBeepCount > 0) {
                 // Use the par time for the interval *after* the first beep (index 0)
                 int parIndex = 0; // Index for the first interval
                 if (parIndex < MAX_PAR_BEEPS) {
                    nextBeepTime = lastBeepTime + (unsigned long)(dryFireParTimesSec[parIndex] * 1000.0f);
                    Serial.printf("Next beep (%d) scheduled using par time %d (%.1fs) at %lu\n", beepsPlayed + 1, parIndex + 1, dryFireParTimesSec[parIndex], nextBeepTime);
                 } else {
                    Serial.println("Warning: Par index out of bounds for next beep calc.");
                    beepsPlayed = dryFireParBeepCount; // Prevent further beeps
                 }
            }
            redrawMenu = true; // Force redraw to show beep count
        }
    }
    // --- Phase 2: Par Beep Sequence ---
    else {
         if (redrawMenu) {
            displayDryFireRunningScreen(false, beepsPlayed, dryFireParBeepCount); // Show "Beep X / Y"
         }
        // Check if all required beeps have been played
        if (beepsPlayed >= dryFireParBeepCount) {
            Serial.println("Par beep sequence finished.");
            setState(DRY_FIRE_READY); // Go back to ready state
            delay(500); // Small delay before returning to ready screen
        }
        // Check if it's time for the next beep
        else if (currentTime >= nextBeepTime && nextBeepTime > 0) { // Ensure nextBeepTime is valid
            Serial.printf("Playing par beep %d\n", beepsPlayed + 1);
            playTone(currentBeepToneHz, currentBeepDuration);
            lastBeepTime = currentTime; // Update last beep time
            beepsPlayed++;
            // Calculate time for the *next* beep (if any)
            if (beepsPlayed < dryFireParBeepCount) {
                int parIndex = beepsPlayed -1; // Index for the interval *after* the beep we just played
                 if (parIndex >= 0 && parIndex < MAX_PAR_BEEPS) {
                    nextBeepTime = lastBeepTime + (unsigned long)(dryFireParTimesSec[parIndex] * 1000.0f);
                    Serial.printf("Next beep (%d) scheduled using par time %d (%.1fs) at %lu\n", beepsPlayed + 1, parIndex + 1, dryFireParTimesSec[parIndex], nextBeepTime);
                 } else {
                    Serial.println("Warning: Par index out of bounds for next beep calc.");
                    beepsPlayed = dryFireParBeepCount; // Prevent further beeps
                    nextBeepTime = 0; // Invalidate next beep time
                 }
            } else {
                nextBeepTime = 0; // No more beeps to schedule
            }
            redrawMenu = true; // Force redraw to update beep count
        }
    }
}

// --- Noisy Range Mode Functions ---

/**
 * @brief Handles input for the NOISY_RANGE_READY state.
 */
void handleNoisyRangeReadyInput() {
    if (redrawMenu) {
        displayTimingScreen(0.0, 0, 0.0); // Use the standard timing screen display
        redrawMenu = false;
    }
    if (StickCP2.BtnA.wasClicked()) {
        Serial.println("Front Button pressed - Starting Noisy Range");
        setState(NOISY_RANGE_GET_READY);
        StickCP2.Lcd.fillScreen(BLACK);
        StickCP2.Lcd.setTextDatum(MC_DATUM);
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(3);
        StickCP2.Lcd.drawString("Ready...", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()/2);
        delay(1000); // Short delay for user readiness
    }
     // Exit (Hold Front) - Allow exiting from ready state
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
        Serial.println("Exiting Noisy Range mode.");
        setState(MODE_SELECTION);
        currentMenuSelection = (int)MODE_NOISY_RANGE; // Highlight Noisy Range on return
        int rotation = StickCP2.Lcd.getRotation();
        int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;
        menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1); // Adjust scroll
        StickCP2.Lcd.fillScreen(BLACK);
    }
}

/**
 * @brief Handles the get ready phase for Noisy Range mode (plays beep).
 */
void handleNoisyRangeGetReady() {
    Serial.println("Generating start beep (Noisy Range)...");
    playTone(currentBeepToneHz, currentBeepDuration);
    delay(POST_BEEP_DELAY_MS); // Add delay after beep
    micPeakRMS.resetPeak();    // Reset peak immediately after beep/delay
    Serial.println("Beep finished, peak reset. Starting timer (Noisy Range).");
    resetShotData(); // Resets counts, peak, recoil flag etc.
    startTime = millis(); // Set start time AFTER reset and delay
    lastDisplayUpdateTime = 0;
    StickCP2.Lcd.fillScreen(BLACK); // <-- Explicit clear before timing starts
    setState(NOISY_RANGE_TIMING); // redrawMenu is set by setState
}

/**
 * @brief Handles the timing and detection logic for Noisy Range mode.
 */
void handleNoisyRangeTiming() {
    unsigned long currentTime = millis();
    float accX, accY, accZ;

    if (currentState != NOISY_RANGE_TIMING) return; // Exit if state changed

    // --- Update Display Periodically ---
    float currentElapsedTime = (currentTime - startTime) / 1000.0;
    if (redrawMenu || currentTime - lastDisplayUpdateTime >= DISPLAY_UPDATE_INTERVAL_MS) { // Check redrawMenu here too
        float lastSplit = (shotCount > 0) ? splitTimes[shotCount - 1] : 0.0;
        displayTimingScreen(currentElapsedTime, shotCount, lastSplit); // Use standard display
        lastDisplayUpdateTime = currentTime;
    }

    // --- Shot Detection Logic (Sound + Recoil) ---
    currentCyclePeakRMS = micPeakRMS.getPeakRMS(); // Get sound level

    // Step 1: Detect sound peak above threshold (if not already waiting for recoil)
    if (!checkingForRecoil &&
        currentCyclePeakRMS > shotThresholdRms &&
        currentTime - lastDetectionTime > SHOT_REFRACTORY_MS && // Basic refractory period
        shotCount < currentMaxShots)
    {
        Serial.printf("Noisy: Sound Peak Detected (%.0f)\n", currentCyclePeakRMS);
        lastSoundPeakTime = currentTime; // Record time of sound peak
        checkingForRecoil = true;       // Start looking for recoil
        micPeakRMS.resetPeak();         // Reset mic peak immediately
    }

    // Step 2: Check for recoil if a sound peak was just detected
    if (checkingForRecoil) {
        StickCP2.Imu.getAccelData(&accX, &accY, &accZ);
        float currentRecoil = abs(accZ); // Use absolute Z-axis acceleration for recoil check

        // Check if recoil threshold is met within the window
        if (currentRecoil > recoilThreshold) {
             unsigned long shotTimeMillis = lastSoundPeakTime; // Use the time of the sound peak

            // --- Ignore first shot if too soon ---
            if (shotCount == 0 && (shotTimeMillis - startTime) <= MIN_FIRST_SHOT_TIME_MS) {
                Serial.printf("Ignoring early first shot (Noisy) detected at %lu ms (Start: %lu ms)\n", shotTimeMillis, startTime);
                lastDetectionTime = currentTime; // Still update refractory timer
                checkingForRecoil = false; // Reset flag
                lastSoundPeakTime = 0;     // Reset time
            } else {
                // --- Register the shot ---
                Serial.printf("Noisy: Recoil Detected (%.2f G) within window. SHOT REGISTERED.\n", currentRecoil);
                lastDetectionTime = shotTimeMillis; // Update refractory timer
                shotTimestamps[shotCount] = shotTimeMillis;

                float currentSplit;
                if (shotCount == 0) {
                    currentSplit = (shotTimeMillis - startTime) / 1000.0;
                } else {
                    if (lastShotTimestamp > 0) {
                       currentSplit = (shotTimeMillis - lastShotTimestamp) / 1000.0;
                    } else { currentSplit = 0.0; }
                }
                lastShotTimestamp = shotTimeMillis;
                splitTimes[shotCount] = currentSplit;

                Serial.printf("Shot %d Registered! Time: %.2fs, Split: %.2fs\n",
                              shotCount + 1, (shotTimeMillis - startTime)/1000.0, currentSplit);
                shotCount++;

                // Force immediate update of timing screen after shot
                redrawMenu = true;
                displayTimingScreen(currentElapsedTime, shotCount, currentSplit);
                lastDisplayUpdateTime = currentTime;

                checkingForRecoil = false; // Stop checking for recoil for this shot
                lastSoundPeakTime = 0;

                // Check if max shots reached
                if (shotCount >= currentMaxShots) {
                    Serial.println("Max shots reached. Stopping timer.");
                    setState(LIVE_FIRE_STOPPED); // Use common stopped state
                    StickCP2.Lcd.fillScreen(BLACK);
                    displayStoppedScreen();
                    if (shotCount > 0) playSuccessBeeps(); else playUnsuccessBeeps();
                    return; // Exit handler
                }
            } // end else (not an ignored first shot)
        }
        // Check if recoil window expired without meeting threshold
        else if (currentTime - lastSoundPeakTime > RECOIL_DETECTION_WINDOW_MS) {
            Serial.println("Noisy: Recoil window expired. False alarm.");
            checkingForRecoil = false; // Stop checking
            lastSoundPeakTime = 0;
        }
    }

    // Reset mic peak if not waiting for recoil (to avoid stale high values)
    if (!checkingForRecoil) {
        micPeakRMS.resetPeak();
    }

    // Check for Stop Button Press (Manual Stop - Use wasClicked)
    if (currentState == NOISY_RANGE_TIMING && StickCP2.BtnA.wasClicked()) {
        Serial.println("Stop button pressed manually (Noisy Range).");
        setState(LIVE_FIRE_STOPPED); // Use common stopped state
        StickCP2.Lcd.fillScreen(BLACK);
        displayStoppedScreen();
        if (shotCount > 0) playSuccessBeeps(); else playUnsuccessBeeps();
        return; // Exit handler
    }

    // Check for Timeout
    if (currentState == NOISY_RANGE_TIMING) {
        unsigned long timeSinceEvent = (shotCount == 0) ? (currentTime - startTime) : (currentTime - lastShotTimestamp);
        bool hasStarted = (startTime > 0); // Timer considered started if beep happened

        if (hasStarted && timeSinceEvent > TIMEOUT_DURATION_MS) {
            Serial.printf("Timeout reached (%s). Stopping timer (Noisy Range).\n", (shotCount == 0) ? "no shots" : "after last shot");
            setState(LIVE_FIRE_STOPPED); // Use common stopped state
            StickCP2.Lcd.fillScreen(BLACK);
            displayStoppedScreen();
             if (shotCount > 0) playSuccessBeeps(); else playUnsuccessBeeps();
        }
    }
}

// void handleConfirmFormatInput() { // <-- REMOVED function
//     // ... (code removed)
// }
