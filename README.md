# ESP32-C3 I2S Shot Timer

An Arduino-based shot timer using an ESP32-C3 microcontroller, an I2S microphone for precise audio capture, an SSD1306 OLED display for user interface, and a buzzer for audio feedback. It detects loud noises (shots) based on sound levels (RMS), times the intervals, and allows configuration through an on-device menu.

## Features

* **Acoustic Shot Detection:** Uses an I2S microphone to listen for shots.
* **RMS Sound Level Analysis:** Calculates the Root Mean Square (RMS) of the audio signal to determine loudness.
* **Calibratable Shot Threshold:**
    * Performs ambient noise calibration on startup.
    * Allows user-triggered calibration to set the shot detection threshold based on a sample shot.
    * Threshold is automatically saved to Non-Volatile Storage (NVS).
* **Random Start Delay:** Implements a configurable random delay before the start beep in timer mode.
* **Timing Modes:**
    * Measures time from start beep to the first shot.
    * Measures split times between subsequent shots.
* **OLED Display Interface:** Shows current status, timing results, settings menu, and calibration prompts on a 128x64 SSD1306 display.
* **Button Interface:** Uses two buttons for all operations:
    * Start/Reset Timer
    * Stop Timer
    * Navigate Settings Menu (Up/Down/Next/Previous)
    * Enter/Exit Settings (Long Press)
    * Initiate Threshold Calibration (Long Press)
* **Configurable Settings:**
    * Maximum number of shots to record per sequence.
    * Start beep duration (ms).
    * Start beep tone frequency (Hz).
* **Non-Volatile Storage (NVS):** Saves user settings (Max Shots, Beep Duration/Tone) and the calibrated Shot Threshold, so they persist after power cycles.
* **Audio Feedback:** Uses `espTone2x` library for buzzer sounds (start beep, success/failure notifications).
* **State Machine Logic:** Clearly defined states (`IDLE`, `READY`, `WAITING`, `TIMING`, `DISPLAY_RESULT`, `SETTINGS`, `CALIBRATING_THRESHOLD`, `CALIBRATION_ERROR`) manage the device's operation.

## Hardware Requirements

* **Microcontroller:** ESP32-C3 (Tested with XIAO ESP32C3, pin definitions use XIAO D# aliases).
* **Microphone:** I2S Digital Microphone (e.g., INMP441, ICS43434, SPH0645).
* **Display:** 128x64 SSD1306 OLED Display (I2C interface).
* **Buttons:** 2 x Momentary Push Buttons.
* **Buzzer:** Passive Buzzer.
* **Wiring:** Connecting wires, potentially a breadboard or PCB.

## Pinout (Based on XIAO ESP32C3 D# Aliases in Code)

| Function              | Pin Alias     | ESP32-C3 GPIO | Connected To      |
| :-------------------- | :------------ | :------------ | :---------------- |
| Button 1 (Start/Up)   | `BUTTON1_PIN` | D0 (`GPIO_NUM_2`) | Momentary Button 1 |
| Button 2 (Stop/Down)  | `BUTTON2_PIN` | D1 (`GPIO_NUM_3`) | Momentary Button 2 |
| Buzzer Pin 1a         | `TONE_PIN_1`  | D6 (`GPIO_NUM_7`) | Buzzer (+)        |
| Buzzer Pin 1b         | `TONE_PIN_1_2`| D7 (`GPIO_NUM_21`)| Buzzer (-)        |
| Buzzer Pin 2a         | `TONE_PIN_2`  | D3 (`GPIO_NUM_4`) | Buzzer (+)        |
| Buzzer Pin 2b         | `TONE_PIN_2_2`| D2 (`GPIO_NUM_5`) | Buzzer (-)        |
| I2C Data              | `I2C_SDA_PIN` | D4 (`GPIO_NUM_5`) | OLED SDA          |
| I2C Clock             | `I2C_SCL_PIN` | D5 (`GPIO_NUM_6`) | OLED SCL          |
| I2S Word Select (LRCL)| `I2S_WS_PIN`  | D9 (`GPIO_NUM_9`) | I2S Mic WS/LRCL   |
| I2S Serial Clock(BCLK)| `I2S_SCK_PIN` | D8 (`GPIO_NUM_8`) | I2S Mic SCK/BCLK  |
| I2S Serial Data (DIN) | `I2S_SD_PIN`  | D10 (`GPIO_NUM_10`)| I2S Mic SD/DOUT   |

**Note on Buzzer:** The `espTone2x` library is configured for differential drive using two pairs of pins (D6/D7 and D3/D2). Connect your buzzer across one of these pairs (e.g., D6 and D7).

**Potential Pin Conflict:** The default code assigns `D2` (`GPIO_NUM_5`) to `TONE_PIN_2_2` and `D4` (`GPIO_NUM_5`) to `I2C_SDA_PIN`. **GPIO 5 cannot be used for both simultaneously.** If you need both the second buzzer channel *and* I2C, you **must** reassign one of these functions to a different available GPIO pin in the `#define` section and update your wiring accordingly. Often, only one buzzer pair (e.g., D6/D7) is needed.

## Libraries Required

* **Adafruit GFX Library:** Install via Arduino Library Manager.
* **Adafruit SSD1306:** Install via Arduino Library Manager.
* **espTone2x:** Needs to be manually installed. You can likely find this library on GitHub. Download it and place it in your Arduino `libraries` folder. ([Search for espTone2x](https://github.com/search?q=esptone2x&type=repositories) - select the appropriate one, often by `lbernstone`).
* **ESP32 Arduino Core:** Assumes you have the ESP32 board support package installed in your Arduino IDE. Includes (`driver/i2s.h`, `esp_random.h`, `Preferences.h`).

## Configuration

* **Runtime Settings:** Max Shots, Beep Duration, and Beep Tone can be adjusted using the on-device Settings menu (see Usage).
* **Shot Threshold:** Calibrated via a dedicated mode (see Usage) and saved automatically.
* **Compile-Time Constants:** Several parameters are set via `#define` in the code (e.g., `AMBIENT_CALIBRATION_DURATION_MS`, `SHOT_LOCKOUT_MICROS`, `RANDOM_DELAY_MIN/MAX_MS`, `SETTINGS_TIMEOUT_MS`, pin definitions). These can be modified directly in the source code before compiling if necessary.

## How it Works

1.  **Boot-up:**
    * Initializes Serial, Pins, Display, I2S, and NVS.
    * Loads saved settings (Max Shots, Beep params, Shot Threshold) from NVS.
    * Performs an **Ambient Noise Calibration** for a few seconds, calculating the average background noise RMS level. Displays "Quiet, calibrating..."
    * If ambient calibration fails, it enters an error state.
    * If successful, plays success beeps and enters the `IDLE` state.
2.  **IDLE State:**
    * Displays "READY" and button functions. Shows the current shot threshold (converted to dB for readability).
    * Waits for user input.
3.  **Timing Sequence:**
    * **Button 1 Short Press (IDLE):** Enters `READY` state.
    * **READY State:** Immediately calculates a random delay (`RANDOM_DELAY_MIN_MS` to `RANDOM_DELAY_MAX_MS`) and enters `WAITING`.
    * **WAITING State:** Displays "Waiting..." Counts down the random delay. Can be cancelled with Button 1 short press.
    * **Timer Expires (WAITING):** Enters `TIMING` state.
    * **TIMING State:**
        * Starts the buzzer beep (using configured tone and duration). Records the precise beep start time (`micros()`).
        * Listens for sounds exceeding the `shotThresholdRms`.
        * When a shot is detected:
            * Records the timestamp (`micros()`).
            * Calculates First Shot time (relative to beep start) or Split Time (relative to previous shot).
            * Increments shot count.
            * Applies a short lockout (`SHOT_LOCKOUT_MICROS`) to prevent double-detection.
            * Updates the display with shot count and last time.
        * Continues until Max Shots reached, Button 2 pressed (Stop), or a timeout occurs.
    * **Stop Condition (TIMING):** Enters `DISPLAY_RESULT`. Stops the beep.
4.  **DISPLAY_RESULT State:**
    * Shows the First Shot time, Last Split time (if applicable), and Total Shots recorded.
    * If no shots were detected, indicates "No Shots".
    * Waits for Button 1 short press to return to `IDLE`.
5.  **Settings Mode:**
    * **Button 2 Long Press (IDLE):** Enters `SETTINGS` state. Loads current settings into temporary variables for editing.
    * Displays the settings menu.
    * **Button 1 Short Press:** Increases the value of the selected setting / Moves to the next value.
    * **Button 2 Short Press:** Decreases the value of the selected setting / Moves to the previous value.
    * **Button 1 Long Press:** Cycles to the *next* setting item (Max Shots -> Beep Duration -> Beep Tone -> Max Shots...). Auto-repeats if held.
    * **Button 2 Long Press:** Cycles to the *previous* setting item. Auto-repeats if held.
    * **Timeout:** If no buttons are pressed for `SETTINGS_TIMEOUT_MS`, the current temporary settings are saved to NVS, runtime variables are updated, success beeps are played, and the device returns to `IDLE`.
6.  **Threshold Calibration Mode:**
    * **Button 1 Long Press (IDLE):** Enters `CALIBRATING_THRESHOLD` state.
    * Displays "Point Mic & Fire!". Prompts the user to fire a single shot towards the microphone.
    * Listens for a short duration (`SHOT_THRESHOLD_CALIBRATION_DURATION_MS`), recording the *peak* RMS level detected. Displays the current peak (in dB).
    * **Button 2 Short Press:** Cancels calibration and returns to `IDLE`.
    * **Timer Expires:**
        * Checks if the detected peak RMS is significantly higher than the calibrated ambient level and above a minimum absolute level.
        * If valid: Sets the `shotThresholdRms` to this peak value, saves *only* the threshold to NVS, plays success beeps, and returns to `IDLE`.
        * If invalid (too quiet, too close to ambient): Plays unsuccess beeps, keeps the *old* threshold, and returns to `IDLE`.

## Usage / Controls

* **Power On:** Device calibrates ambient noise, loads settings, and enters `IDLE` state.

* **In IDLE State:**
    * `Button 1 Short Press`: Start the timing sequence (Ready -> Waiting -> Timing).
    * `Button 1 Long Press`: Enter Shot Threshold Calibration mode.
    * `Button 2 Short Press`: No action.
    * `Button 2 Long Press`: Enter Settings menu.

* **In WAITING State:**
    * `Button 1 Short Press`: Cancel the timer start and return to `IDLE`.
    * `Button 2 Short/Long Press`: No action.

* **In TIMING State:**
    * `Button 1 Short/Long Press`: No action (intended to prevent accidental reset).
    * `Button 2 Short Press`: Stop timing, go to `DISPLAY_RESULT`.
    * `Button 2 Long Press`: Stop timing, go to `DISPLAY_RESULT`.

* **In DISPLAY_RESULT State:**
    * `Button 1 Short Press`: Reset and return to `IDLE`.
    * `Button 1 Long Press`: Reset and return to `IDLE`.
    * `Button 2 Short/Long Press`: No action.

* **In SETTINGS State:**
    * `Button 1 Short Press`: Increase selected value.
    * `Button 2 Short Press`: Decrease selected value.
    * `Button 1 Long Press`: Cycle to next setting item (auto-repeats if held).
    * `Button 2 Long Press`: Cycle to previous setting item (auto-repeats if held).
    * *(Timeout)*: Save settings and return to `IDLE`.

* **In CALIBRATING_THRESHOLD State:**
    * `Button 1 Short/Long Press`: No action.
    * `Button 2 Short Press`: Cancel calibration and return to `IDLE`.
    * *(Timeout)*: Attempt to set threshold based on peak sound, save if valid, return to `IDLE`.

## Installation / Setup

1.  **Install Arduino IDE:** Download and install from the [Arduino website](https://www.arduino.cc/en/software).
2.  **Install ESP32 Board Support:** Follow the instructions for your OS to add ESP32 board support to the Arduino IDE (usually via the Board Manager). [See Espressif Docs](https://docs.espressif.com/projects/arduino-esp32/en/latest/installing.html).
3.  **Install Libraries:** Open the Arduino Library Manager (`Tools -> Manage Libraries...`) and install:
    * `Adafruit GFX Library`
    * `Adafruit SSD1306`
4.  **Install `espTone2x`:** Download the `espTone2x` library (e.g., from GitHub) as a ZIP file. In the Arduino IDE, select `Sketch -> Include Library -> Add .ZIP Library...` and choose the downloaded file.
5.  **Connect Hardware:** Wire the ESP32-C3, I2S microphone, OLED display, buttons, and buzzer according to the Pinout section. **Remember to resolve the potential GPIO 5 conflict if necessary.**
6.  **Configure IDE:**
    * Select your ESP32-C3 board from the `Tools -> Board` menu (e.g., "XIAO_ESP32C3").
    * Select the correct COM port under `Tools -> Port`.
7.  **Upload Code:** Open the `.ino` file, verify (compile), and upload to the ESP32-C3.
8.  **Monitor (Optional):** Open the Serial Monitor (`Tools -> Serial Monitor`) at 115200 baud to view debug messages, calibration info, and state changes.

## License

Consider adding a license file (e.g., MIT, Apache 2.0) to your repository. You can mention the chosen license here. Example:

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details (assuming you create a LICENSE file).
