# M5StickCPlus2 Shot Timer

## Description

This project turns an M5StickCPlus2 into a versatile shot timer for live fire and dry fire practice. It utilizes the built-in microphone (or microphone+IMU in Noisy Range mode) to detect shots and provides various timing modes and configuration options.

## Hardware Required

* **M5StickCPlus2:** The core microcontroller and display unit. ([M5Stack Store](https://shop.m5stack.com/products/m5stickc-plus2-esp32-pico-d4))
* **External Buzzer (Optional but Recommended):** Connect an active buzzer to GPIO 25 and GPIO 2 (and GND) for louder audio feedback than the internal speaker (which is disabled by the code).

## Features

* **Multiple Operating Modes:**
    * **Live Fire:** Standard shot timer using microphone detection. Records first shot time and split times.
    * **Dry Fire Par:** Buzzer-only mode with a random start delay (2-5s) followed by a sequence of beeps at user-defined intervals (par times). Useful for practicing draws and shots against a par time without needing microphone input.
    * **Noisy Range (Sound + Recoil):** Detects shots based on a combination of a sound peak exceeding a threshold *and* a subsequent recoil spike detected by the IMU (Z-axis acceleration) within a short time window. Aims to reduce false positives in loud environments.
* **Configurable Settings:**
<<<<<<< HEAD
    * Maximum number of shots to record per sequence.
    * Start beep duration (ms).
    * Start beep tone frequency (Hz).
* **Non-Volatile Storage (NVS):** Saves user settings (Max Shots, Beep Duration/Tone) and the calibrated Shot Threshold, so they persist after power cycles.
* **Audio Feedback:** Uses `espTone2x` library for buzzer sounds (start beep, success/failure notifications).
* **State Machine Logic:** Clearly defined states (`IDLE`, `READY`, `WAITING`, `TIMING`, `DISPLAY_RESULT`, `SETTINGS`, `CALIBRATING_THRESHOLD`, `CALIBRATION_ERROR`) manage the device's operation.

## Future Changes / Feature Requests
* **Add a Dry fire timing drill**
* **Add a Live fire timing drill**
   * Incorporate MMU/Gyroscope to match recoil with gun shot, for busy ranges
* **Migrate code to m5stickC PLUS2**
   * The m5stickC PLUS2 has most of the below required hardware, with the only drawback being a lower output piezo buzzer (75dB versus my recommended 100dB)
   * Design a simple, low cost daughter board for louder buzzer




## Hardware Requirements

* **Microcontroller:** ESP32-C3 [DigiKey](https://www.digikey.com/en/products/detail/seeed-technology-co-ltd/113991054/16652880) (Tested with XIAO ESP32C3, pin definitions use XIAO D# aliases).
* **Microphone:** I2S Digital Microphone (e.g., INMP441, ICS43434, SPH0645). [Amazon](https://a.co/d/dpjLYD9)
* **Display:** 128x64 SSD1306 OLED Display (I2C interface). [DigiKey](https://www.digikey.com/en/products/detail/universal-solder-electronics-ltd/26095/16822116)
* **Buttons:** 2 x Momentary Push Buttons. [Amazon](https://a.co/d/9ReTBhi)
* **Buzzer:** Passive Buzzer. [DigiKey](https://www.digikey.com/short/qd1v02wq)
* **Wiring:** Connecting wires, potentially a breadboard or PCB. [Amazon-Solderable Breadboard](https://a.co/d/a0BvrwT)
* **Battery** Must be 3.7v LiPo to use the charging circuit on the XIAO boards, something between 800mah and 1500mah should be sufficient [Amazon](https://a.co/d/4fThqvW)

## Pinout (Based on XIAO ESP32C3 D# Aliases in Code)

| Function              | Pin Alias     | ESP32-C3 GPIO | Connected To      |
| :-------------------- | :------------ | :------------ | :---------------- |
| Button 1 (Start/Up)   | `BUTTON1_PIN` | D0 (`GPIO_NUM_2`) | Momentary Button 1 |
| Button 2 (Stop/Down)  | `BUTTON2_PIN` | D1 (`GPIO_NUM_3`) | Momentary Button 2 |
| Buzzer Pin 1a         | `TONE_PIN_1`  | D6 (`GPIO_NUM_7`) | Buzzer (+)        |
| Buzzer Pin 1b         | `TONE_PIN_1_2`| D7 (`GPIO_NUM_21`)| Buzzer (+)        |
| Buzzer Pin 2a         | `TONE_PIN_2`  | D3 (`GPIO_NUM_4`) | Buzzer (-)        |
| Buzzer Pin 2b         | `TONE_PIN_2_2`| D2 (`GPIO_NUM_5`) | Buzzer (-)        |
| I2C Data              | `I2C_SDA_PIN` | D4 (`GPIO_NUM_5`) | OLED SDA          |
| I2C Clock             | `I2C_SCL_PIN` | D5 (`GPIO_NUM_6`) | OLED SCL          |
| I2S Word Select (LRCL)| `I2S_WS_PIN`  | D9 (`GPIO_NUM_9`) | I2S Mic WS/LRCL   |
| I2S Serial Clock(BCLK)| `I2S_SCK_PIN` | D8 (`GPIO_NUM_8`) | I2S Mic SCK/BCLK  |
| I2S Serial Data (DIN) | `I2S_SD_PIN`  | D10 (`GPIO_NUM_10`)| I2S Mic SD/DOUT   |

**Note on Buzzer:** The `espTone2x` library is configured for differential drive using two pairs of pins (D6/D7 and D3/D2). Connect your buzzer across one of these pairs (e.g., D6 and D7).

**Potential Pin Conflict:** The default code assigns `D2` (`GPIO_NUM_5`) to `TONE_PIN_2_2` and `D4` (`GPIO_NUM_5`) to `I2C_SDA_PIN`. **GPIO 5 cannot be used for both simultaneously.** If you need both the second buzzer channel *and* I2C, you **must** reassign one of these functions to a different available GPIO pin in the `#define` section and update your wiring accordingly. Often, only one buzzer pair (e.g., D6/D7) is needed.
=======
    * Maximum Shots (Live/Noisy modes)
    * Beep Duration & Tone (Frequency)
    * Sound Detection Threshold (Live/Noisy modes)
    * Recoil Threshold (Noisy mode)
    * Dry Fire Par Beep Count
    * Individual Dry Fire Par Times for each beep interval
    * Screen Rotation (0, 1, 2, 3)
    * Enable/Disable Boot Animation
* **Calibration:**
    * Calibrate sound threshold based on ambient noise or specific sound source.
    * Calibrate recoil threshold by capturing peak G-force during actual recoil.
* **Device Status Screen:** Displays battery voltage/percentage, charging status, peak recorded battery voltage, IMU accelerometer readings, and LittleFS usage.
* **File System:** Uses LittleFS for storing settings and boot animation images.
* **Boot Animation:** Optionally displays a sequence of JPG images (`/1.jpg`, `/2.jpg`, etc.) from LittleFS on startup. Can be skipped with a button press.
* **Low Battery Warning:** Visual indicator and audible alert when battery is low.
>>>>>>> e426728 (updated readme)

## Libraries Required

* **M5StickCPlus2 Library:** (Installs M5Unified and M5GFX as dependencies) - Install via Arduino Library Manager or PlatformIO Library Manager.
* **Preferences:** Built-in ESP32 library.
* **LittleFS:** Built-in ESP32 library (ensure ESP32 core is up-to-date).
* **M5MicPeakRMS:** The custom library included (`M5MicPeakRMS.h`, `M5MicPeakRMS.cpp`) for microphone peak detection. Place these files in your sketch directory or in your Arduino libraries folder under a subfolder named `M5MicPeakRMS`.

## Setup Instructions

1.  **IDE Setup:**
    * **Arduino IDE:** Install the ESP32 board support package and the M5StickCPlus2 library.
    * **PlatformIO:** Configure your `platformio.ini` for the `m5stick-c-plus` board (or similar, check exact board ID) and include the M5StickCPlus2 library under `lib_deps`.
2.  **Board Selection:** Select "M5StickCPlus2" (or the appropriate variant) in your IDE's board manager.
3.  **Partition Scheme:** **Crucially**, select a partition scheme that includes **LittleFS**. A scheme like "Default 4MB with LittleFS (...)" is recommended. If you change the partition scheme, you must re-upload the main sketch.
4.  **Custom Library:** Ensure the `M5MicPeakRMS.h` and `M5MicPeakRMS.cpp` [Link](https://github.com/jcarletto27/M5MicPeakRMS)files are accessible to the compiler (either in the sketch folder or a dedicated library folder).
5.  **LittleFS Data Upload:**
    * Create a folder named `data` in your main sketch directory (Arduino IDE) or project root (PlatformIO).
    * Place your boot animation JPG files inside this `data` folder, named sequentially: `1.jpg`, `2.jpg`, `3.jpg`, etc.
    * Use the appropriate tool to upload the filesystem image:
        * **Arduino IDE:** `Tools` -> `ESP32 Sketch Data Upload`
        * **PlatformIO:** Run the "Upload Filesystem Image" task.
6.  **Compile & Upload:** Compile and upload the main sketch (`.ino` file).

## Usage

* **Boot:** The device will boot, initialize components, and optionally play the JPG boot animation (if enabled and files are present). Press the front button (BtnA) during the animation to skip it.
* **Mode Selection:** Use the top/bottom side buttons (BtnB/PWR, orientation-dependent) to scroll through modes and the front button (BtnA) to select. Battery percentage is shown in the top-right.
* **Timer Operation (Live/Noisy):**
    * Press BtnA to show "Ready...".
    * After a short delay, the start beep sounds, and timing begins.
    * Shots are detected based on the selected mode's criteria (sound or sound+recoil).
    * Press BtnA again to manually stop the timer.
    * The timer stops automatically after the max shot count or timeout.
    * The results screen shows total shots, first shot time, last split, and fastest split. Press BtnA to reset to the ready screen.
* **Timer Operation (Dry Fire Par):**
    * Press BtnA to start the sequence.
    * "Waiting..." is displayed during the random delay (2-5s).
    * The first beep sounds.
    * Subsequent beeps sound based on the configured Par Times. The screen shows "Beep X / Y".
    * Hold BtnA during the sequence to cancel and return to the ready screen.
* **Settings Menu:**
    * Hold the top side button (BtnB) from any main mode screen to enter Settings.
    * Navigate menus using the top/bottom side buttons.
    * Select items/confirm edits with a short press of the front button (BtnA).
    * Go back/cancel edits/exit settings with a long press of the front button (BtnA).

## Filesystem Structure (LittleFS)

The LittleFS partition should contain the following files in the root directory (`/`) for the boot animation:

* `/1.jpg`
* `/2.jpg`
* `/3.jpg`
* ... (up to `/150.jpg` or however many frames you have, respecting `MAX_BOOT_JPG_FRAMES`)

## Future Enhancements

* Fully implement IMU data reading and processing for `CALIBRATE_RECOIL`.
* Refine recoil detection logic in Noisy Range mode.
* Add options to save/review shot data strings.
* Implement more sophisticated power management.

## Contributing

Contributions are welcome! Please feel free to fork the repository, make changes, and submit pull requests.
