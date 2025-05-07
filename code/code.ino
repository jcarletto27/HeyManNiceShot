// External Libraries
#include <M5StickCPlus2.h>
#include "M5MicPeakRMS.h"      // Custom library for microphone
#include "BluetoothA2DPSource.h"
#include <ESP32BluetoothScanner.h>
#include <Preferences.h>
#include <LittleFS.h>
#include <cmath>        // For abs, sin
#include <float.h>      // For FLT_MAX
#include <vector>       // For std::vector
#include "driver/rtc_io.h" // For deep/light sleep wakeup
#include "esp_sleep.h"     // For light sleep functions
#include <freertos/FreeRTOS.h> // Added for FreeRTOS
#include <freertos/task.h>
#include <freertos/queue.h>

// Project Headers
#include "config.h"
#include "globals.h"
#include "display_utils.h"
#include "input_handler.h"
#include "timer_modes.h"
#include "audio_utils.h"
#include "bluetooth_utils.h"
#include "nvs_utils.h"
#include "system_utils.h"


// --- Global Variable Definitions ---
TimerState currentState = BOOT_SCREEN;
TimerState previousState = BOOT_SCREEN;
TimerState stateBeforeEdit = SETTINGS_MENU_MAIN;
TimerState stateBeforeScan = SETTINGS_MENU_BLUETOOTH;
OperatingMode currentMode = MODE_LIVE_FIRE;
unsigned long startTime = 0;
unsigned long lastDisplayUpdateTime = 0;
unsigned long lastActivityTime = 0;

int currentMaxShots = 10;
unsigned long currentBeepDuration = 150;
int currentBeepToneHz = 2000;
int shotThresholdRms = 15311;
int dryFireParBeepCount = 3;
float dryFireParTimesSec[MAX_PAR_BEEPS];
float recoilThreshold = 1.5f;
int screenRotationSetting = 3;
bool playBootAnimation = true;
bool enableAutoSleep = true;

BluetoothA2DPSource a2dp_source;
String currentBluetoothDeviceName = "LEXON MINO L";
bool currentBluetoothAutoReconnect = false;
int currentBluetoothVolume = 80;
int currentBluetoothAudioOffsetMs = 0; 
bool bluetoothJustConnected = false;
bool bluetoothJustDisconnected = false;

// Volatile variables for A2DP audio callback
volatile int btBeepFrequency = 0;
volatile unsigned long btBeepScheduledStartTime = 0;
volatile unsigned int btBeepDurationVolatile = 0;
volatile bool new_bt_beep_request = false;
volatile bool current_bt_beep_is_active = false; 
volatile unsigned long current_bt_beep_actual_end_time = 0;

// --- Timer State Variables ---
volatile bool is_listening_active = false;      // Definition
volatile unsigned long beep_audio_end_time = 0; // Definition


ESP32BluetoothScanner btScanner;
std::vector<BTDevice> discoveredBtDevices;
int scanMenuSelection = 0;
int scanMenuScrollOffset = 0;
bool scanInProgress = false;
unsigned long scanStartTime = 0;

int shotCount = 0;
unsigned long shotTimestamps[MAX_SHOTS_LIMIT];
float splitTimes[MAX_SHOTS_LIMIT];
unsigned long lastShotTimestamp = 0;
unsigned long lastDetectionTime = 0;

int currentMenuSelection = 0;
int menuScrollOffset = 0;
int settingsMenuLevel = 0;
unsigned long btnTopPressTime = 0;
bool btnTopHeld = false;
bool redrawMenu = true;

EditableSetting settingBeingEdited = EDIT_NONE;
int editingIntValue = 0;
unsigned long editingULongValue = 0;
float editingFloatValue = 0.0f;
bool editingBoolValue = false;
const char* editingSettingName = "";

String fileListNames[MAX_FILES_LIST];
size_t fileListSizes[MAX_FILES_LIST];
int fileListCount = 0;
int fileListScrollOffset = 0;

float currentCyclePeakRMS = 0.0f;
float peakRMSOverall = 0.0f;

Preferences preferences;
float peakBatteryVoltage = 4.2f;
float currentBatteryVoltage = 0.0f;
bool lowBatteryWarning = false;
unsigned long lastBatteryCheckTime = 0;

M5MicPeakRMS micPeakRMS;

int currentJpgFrame = 1;
bool filesystem_ok_for_boot = false;
unsigned long lastFrameTime = 0;

unsigned long randomDelayStartMs = 0;
unsigned long parTimerStartTime = 0;
unsigned long beepSequenceStartTime = 0;
int beepsPlayed = 0;
unsigned long nextBeepTime = 0;
unsigned long lastBeepTime = 0;

unsigned long lastSoundPeakTime = 0;
bool checkingForRecoil = false;
float peakRecoilValue = 0.0f;
// AVRC metadata is defined in bluetooth_utils.cpp

// --- FreeRTOS Handles ---
QueueHandle_t buzzerQueue = NULL; 
TaskHandle_t buzzerTaskHandle = NULL; 

// Forward declaration for the task function (defined in audio_utils.cpp)
void buzzerTask(void *pvParameters); 

// --- Setup ---
void setup() {
    StickCP2.begin();

    preferences.begin(NVS_NAMESPACE, false); 
    loadSettings(); 

    StickCP2.Lcd.setRotation(screenRotationSetting);
    StickCP2.Lcd.setTextColor(WHITE, BLACK);
    StickCP2.Lcd.setTextDatum(MC_DATUM);
    StickCP2.Lcd.setTextFont(0); 

    pinMode(BUZZER_PIN, OUTPUT);
    digitalWrite(BUZZER_PIN, LOW);
    pinMode(BUZZER_PIN_2, OUTPUT);
    digitalWrite(BUZZER_PIN_2, LOW);

    StickCP2.Speaker.end(); 

    if (!micPeakRMS.begin(StickCP2)) {
        displayBootScreen("ERROR", "", "Mic Init Failed!");
        // playUnsuccessBeeps(); // Buzzer task not running yet
        while(true); 
    }
    micPeakRMS.resetPeak();

    if (!StickCP2.Imu.begin()) {
        displayBootScreen("WARNING", "", "IMU Init Failed!");
        // playUnsuccessBeeps(); 
        delay(2000);
    }

    if(!LittleFS.begin()){
        displayBootScreen("ERROR", "", "FS Failed!");
        // playUnsuccessBeeps(); 
        delay(2000);
        filesystem_ok_for_boot = false;
    } else {
        filesystem_ok_for_boot = true;
    }

    // --- Create Buzzer Task and Queue ---
    buzzerQueue = xQueueCreate(BUZZER_QUEUE_LENGTH, sizeof(BuzzerRequest));
    if (buzzerQueue == NULL) {
        displayBootScreen("ERROR", "", "Queue Fail!");
        while(true);
    }

    xTaskCreatePinnedToCore(
        buzzerTask,          
        "BuzzerTask",        
        BUZZER_TASK_STACK_SIZE, 
        NULL,                
        1,                   
        &buzzerTaskHandle,   
        0);                  

    if (buzzerTaskHandle == NULL) {
         displayBootScreen("ERROR", "", "Task Fail!");
         while(true);
    }
    // --- End Buzzer Task Setup ---


    checkBattery(); 

    a2dp_source.set_auto_reconnect(false); 
    a2dp_source.set_data_callback_in_frames(get_data_frames);
    a2dp_source.set_volume(currentBluetoothVolume);
    a2dp_source.set_on_connection_state_changed(a2dp_connection_state_changed_callback);
    a2dp_source.set_ssid_callback(a2dp_ssid_callback); 
    // a2dp_source.set_avrc_metadata(avrc_metadata);

    if (currentBluetoothAutoReconnect && !currentBluetoothDeviceName.isEmpty()) {
        a2dp_source.start((char*)currentBluetoothDeviceName.c_str()); 
    }

    StickCP2.Lcd.fillScreen(BLACK);
    displayBootScreen("Hey Man, Nice Shot", "Timer", "Initialization Complete!");
    playSuccessBeeps(); 
    delay(500); 

    resetActivityTimer();

    if (filesystem_ok_for_boot && playBootAnimation) {
        setState(BOOT_JPG_SEQUENCE);
        currentJpgFrame = 1;
        lastFrameTime = 0;
        StickCP2.Lcd.fillScreen(BLACK);
    } else {
        delay(1000); 
        setState(MODE_SELECTION);
        currentMenuSelection = (int)currentMode;
        menuScrollOffset = 0;
        StickCP2.Lcd.fillScreen(BLACK);
    }
}

// --- Main Loop ---
void loop() {
    StickCP2.update();
    unsigned long currentTime = millis();

    if (bluetoothJustConnected) {
        playSuccessBeeps(); 
        bluetoothJustConnected = false;
        if(currentState == SETTINGS_MENU_BLUETOOTH || currentState == BLUETOOTH_SCANNING || currentState == MODE_SELECTION) {
            redrawMenu = true;
        }
    }
    if (bluetoothJustDisconnected) {
        playUnsuccessBeeps(); 
        bluetoothJustDisconnected = false;
        if(currentState == SETTINGS_MENU_BLUETOOTH || currentState == BLUETOOTH_SCANNING || currentState == MODE_SELECTION) {
            redrawMenu = true;
        }
    }

    if (enableAutoSleep && 
        currentState != BOOT_SCREEN && 
        currentState != BOOT_JPG_SEQUENCE && 
        currentState != BLUETOOTH_SCANNING && 
        !a2dp_source.is_connected() ) { 
        if (currentTime - lastActivityTime > AUTO_SLEEP_TIMEOUT_MS) {
            StickCP2.Lcd.fillScreen(BLACK);
            StickCP2.Lcd.setTextDatum(MC_DATUM);
            StickCP2.Lcd.drawString("Sleeping...", StickCP2.Lcd.width()/2, StickCP2.Lcd.height()/2);
            delay(SLEEP_MESSAGE_DELAY_MS);
            StickCP2.Lcd.sleep();
            StickCP2.Lcd.waitDisplay();

            esp_sleep_enable_ext1_wakeup((1ULL << 37), ESP_EXT1_WAKEUP_ALL_LOW); 
            esp_light_sleep_start();

            StickCP2.Lcd.wakeup();
            delay(200); 
            resetActivityTimer();
            redrawMenu = true; 
        }
    }

    bool micUpdateNeeded = (currentState == LIVE_FIRE_TIMING ||
                            currentState == NOISY_RANGE_TIMING ||
                            currentState == CALIBRATE_THRESHOLD);
    if (micUpdateNeeded && (is_listening_active || currentState == CALIBRATE_THRESHOLD)) {
        micPeakRMS.update();
    }

    if (currentTime - lastBatteryCheckTime > BATTERY_CHECK_INTERVAL_MS) {
        checkBattery();
        if (currentState == DEVICE_STATUS || currentState == LIST_FILES || 
            currentState == MODE_SELECTION || currentState == SETTINGS_MENU_BLUETOOTH || 
            currentState == BLUETOOTH_SCANNING || lowBatteryWarning) { 
             redrawMenu = true;
        }
    }

    if (StickCP2.BtnB.isPressed()) {
        resetActivityTimer(); 
        if (btnTopPressTime == 0) {
            btnTopPressTime = currentTime;
        } else if (!btnTopHeld && (currentTime - btnTopPressTime > LONG_PRESS_DURATION_MS)) {
            btnTopHeld = true; 

            bool exitToModeSelect = (currentState == LIVE_FIRE_READY || currentState == LIVE_FIRE_TIMING || currentState == LIVE_FIRE_STOPPED ||
                                     currentState == DRY_FIRE_READY || currentState == DRY_FIRE_RUNNING ||
                                     currentState == NOISY_RANGE_READY || currentState == NOISY_RANGE_TIMING || currentState == NOISY_RANGE_GET_READY);

            if (exitToModeSelect) {
                playUnsuccessBeeps();
                setState(MODE_SELECTION);
                currentMenuSelection = (int)currentMode; 
                menuScrollOffset = 0;
                StickCP2.Lcd.fillScreen(BLACK);
            }
            else if (currentState != SETTINGS_MENU_MAIN && currentState != SETTINGS_MENU_GENERAL &&
                     currentState != SETTINGS_MENU_BEEP && currentState != SETTINGS_MENU_BLUETOOTH &&
                     currentState != SETTINGS_MENU_DRYFIRE && currentState != SETTINGS_MENU_NOISY &&
                     currentState != BLUETOOTH_SCANNING && 
                     currentState != DEVICE_STATUS && currentState != LIST_FILES && 
                     currentState != EDIT_SETTING && currentState != CALIBRATE_THRESHOLD && 
                     currentState != CALIBRATE_RECOIL &&
                     currentState != BOOT_JPG_SEQUENCE) 
            {
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

    switch (currentState) {
        case BOOT_SCREEN: break; 

        case BOOT_JPG_SEQUENCE:
            {
                if (currentState != BOOT_JPG_SEQUENCE) break; 
                if (StickCP2.BtnA.wasClicked()) {
                    resetActivityTimer();
                    setState(MODE_SELECTION);
                    currentMenuSelection = (int)currentMode;
                    menuScrollOffset = 0;
                    StickCP2.Lcd.fillScreen(BLACK);
                    break;
                }
                if (currentTime - lastFrameTime >= BOOT_JPG_FRAME_DELAY_MS) {
                    resetActivityTimer();
                    char jpgFilename[12];
                    sprintf(jpgFilename, "/%d.jpg", currentJpgFrame);
                    if (LittleFS.exists(jpgFilename) && currentJpgFrame <= MAX_BOOT_JPG_FRAMES) {
                        File jpgFile = LittleFS.open(jpgFilename, FILE_READ);
                        if (!jpgFile) {
                            setState(MODE_SELECTION); break;
                        }
                        bool success = StickCP2.Lcd.drawJpg(&jpgFile, 0, 0, StickCP2.Lcd.width(), StickCP2.Lcd.height(), 0, 0, 0.0f, 0.0f, datum_t::middle_center);
                        jpgFile.close();
                        if (!success) {
                            setState(MODE_SELECTION); break;
                        }
                        currentJpgFrame++;
                        lastFrameTime = currentTime;
                    } else { 
                        setState(MODE_SELECTION);
                    }
                }
                 if (currentState == MODE_SELECTION) { 
                    currentMenuSelection = (int)currentMode;
                    menuScrollOffset = 0;
                    StickCP2.Lcd.fillScreen(BLACK); 
                 }
            }
            break;

        case MODE_SELECTION:          handleModeSelectionInput(); break;
        case LIVE_FIRE_READY:         handleLiveFireReady(); break;
        case LIVE_FIRE_GET_READY:     handleLiveFireGetReady(); break;
        case LIVE_FIRE_TIMING:        handleLiveFireTiming(); break;
        case LIVE_FIRE_STOPPED:
            if (redrawMenu) {
                displayStoppedScreen();
                redrawMenu = false;
            }
            if (StickCP2.BtnA.wasClicked()) {
                resetActivityTimer();
                if (previousState == NOISY_RANGE_TIMING || previousState == NOISY_RANGE_GET_READY || currentMode == MODE_NOISY_RANGE) {
                     setState(NOISY_RANGE_READY);
                } else if (previousState == DRY_FIRE_RUNNING || currentMode == MODE_DRY_FIRE) { 
                    setState(DRY_FIRE_READY); 
                }
                else { 
                    setState(LIVE_FIRE_READY);
                }
                StickCP2.Lcd.fillScreen(BLACK);
            }
            break;
        case DRY_FIRE_READY:          handleDryFireReadyInput(); break;
        case DRY_FIRE_RUNNING:        handleDryFireRunning(); break;
        case NOISY_RANGE_READY:       handleNoisyRangeReadyInput(); break;
        case NOISY_RANGE_GET_READY:   handleNoisyRangeGetReady(); break;
        case NOISY_RANGE_TIMING:      handleNoisyRangeTiming(); break;
        case SETTINGS_MENU_MAIN:
        case SETTINGS_MENU_GENERAL:
        case SETTINGS_MENU_BEEP:
        case SETTINGS_MENU_DRYFIRE:
        case SETTINGS_MENU_NOISY:
        case SETTINGS_MENU_BLUETOOTH: handleSettingsInput(); break;
        case BLUETOOTH_SCANNING:      handleBluetoothScanning(); break;
        case EDIT_SETTING:            handleEditSettingInput(); break;
        case DEVICE_STATUS:           handleDeviceStatusInput(); break;
        case LIST_FILES:              handleListFilesInput(); break;
        case CALIBRATE_THRESHOLD:
        case CALIBRATE_RECOIL:        handleCalibrationInput(currentState); break;
        default: break; 
    }
    vTaskDelay(pdMS_TO_TICKS(10)); 
}

// --- Buzzer Task Definition Removed ---
// (It is now defined in audio_utils.cpp)
