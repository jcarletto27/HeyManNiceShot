#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h> // For String, PI etc.

// --- Configuration Constants (These are generally safe in headers as const) ---
const unsigned long LONG_PRESS_DURATION_MS = 750;
const unsigned long SHOT_REFRACTORY_MS = 150;
const unsigned long TIMEOUT_DURATION_MS = 15000;
const unsigned long BEEP_NOTE_DURATION_MS = 150;
const unsigned long BEEP_NOTE_DELAY_MS = 50;
const unsigned long BATTERY_CHECK_INTERVAL_MS = 60000;
const float BATTERY_LOW_PERCENTAGE = 0.78f;
const int MAX_SHOTS_LIMIT = 20;
const int MENU_ITEM_HEIGHT_LANDSCAPE = 25;
const int MENU_ITEM_HEIGHT_PORTRAIT = 18;
const int MENU_ITEMS_PER_SCREEN_LANDSCAPE = 3;
const int MENU_ITEMS_PER_SCREEN_PORTRAIT = 5;
const unsigned long POST_BEEP_DELAY_MS = 750; // Increased further to 750ms
const int MAX_FILES_LIST = 20;
const unsigned long BOOT_JPG_FRAME_DELAY_MS = 100;
const int MAX_BOOT_JPG_FRAMES = 150;
const unsigned long MESSAGE_DISPLAY_MS = 2000;
const unsigned long DRY_FIRE_RANDOM_DELAY_MIN_MS = 2000;
const unsigned long DRY_FIRE_RANDOM_DELAY_MAX_MS = 5000;
const int MAX_PAR_BEEPS = 10;
const unsigned long RECOIL_DETECTION_WINDOW_MS = 100;
const unsigned long MIN_FIRST_SHOT_TIME_MS = 100; // Min time after start for first shot
const unsigned long AUTO_SLEEP_TIMEOUT_MS = 1 * 60 * 1000;
const unsigned long SLEEP_MESSAGE_DELAY_MS = 1500;
// #define C3_FREQUENCY 130.81f // No longer used for keep-alive
const unsigned long BT_SCAN_DURATION_S = 10;
const int MAX_BT_DEVICES_DISPLAY = 20;
const unsigned long DISPLAY_UPDATE_INTERVAL_MS = 100;
const int BT_AUDIO_OFFSET_STEP_MS = 50; 
const int BUZZER_QUEUE_LENGTH = 10; 
const int BUZZER_TASK_STACK_SIZE = 2048; 

// --- Buzzer Pins (External) ---
#define BUZZER_PIN 25
#define BUZZER_PIN_2 2

// --- NVS Keys (Declarations) ---
extern const char* NVS_NAMESPACE;
extern const char* KEY_MAX_SHOTS;
extern const char* KEY_BEEP_DUR;
extern const char* KEY_BEEP_HZ;
extern const char* KEY_SHOT_THRESH;
extern const char* KEY_DF_BEEP_CNT;
extern const char* KEY_NR_RECOIL;
extern const char* KEY_PEAK_BATT;
extern const char* KEY_ROTATION;
extern const char* KEY_BOOT_ANIM;
extern const char* KEY_AUTO_SLEEP;
extern const char* KEY_BT_DEVICE_NAME;
extern const char* KEY_BT_AUTO_RECONNECT;
extern const char* KEY_BT_VOLUME;
extern const char* KEY_BT_AUDIO_OFFSET; 

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
    SETTINGS_MENU_BLUETOOTH,
    BLUETOOTH_SCANNING,
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
    EDIT_BOOT_ANIM,
    EDIT_AUTO_SLEEP,
    EDIT_BT_AUTO_RECONNECT,
    EDIT_BT_VOLUME,
    EDIT_BT_AUDIO_OFFSET 
};

// --- Struct for Buzzer Task Queue ---
typedef struct {
    int frequency;
    int duration;
} BuzzerRequest;


#endif // CONFIG_H
