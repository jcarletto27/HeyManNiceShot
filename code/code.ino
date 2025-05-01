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
const unsigned long BEEP_NOTE_DURATION_MS = 100;  // Duration for beep notes
const unsigned long BEEP_NOTE_DELAY_MS = 50;      // Delay between beep notes
const unsigned long BATTERY_CHECK_INTERVAL_MS = 60000; // Check battery every 60 seconds
const float BATTERY_LOW_PERCENTAGE = 0.78;        // 78% threshold for low battery warning
const int MAX_SHOTS_LIMIT = 20; // Absolute maximum array size
const int MENU_ITEM_HEIGHT_LANDSCAPE = 25;
const int MENU_ITEM_HEIGHT_PORTRAIT = 18; // Smaller height for portrait text size 1
const int MENU_ITEMS_PER_SCREEN_LANDSCAPE = 4;
const int MENU_ITEMS_PER_SCREEN_PORTRAIT = 6; // Can fit more vertically with text size 1
const unsigned long POST_BEEP_DELAY_MS = 100; // Small delay after start beep to prevent false trigger
const int MAX_FILES_LIST = 20; // Max files to list on status screen
const unsigned long BOOT_JPG_FRAME_DELAY_MS = 100; // Delay between JPG frames <-- CHANGED
const int MAX_BOOT_JPG_FRAMES = 150; // Stop checking after this many frames <-- CHANGED
const unsigned long MESSAGE_DISPLAY_MS = 2000; // How long to show format success/fail message

// --- NVS Keys ---
const char* NVS_NAMESPACE = "ShotTimer";
const char* KEY_MAX_SHOTS = "maxShots";
const char* KEY_BEEP_DUR = "beepDur";
const char* KEY_BEEP_HZ = "beepHz";
const char* KEY_SHOT_THRESH = "shotThresh";
const char* KEY_DF_BEEP_CNT = "dfBeepCnt";
const char* KEY_NR_RECOIL = "nrRecoil";
const char* KEY_PEAK_BATT = "peakBatt";
const char* KEY_ROTATION = "rotation";

// --- Timer States ---
enum TimerState {
    BOOT_SCREEN,
    BOOT_JPG_SEQUENCE, // State for JPG sequence
    MODE_SELECTION,
    LIVE_FIRE_READY,
    LIVE_FIRE_GET_READY,
    LIVE_FIRE_TIMING,
    LIVE_FIRE_STOPPED,
    DRY_FIRE_MODE,
    NOISY_RANGE_MODE,
    SETTINGS_MENU_MAIN,
    SETTINGS_MENU_GENERAL,
    SETTINGS_MENU_DRYFIRE,
    SETTINGS_MENU_NOISY,
    DEVICE_STATUS,
    LIST_FILES,
    EDIT_SETTING,
    CALIBRATE_THRESHOLD,
    CALIBRATE_RECOIL,
    CONFIRM_FORMAT_LITTLEFS
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
    EDIT_RECOIL_THRESHOLD,
    EDIT_ROTATION
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
float recoilThreshold = 1.5;
int screenRotationSetting = 3;

// Shot Data Arrays
int shotCount = 0;
unsigned long shotTimestamps[MAX_SHOTS_LIMIT];
float splitTimes[MAX_SHOTS_LIMIT];
unsigned long lastShotTimestamp = 0;
unsigned long lastDetectionTime = 0;

// Menu Variables
int currentMenuSelection = 0;
int menuScrollOffset = 0;
int settingsMenuLevel = 0;
unsigned long btnTopPressTime = 0;
bool btnTopHeld = false;
bool redrawMenu = true;

// Editing Variables
EditableSetting settingBeingEdited = EDIT_NONE;
int editingIntValue = 0;
unsigned long editingULongValue = 0;
float editingFloatValue = 0.0;
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

// Buzzer Pin (External)
#define BUZZER_PIN 25

// Boot Sequence Variables
int currentJpgFrame = 1;
bool filesystem_ok_for_boot = false; // Flag if LittleFS mounted correctly

// --- Function Prototypes ---
// (Keep all existing prototypes except printLittleFSFiles)
void displayBootScreen(const char* line1a, const char* line1b, const char* line2);
void displayMenu(const char* title, const char* items[], int count, int selection, int scrollOffset);
void displayTimingScreen(float elapsedTime, int count, float lastSplit);
void displayStoppedScreen();
void displayEditScreen();
void displayCalibrationScreen(const char* title, float peakValue, const char* unit);
void displayDeviceStatusScreen();
void displayListFilesScreen();
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
void handleConfirmFormatInput();
void setState(TimerState newState);
String getUpButtonLabel();
String getDownButtonLabel();
// void printLittleFSFiles(); // <-- REMOVED prototype


// --- Setup ---
void setup() {
    Serial.begin(115200);
    Serial.println("\n=====================================");
    Serial.println(" Hey Man, Nice Shot... Timer - Booting");
    Serial.println("=====================================");

    StickCP2.begin(); // Initialize StickCP2 object

    Serial.println("Initializing Preferences (NVS)...");
    preferences.begin(NVS_NAMESPACE, false);
    loadSettings(); // Load saved settings (includes rotation)

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
    // Use LittleFS.begin() without true to avoid auto-formatting
    if(!LittleFS.begin()){
        Serial.println("!!! LittleFS Mount Failed! Check partition scheme or format manually.");
        displayBootScreen("ERROR", "", "FS Failed!"); // Generic FS message
        playUnsuccessBeeps();
        delay(2000); // Show error briefly
        filesystem_ok_for_boot = false; // Disable JPG sequence if FS fails
    } else {
        Serial.println("LittleFS Mounted.");
        filesystem_ok_for_boot = true; // Filesystem is OK
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
    if (filesystem_ok_for_boot) {
        Serial.println("Starting JPG Boot Sequence...");
        setState(BOOT_JPG_SEQUENCE);
        currentJpgFrame = 1; // Start from frame 1.jpg
        StickCP2.Lcd.fillScreen(BLACK); // Clear for sequence
    } else {
        Serial.println("Filesystem failed, skipping boot sequence.");
        delay(1000); // Show "Init Complete" longer if no sequence
        // printLittleFSFiles(); // <-- REMOVED call
        setState(MODE_SELECTION); // Go directly to mode selection
        currentMenuSelection = (int)currentMode;
        menuScrollOffset = 0;
        StickCP2.Lcd.fillScreen(BLACK); // Clear screen
    }
}

// --- Main Loop ---
void loop() {
    StickCP2.update(); // Update StickCP2 components

    unsigned long currentTime = millis();

    // --- Update Mic Library (if needed by current state) ---
    bool micUpdateNeeded = (currentState == LIVE_FIRE_TIMING ||
                            currentState == CALIBRATE_THRESHOLD ||
                            currentState == NOISY_RANGE_MODE);
    if (micUpdateNeeded) {
        micPeakRMS.update();
    }

    // --- Periodic Battery Check ---
    if (currentTime - lastBatteryCheckTime > BATTERY_CHECK_INTERVAL_MS) {
        checkBattery();
        // Force redraw if status screen is active and battery warning state changed
        if (currentState == DEVICE_STATUS || currentState == LIST_FILES) {
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
                currentState != SETTINGS_MENU_DRYFIRE && currentState != SETTINGS_MENU_NOISY &&
                currentState != DEVICE_STATUS && currentState != LIST_FILES &&
                currentState != EDIT_SETTING && currentState != CALIBRATE_THRESHOLD &&
                currentState != CALIBRATE_RECOIL && currentState != CONFIRM_FORMAT_LITTLEFS && // Don't interrupt format confirm
                currentState != BOOT_JPG_SEQUENCE) // Don't interrupt boot sequence
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

                // Construct filename (e.g., /1.jpg, /2.jpg)
                char jpgFilename[12];
                sprintf(jpgFilename, "/%d.jpg", currentJpgFrame);

                Serial.printf("Checking for: %s\n", jpgFilename);

                if (LittleFS.exists(jpgFilename) && currentJpgFrame <= MAX_BOOT_JPG_FRAMES) {
                    Serial.printf("Opening %s\n", jpgFilename);
                    File jpgFile = LittleFS.open(jpgFilename, FILE_READ);

                    if (!jpgFile) {
                        Serial.printf("!!! Failed to open %s\n", jpgFilename);
                        // Skip to end if file open fails
                        // printLittleFSFiles(); // <-- REMOVED call
                        setState(MODE_SELECTION);
                        currentMenuSelection = (int)currentMode;
                        menuScrollOffset = 0;
                        StickCP2.Lcd.fillScreen(BLACK);
                        break; // Exit case
                    }

                    Serial.printf("Displaying %s\n", jpgFilename);
                    // Display the JPG from the File stream, centered and scaled to fit
                    bool success = StickCP2.Lcd.drawJpg(&jpgFile, // Pass Stream pointer
                                                        0, 0,     // x, y position (relative to datum)
                                                        StickCP2.Lcd.width(), StickCP2.Lcd.height(), // Max width/height
                                                        0, 0,     // Offset x, y (within image)
                                                        0.0f, 0.0f, // Scale x, y (0.0 means scale to fit bounds)
                                                        datum_t::middle_center); // Datum (center image on x,y)

                    jpgFile.close(); // IMPORTANT: Close the file handle

                    if (!success) {
                        Serial.printf("!!! Failed to draw %s\n", jpgFilename);
                        // Decide how to handle draw failure - skip to end?
                        // printLittleFSFiles(); // <-- REMOVED call
                        setState(MODE_SELECTION);
                        currentMenuSelection = (int)currentMode;
                        menuScrollOffset = 0;
                        StickCP2.Lcd.fillScreen(BLACK);
                        break; // Exit case
                    }

                    currentJpgFrame++; // Move to next frame
                    delay(BOOT_JPG_FRAME_DELAY_MS); // Wait before showing next frame

                } else {
                    // File doesn't exist or max frames reached
                    if (currentJpgFrame > MAX_BOOT_JPG_FRAMES) {
                       Serial.println("Max boot frames reached.");
                    } else {
                       Serial.printf("%s not found.\n", jpgFilename);
                    }
                    Serial.println("Boot sequence finished.");
                    // printLittleFSFiles(); // <-- REMOVED call
                    setState(MODE_SELECTION);
                    currentMenuSelection = (int)currentMode;
                    menuScrollOffset = 0;
                    StickCP2.Lcd.fillScreen(BLACK); // Clear screen after sequence
                }
            }
            break;

        case MODE_SELECTION:
            handleModeSelectionInput();
            break;

        // --- Live Fire Mode States ---
        case LIVE_FIRE_READY:
            if (redrawMenu) {
                displayTimingScreen(0.0, 0, 0.0);
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
                delay(1000);
            }
            break;

        case LIVE_FIRE_GET_READY:
            Serial.println("Generating start beep...");
            playTone(currentBeepToneHz, currentBeepDuration);
            delay(POST_BEEP_DELAY_MS); // Add delay after beep
            micPeakRMS.resetPeak();    // Reset peak immediately after beep/delay
            Serial.println("Beep finished, peak reset. Starting timer.");
            startTime = millis(); // Use current time AFTER beep/delay
            resetShotData(); // Resets counts and overall peak level
            lastDisplayUpdateTime = 0;
            setState(LIVE_FIRE_TIMING);
            break;

        case LIVE_FIRE_TIMING:
            {
                if (currentState != LIVE_FIRE_TIMING) break;

                float currentElapsedTime = (currentTime - startTime) / 1000.0;
                currentCyclePeakRMS = micPeakRMS.getPeakRMS();

                if (currentCyclePeakRMS > peakRMSOverall) {
                    peakRMSOverall = currentCyclePeakRMS;
                }

                if (currentTime - lastDisplayUpdateTime >= DISPLAY_UPDATE_INTERVAL_MS) {
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
                } // End shot detection

                micPeakRMS.resetPeak();

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
                    bool hasStarted = (shotCount > 0 || startTime > 0); // Timer considered started if beep happened

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
                setState(LIVE_FIRE_READY);
                StickCP2.Lcd.fillScreen(BLACK);
            }
            break;

        // --- Placeholder Modes ---
        case DRY_FIRE_MODE:
            if (redrawMenu) {
                StickCP2.Lcd.fillScreen(BLACK);
                StickCP2.Lcd.setTextDatum(MC_DATUM);
                StickCP2.Lcd.drawString("Dry Fire Mode", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()/2 - 10);
                StickCP2.Lcd.setTextSize(1);
                StickCP2.Lcd.drawString("(Not Implemented)", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()/2 + 10);
                StickCP2.Lcd.drawString("Hold Front to Return", StickCP2.Lcd.width()/2, StickCP2.Lcd.height() - 20);
                drawLowBatteryIndicator();
                redrawMenu = false;
            }
            if(StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
                setState(MODE_SELECTION);
                 currentMenuSelection = (int)currentMode;
                 menuScrollOffset = 0;
                 StickCP2.Lcd.fillScreen(BLACK);
            }
            break;

        case NOISY_RANGE_MODE:
            if (redrawMenu) {
                StickCP2.Lcd.fillScreen(BLACK);
                StickCP2.Lcd.setTextDatum(MC_DATUM);
                StickCP2.Lcd.drawString("Noisy Range Mode", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()/2 - 10);
                StickCP2.Lcd.setTextSize(1);
                StickCP2.Lcd.drawString("(Not Implemented)", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()/2 + 10);
                StickCP2.Lcd.drawString("Hold Front to Return", StickCP2.Lcd.width()/2, StickCP2.Lcd.height() - 20);
                drawLowBatteryIndicator();
                redrawMenu = false;
            }
            if(StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
                setState(MODE_SELECTION);
                 currentMenuSelection = (int)currentMode;
                 menuScrollOffset = 0;
                 StickCP2.Lcd.fillScreen(BLACK);
            }
            break;

        // --- Settings States ---
        case SETTINGS_MENU_MAIN:
        case SETTINGS_MENU_GENERAL:
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

        case CONFIRM_FORMAT_LITTLEFS: // <-- RENAMED case
            handleConfirmFormatInput();
            break;

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
        redrawMenu = true;
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

    StickCP2.Lcd.setTextDatum(TL_DATUM);
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

        String itemText = items[i];
        // Display current value next to the setting name
        if (settingsMenuLevel > 0 && strcmp(items[i], "Back") != 0 &&
            strcmp(items[i], "Calibrate Thresh.") != 0 &&
            strcmp(items[i], "Calibrate Recoil") != 0 &&
            strcmp(items[i], "Device Status") != 0 &&
            strcmp(items[i], "List Files") != 0 &&
            strcmp(items[i], "Format LittleFS") != 0) // Don't show value for Format <-- UPDATED
        {
            itemText += ": ";
            if (strcmp(items[i], "Max Shots") == 0) itemText += currentMaxShots;
            else if (strcmp(items[i], "Beep Duration") == 0) itemText += currentBeepDuration;
            else if (strcmp(items[i], "Beep Tone") == 0) itemText += currentBeepToneHz;
            else if (strcmp(items[i], "Shot Threshold") == 0) itemText += shotThresholdRms;
            else if (strcmp(items[i], "Par Beep Count") == 0) itemText += dryFireParBeepCount;
            else if (strcmp(items[i], "Recoil Threshold") == 0) itemText += String(recoilThreshold, 1);
            else if (strcmp(items[i], "Screen Rotation") == 0) itemText += screenRotationSetting;
        }

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

    drawLowBatteryIndicator();
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
            case MODE_DRY_FIRE:    setState(DRY_FIRE_MODE); break;
            case MODE_NOISY_RANGE: setState(NOISY_RANGE_MODE); break;
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
    const char** items = nullptr;
    int itemCount = 0;
    int rotation = StickCP2.Lcd.getRotation();
    int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;


    // Add "Format LittleFS" to main menu
    static const char* mainItems[] = {"General", "Dry Fire", "Noisy Range", "Device Status", "List Files", "Format LittleFS", "Save & Exit"}; // <-- UPDATED
    static const char* generalItems[] = {"Max Shots", "Beep Duration", "Beep Tone", "Shot Threshold", "Screen Rotation", "Calibrate Thresh.", "Back"};
    static const char* dryFireItems[] = {"Par Beep Count", "Back"};
    static const char* noisyItems[] = {"Recoil Threshold", "Calibrate Recoil", "Back"};

    switch (settingsMenuLevel) {
        case 0: items = mainItems; itemCount = sizeof(mainItems) / sizeof(mainItems[0]); title = "Settings"; break;
        case 1: items = generalItems; itemCount = sizeof(generalItems) / sizeof(generalItems[0]); title = "General Settings"; break;
        case 2: items = dryFireItems; itemCount = sizeof(dryFireItems) / sizeof(dryFireItems[0]); title = "Dry Fire Settings"; break;
        case 3: items = noisyItems; itemCount = sizeof(noisyItems) / sizeof(noisyItems[0]); title = "Noisy Range Settings"; break;
    }

    // Adjust scroll offset
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
         } else {
             Serial.println("Returning to Main Settings Menu");
             settingsMenuLevel = 0; currentMenuSelection = 0; menuScrollOffset = 0; redrawMenu = true;
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
            else if (strcmp(items[currentMenuSelection], "Format LittleFS") == 0) { // <-- UPDATED Action
                setState(CONFIRM_FORMAT_LITTLEFS); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            }
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
            } else if (strcmp(editingSettingName, "Beep Duration") == 0) {
                settingBeingEdited = EDIT_BEEP_DURATION; editingULongValue = currentBeepDuration; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Beep Tone") == 0) {
                settingBeingEdited = EDIT_BEEP_TONE; editingIntValue = currentBeepToneHz; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Shot Threshold") == 0) {
                settingBeingEdited = EDIT_SHOT_THRESHOLD; editingIntValue = shotThresholdRms; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Screen Rotation") == 0) {
                settingBeingEdited = EDIT_ROTATION; editingIntValue = screenRotationSetting; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Calibrate Thresh.") == 0) {
                setState(CALIBRATE_THRESHOLD); peakRMSOverall = 0; micPeakRMS.resetPeak(); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Back") == 0) {
                settingsMenuLevel = 0; currentMenuSelection = 0; menuScrollOffset = 0;
            }
        }
        else if (settingsMenuLevel == 2) { // Dry Fire Settings
            editingSettingName = items[currentMenuSelection];
            stateBeforeEdit = SETTINGS_MENU_DRYFIRE;
            if (strcmp(editingSettingName, "Par Beep Count") == 0) {
                settingBeingEdited = EDIT_PAR_BEEP_COUNT; editingIntValue = dryFireParBeepCount; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Back") == 0) {
                settingsMenuLevel = 0; currentMenuSelection = 0; menuScrollOffset = 0;
            }
        }
        else if (settingsMenuLevel == 3) { // Noisy Range Settings
            editingSettingName = items[currentMenuSelection];
            stateBeforeEdit = SETTINGS_MENU_NOISY;
            if (strcmp(editingSettingName, "Recoil Threshold") == 0) {
                settingBeingEdited = EDIT_RECOIL_THRESHOLD; editingFloatValue = recoilThreshold; setState(EDIT_SETTING); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Calibrate Recoil") == 0) {
                setState(CALIBRATE_RECOIL); needsActionRedraw = false; StickCP2.Lcd.fillScreen(BLACK);
            } else if (strcmp(editingSettingName, "Back") == 0) {
                settingsMenuLevel = 0; currentMenuSelection = 0; menuScrollOffset = 0;
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

    if (upPressed) { // Up Action
        valueChanged = true;
        if(rotation == 3) Serial.println("Bottom (BtnPWR) Click Detected for Edit Up (Rotation 3)");
        switch(settingBeingEdited) {
            case EDIT_MAX_SHOTS: editingIntValue = min(editingIntValue + 1, MAX_SHOTS_LIMIT); break;
            case EDIT_BEEP_DURATION: editingULongValue = min(editingULongValue + 50, 2000UL); break;
            case EDIT_BEEP_TONE: editingIntValue = min(editingIntValue + 100, 8000); break;
            case EDIT_SHOT_THRESHOLD: editingIntValue = min(editingIntValue + 500, 32000); break;
            case EDIT_PAR_BEEP_COUNT: editingIntValue = min(editingIntValue + 1, 10); break;
            case EDIT_RECOIL_THRESHOLD: editingFloatValue = min(editingFloatValue + 0.1f, 5.0f); break;
            case EDIT_ROTATION: editingIntValue = (editingIntValue + 1) % 4; break; // Cycle 0-3
            default: valueChanged = false; break;
        }
        // Apply rotation change immediately
        if (settingBeingEdited == EDIT_ROTATION) {
            StickCP2.Lcd.setRotation(editingIntValue);
            redrawMenu = true; // Force redraw after rotation change
        }
    }
    if (downPressed) { // Down Action
        valueChanged = true;
        if(rotation != 3) Serial.println("Bottom (BtnPWR) Click Detected for Edit Down");
        else Serial.println("Top (BtnB) Click Detected for Edit Down (Rotation 3)");
        switch(settingBeingEdited) {
            case EDIT_MAX_SHOTS: editingIntValue = max(editingIntValue - 1, 1); break;
            case EDIT_BEEP_DURATION: editingULongValue = (editingULongValue <= 50) ? 50 : editingULongValue - 50; break;
            case EDIT_BEEP_TONE: editingIntValue = max(editingIntValue - 100, 500); break;
            case EDIT_SHOT_THRESHOLD: editingIntValue = max(editingIntValue - 500, 100); break;
            case EDIT_PAR_BEEP_COUNT: editingIntValue = max(editingIntValue - 1, 1); break;
            case EDIT_RECOIL_THRESHOLD: editingFloatValue = max(editingFloatValue - 0.1f, 0.5f); break;
            case EDIT_ROTATION: editingIntValue = (editingIntValue - 1 + 4) % 4; break; // Cycle 0-3
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
            case EDIT_RECOIL_THRESHOLD: recoilThreshold = editingFloatValue; break;
            case EDIT_ROTATION: screenRotationSetting = editingIntValue; break; // Save confirmed rotation
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
         // Clear larger area for Font 7
         StickCP2.Lcd.fillRect(0, StickCP2.Lcd.height()/2 - 25, StickCP2.Lcd.width(), 50, BLACK);
    } else {
        StickCP2.Lcd.fillScreen(BLACK);
        StickCP2.Lcd.setTextDatum(TC_DATUM);
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(2);
        StickCP2.Lcd.drawString(editingSettingName, StickCP2.Lcd.width() / 2, 15);
        // Instructions only need to be drawn once
        StickCP2.Lcd.setTextDatum(BC_DATUM);
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(1);
        // Use helper functions for dynamic button labels
        StickCP2.Lcd.drawString(getUpButtonLabel() + "=Up / " + getDownButtonLabel() + "=Down", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 25);
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
        case EDIT_RECOIL_THRESHOLD: StickCP2.Lcd.drawFloat(editingFloatValue, 1, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
        case EDIT_ROTATION: StickCP2.Lcd.drawNumber(editingIntValue, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); break;
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
    float peakValue = 0.0;
    const char* title = "Calibrating...";
    const char* unit = "";
    bool peakChanged = false;
    int rotation = StickCP2.Lcd.getRotation(); // Get rotation for menu item calculation
    int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;


    // --- Get Current Values ---
    if (calibrationType == CALIBRATE_THRESHOLD) {
        title = "Calibrate Threshold";
        unit = "RMS";
        currentValue = micPeakRMS.getPeakRMS();
        if (currentValue > peakRMSOverall) { peakRMSOverall = currentValue; }
        peakValue = peakRMSOverall;
        // Check if peak changed significantly enough for redraw
        if (abs(peakValue - prevPeakValue) > 0.1) {
            peakChanged = true;
            prevPeakValue = peakValue;
        }
        micPeakRMS.resetPeak();
    } else if (calibrationType == CALIBRATE_RECOIL) {
        title = "Calibrate Recoil";
        unit = "G";
        // TODO: Read IMU data
        currentValue = 0.0; peakValue = 0.0;
        // if (peak changed) { peakChanged = true; prevPeakValue = peakValue; }
    }

    // --- Redraw Screen Only If Needed ---
    if (redrawMenu || peakChanged) {
        // Only clear the peak value area if just the peak changed
        if (!redrawMenu && peakChanged) {
             StickCP2.Lcd.fillRect(0, StickCP2.Lcd.height() / 2 - 15, StickCP2.Lcd.width(), 30, BLACK); // Clear old peak area
        } else { // Full redraw on entering state (redrawMenu == true)
            StickCP2.Lcd.fillScreen(BLACK);
            StickCP2.Lcd.setTextDatum(TC_DATUM); StickCP2.Lcd.setTextFont(0); StickCP2.Lcd.setTextSize(2);
            StickCP2.Lcd.drawString(title, StickCP2.Lcd.width() / 2, 10);
            // Simplified Instructions
            StickCP2.Lcd.setTextDatum(BC_DATUM); StickCP2.Lcd.setTextFont(0); StickCP2.Lcd.setTextSize(1);
            StickCP2.Lcd.drawString("Press Front=Save Peak", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 25);
            StickCP2.Lcd.drawString("Hold Front=Cancel", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 10);
            if (calibrationType == CALIBRATE_RECOIL) {
                 StickCP2.Lcd.drawString("(Not Implemented)", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()-45);
            }
        }

        // Draw Peak Value (always update this part if peakChanged or redrawMenu)
        StickCP2.Lcd.setTextDatum(MC_DATUM); StickCP2.Lcd.setTextFont(7); StickCP2.Lcd.setTextSize(1);
        String peakStr = "PEAK: " + String((int)peakValue);
        StickCP2.Lcd.drawString(peakStr, StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2); // Centered vertically

        drawLowBatteryIndicator();
        redrawMenu = false; // Redraw done
    }

    // Input Handling
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) { // Cancel
        Serial.println("Calibration cancelled.");
        stateBeforeEdit = (calibrationType == CALIBRATE_THRESHOLD) ? SETTINGS_MENU_GENERAL : SETTINGS_MENU_NOISY;
        setState(stateBeforeEdit);
        // Adjust selection index based on removed "Show Boot GIF" item
        currentMenuSelection = (calibrationType == CALIBRATE_THRESHOLD) ? 5 : 1; // Index of Calibrate items
        menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
        StickCP2.Lcd.fillScreen(BLACK);
        playUnsuccessBeeps();
    } else if (StickCP2.BtnA.wasClicked()) { // Save
        if (calibrationType == CALIBRATE_THRESHOLD) {
            Serial.printf("Saving peak threshold value: %.0f\n", peakValue);
            shotThresholdRms = (int)peakValue;
            stateBeforeEdit = SETTINGS_MENU_GENERAL;
            setState(stateBeforeEdit);
            currentMenuSelection = 5; // Index of Calibrate Thresh
            menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
            StickCP2.Lcd.fillScreen(BLACK);
            playSuccessBeeps();
        } else if (calibrationType == CALIBRATE_RECOIL) {
            Serial.printf("Saving peak recoil value: %.2f\n", peakValue);
            // recoilThreshold = peakValue; // Uncomment when implemented
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
    if (redrawMenu) { // <-- REMOVED time-based check
        displayDeviceStatusScreen();
        redrawMenu = false;
        // lastStatusUpdateTime = millis(); // No longer needed
    }

    // Check for return (Hold Front)
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
        Serial.println("Returning to Main Settings Menu from Status");
        setState(SETTINGS_MENU_MAIN); // Go back to main settings
        currentMenuSelection = 3; // Select "Device Status" item (adjust index if menu changes)
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
    Serial.println("Shot data and library peak reset.");
    for (int i = 0; i < MAX_SHOTS_LIMIT; ++i) {
        shotTimestamps[i] = 0;
        splitTimes[i] = 0.0;
    }
}

/**
 * @brief Updates the display during the TIMING state (Cleaned up).
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

    if (!updateNeeded) {
        return;
    }

    // --- Perform Selective Redraw ---
    StickCP2.Lcd.setTextColor(WHITE, BLACK);
    StickCP2.Lcd.setTextDatum(TL_DATUM);

    // Update Time
    if (redrawMenu || abs(elapsedTime - prevElapsedTime) > 0.01) {
        StickCP2.Lcd.setTextFont(7);
        StickCP2.Lcd.setTextSize(1);
        int time_y = (rotation % 2 == 0) ? 20 : 15;
        StickCP2.Lcd.fillRect(5, time_y, StickCP2.Lcd.width() - 10 , StickCP2.Lcd.fontHeight(7) + 4, BLACK);
        StickCP2.Lcd.setCursor(10, time_y);
        StickCP2.Lcd.printf("%.2f", elapsedTime);
        prevElapsedTime = elapsedTime;
    }

    // Update Shot Count & Split Time
    int shots_y = (rotation % 2 == 0) ? 80 : 75;
    int split_y = shots_y + ((rotation % 2 == 0) ? 20 : 25);
    int text_size = (rotation % 2 == 0) ? 1 : 2;
    int line_h = (text_size == 1) ? 14 : 20;

    if (redrawMenu || count != prevCount) {
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(text_size);
        StickCP2.Lcd.fillRect(10, shots_y, StickCP2.Lcd.width() - 20, line_h, BLACK);
        StickCP2.Lcd.setCursor(10, shots_y);
        StickCP2.Lcd.printf("Shots: %d/%d", count, currentMaxShots);
        prevCount = count;
    }

    if (redrawMenu || abs(lastSplit - prevLastSplit) > 0.01 || count != prevCount) {
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(text_size);
        StickCP2.Lcd.fillRect(10, split_y, StickCP2.Lcd.width() - 20, line_h, BLACK);
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
 */
void displayStoppedScreen() {
    StickCP2.Lcd.fillScreen(BLACK);
    StickCP2.Lcd.setTextFont(0);
    StickCP2.Lcd.setTextColor(WHITE, BLACK);
    StickCP2.Lcd.setTextDatum(TL_DATUM);
    int rotation = StickCP2.Lcd.getRotation();
    int text_size = (rotation % 2 == 0) ? 1 : 2;
    int line_h = (text_size == 1) ? 18 : 30;
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

    StickCP2.Lcd.setTextSize(1);
    StickCP2.Lcd.setCursor(30, StickCP2.Lcd.height() - 20);
    StickCP2.Lcd.print("Press Front to Reset");

    drawLowBatteryIndicator();
}

/**
 * @brief Plays a tone on the external buzzer pin.
 */
void playTone(int freq, int duration) {
    if (freq > 0) {
        tone(BUZZER_PIN, freq, duration);
    } else {
        delay(duration);
    }
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
    recoilThreshold = preferences.getFloat(KEY_NR_RECOIL, 1.5);
    screenRotationSetting = preferences.getInt(KEY_ROTATION, 3); // Default to 3
    if (screenRotationSetting < 0 || screenRotationSetting > 3) screenRotationSetting = 3; // Validate

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
    preferences.putFloat(KEY_NR_RECOIL, recoilThreshold);
    preferences.putInt(KEY_ROTATION, screenRotationSetting);
    Serial.println("Settings saved.");
}

/**
 * @brief Saves the peak battery voltage to NVS.
 */
void savePeakVoltage(float voltage) {
    preferences.putFloat(KEY_PEAK_BATT, voltage);
}

// /**
//  * @brief Prints the list of files in LittleFS root directory to Serial.
//  */
// void printLittleFSFiles() { // <-- REMOVED function
//     // ... (code removed)
// }


// --- Function to handle LIST_FILES state ---
void handleListFilesInput() {
    int rotation = StickCP2.Lcd.getRotation();
    int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT + 2 : MENU_ITEMS_PER_SCREEN_LANDSCAPE + 1;

    // Populate file list only when entering the state
    if (redrawMenu) {
        fileListCount = 0;
        File root = LittleFS.open("/"); // <-- CHANGED from SPIFFS
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
         currentMenuSelection = 4; // Select "List Files" item (adjust index if menu changes)
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
    StickCP2.Lcd.drawString("LittleFS Files", StickCP2.Lcd.width() / 2, 10); // <-- UPDATED Text

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
        StickCP2.Lcd.print("LittleFS error."); // <-- UPDATED Text
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

/**
 * @brief Handles input and display for the Format LittleFS confirmation screen.
 */
void handleConfirmFormatInput() { // <-- RENAMED function
    int rotation = StickCP2.Lcd.getRotation();

    if (redrawMenu) {
        StickCP2.Lcd.fillScreen(BLACK);
        StickCP2.Lcd.setTextDatum(MC_DATUM);
        StickCP2.Lcd.setTextFont(0);
        StickCP2.Lcd.setTextSize(2);
        StickCP2.Lcd.drawString("Format LittleFS?", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2 - 20); // <-- UPDATED Text
        StickCP2.Lcd.setTextSize(1);
        StickCP2.Lcd.drawString("ALL FILES WILL BE ERASED!", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2 + 5);
        StickCP2.Lcd.setTextDatum(BC_DATUM);
        StickCP2.Lcd.drawString("Press Front=OK / Hold=Cancel", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() - 10);
        drawLowBatteryIndicator();
        redrawMenu = false;
    }

    // Cancel (Hold Front)
    if (StickCP2.BtnA.pressedFor(LONG_PRESS_DURATION_MS)) {
        Serial.println("Format LittleFS cancelled."); // <-- UPDATED Text
        setState(SETTINGS_MENU_MAIN); // Go back to main settings
        currentMenuSelection = 5; // Index of "Format LittleFS"
        int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;
        menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
        StickCP2.Lcd.fillScreen(BLACK); // Explicit clear on cancel
        playUnsuccessBeeps();
        return;
    }

    // Confirm (Short Press Front)
    if (StickCP2.BtnA.wasClicked()) {
        Serial.println("Formatting LittleFS..."); // <-- UPDATED Text
        StickCP2.Lcd.fillScreen(BLACK);
        StickCP2.Lcd.setTextDatum(MC_DATUM);
        StickCP2.Lcd.setTextSize(2);
        StickCP2.Lcd.drawString("Formatting...", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2);

        bool formatSuccess = LittleFS.format(); // Perform the format <-- CHANGED from SPIFFS

        StickCP2.Lcd.fillScreen(BLACK); // Clear "Formatting..." message
        if (formatSuccess) {
            Serial.println("LittleFS Format Successful."); // <-- UPDATED Text
            StickCP2.Lcd.drawString("Format OK!", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2);
            playSuccessBeeps();
        } else {
            Serial.println("!!! LittleFS Format Failed!"); // <-- UPDATED Text
            StickCP2.Lcd.drawString("Format FAILED!", StickCP2.Lcd.width() / 2, StickCP2.Lcd.height() / 2);
            playUnsuccessBeeps();
        }
        delay(MESSAGE_DISPLAY_MS); // Show message briefly

        // Return to main settings menu
        setState(SETTINGS_MENU_MAIN);
        currentMenuSelection = 5; // Index of "Format LittleFS"
        int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;
        menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
        StickCP2.Lcd.fillScreen(BLACK); // Explicit clear on return
        return;
    }

    // Cancel with other buttons (Top/Bottom)
    bool upPressed = (rotation == 3) ? M5.BtnPWR.wasClicked() : StickCP2.BtnB.wasClicked();
    bool downPressed = (rotation == 3) ? StickCP2.BtnB.wasClicked() : M5.BtnPWR.wasClicked();
    if (upPressed || downPressed) {
        Serial.println("Format LittleFS cancelled (Side button)."); // <-- UPDATED Text
        setState(SETTINGS_MENU_MAIN); // Go back to main settings
        currentMenuSelection = 5; // Index of "Format LittleFS"
        int itemsPerScreen = (rotation % 2 == 0) ? MENU_ITEMS_PER_SCREEN_PORTRAIT : MENU_ITEMS_PER_SCREEN_LANDSCAPE;
        menuScrollOffset = max(0, currentMenuSelection - itemsPerScreen + 1);
        StickCP2.Lcd.fillScreen(BLACK); // Explicit clear on cancel
        playUnsuccessBeeps();
        return;
    }
}
