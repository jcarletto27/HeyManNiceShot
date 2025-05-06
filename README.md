# Hey Man, Nice Shot...Timer
## M5StickCPlus2 Shot Timer

## Description

This project turns an M5StickCPlus2 into a versatile shot timer for live fire and dry fire practice. It utilizes the built-in microphone (or microphone+IMU in Noisy Range mode) to detect shots and provides various timing modes and configuration options.

## Hardware Required

* **M5StickCPlus2:** The core microcontroller and display unit. ([M5Stack Store](https://shop.m5stack.com/products/m5stickc-plus2-esp32-pico-d4))
* **External Buzzer (Optional but Recommended):** Connect an active buzzer to GPIO 25 and GND for louder audio feedback than the internal speaker.

## Features

* **Multiple Operating Modes:**
    * **Live Fire:** Standard shot timer using microphone detection. Records first shot time and split times. Ignores initial detections faster than 0.1s.
    * **Dry Fire Par:** Buzzer-only mode with a random start delay (2-5s) followed by a sequence of beeps at user-defined intervals (individual par times per beep). Useful for practicing draws and shots against a par time without needing microphone input.
    * **Noisy Range (Sound + Recoil):** Detects shots based on a combination of a sound peak exceeding a threshold *and* a subsequent recoil spike detected by the IMU (Z-axis acceleration) within a short time window. Aims to reduce false positives in loud environments. Ignores initial detections faster than 0.1s.
* **Configurable Settings:**
    * Maximum Shots (Live/Noisy modes)
    * Beep Settings (Sub-menu for Duration & Tone/Frequency)
    * Sound Detection Threshold (Live/Noisy modes)
    * Recoil Threshold (Noisy mode)
    * Dry Fire Par Beep Count
    * Individual Dry Fire Par Times for each beep interval (up to Par Beep Count)
    * Screen Rotation (0, 1, 2, 3)
    * Enable/Disable Boot Animation
    * Enable/Disable Auto Power Off (10-minute inactivity timer)
* **Calibration:**
    * Calibrate sound threshold based on ambient noise or specific sound source.
    * Calibrate recoil threshold by capturing peak G-force during actual recoil.
* **Device Status Screen:** Displays battery voltage/percentage, charging status, peak recorded battery voltage, IMU accelerometer readings, and LittleFS usage.
* **File System:** Uses LittleFS for storing settings and boot animation images.
* **Boot Animation:** Optionally displays a sequence of JPG images (`/1.jpg`, `/2.jpg`, etc.) from LittleFS on startup. Can be skipped with a button press (BtnA).
* **Low Battery Warning:** Visual indicator and audible alert when battery is low.
* **Power Management:**
    * Immediate Power Off option in the main settings menu.
    * Optional 10-minute auto power-off timer (resets on activity).

## Libraries Required

* **M5StickCPlus2 Library:** (Installs M5Unified and M5GFX as dependencies) - Install via Arduino Library Manager or PlatformIO Library Manager.
* **Preferences:** Built-in ESP32 library.
* **LittleFS:** Built-in ESP32 library (ensure ESP32 core is up-to-date).
* **M5MicPeakRMS:** [link](https://github.com/jcarletto27/M5MicPeakRMS) The custom library included (`M5MicPeakRMS.h`, `M5MicPeakRMS.cpp`) for microphone peak detection. Place these files in your sketch directory or in your Arduino libraries folder under a subfolder named `M5MicPeakRMS`.

## Setup Instructions

1.  **IDE Setup:**
    * **Arduino IDE:** Install the ESP32 board support package and the M5StickCPlus2 library.
    * **PlatformIO:** Configure your `platformio.ini` for the `m5stick-c-plus` board (or similar, check exact board ID) and include the M5StickCPlus2 library under `lib_deps`.
2.  **Board Selection:** Select "M5StickCPlus2" (or the appropriate variant) in your IDE's board manager.
3.  **Partition Scheme:** **Crucially**, select a partition scheme that includes **LittleFS** (e.g., "Default 4MB with LittleFS (...)"). If you change the partition scheme, you must re-upload the main sketch *and* the filesystem image.
4.  **Custom Library:** Ensure the `M5MicPeakRMS.h` and `M5MicPeakRMS.cpp` files are accessible to the compiler (either in the sketch folder or a dedicated library folder).
5.  **LittleFS Data Upload:**
    * Create a folder named `data` in your main sketch directory (Arduino IDE) or project root (PlatformIO).
    * Place your boot animation JPG files inside this `data` folder, named sequentially: `1.jpg`, `2.jpg`, `3.jpg`, etc.
    * Use the appropriate tool to upload the filesystem image:
        * **Arduino IDE:** Install the [LittleFS ESP32 Arduino upload tool](https://github.com/lorol/arduino-esp32fs-plugin) or use the PlatformIO method.
        * **PlatformIO:** Run the "Upload Filesystem Image" task (usually found under the PlatformIO sidebar -> Project Tasks -> [your environment] -> Platform -> Upload Filesystem Image).
6.  **Compile & Upload:** Compile and upload the main sketch (`.ino` file).

## Usage

* **Boot:** The device will boot, initialize components, and optionally play the JPG boot animation (if enabled and files are present). Press the front button (BtnA) during the animation to skip it.
* **Mode Selection:** Use the top/bottom side buttons (BtnB/PWR, orientation-dependent) to scroll through modes and the front button (BtnA) to select. Battery percentage is shown in the top-right.
* **Timer Operation (Live/Noisy):**
    * Press BtnA to show "Ready...".
    * After a short delay, the start beep sounds, and timing begins.
    * Shots are detected based on the selected mode's criteria (sound or sound+recoil).
    * Press BtnA again to manually stop the timer.
    * Hold the top button (BtnB) to exit back to Mode Selection.
    * The timer stops automatically after the max shot count or timeout.
    * The results screen shows total shots, first shot time, last split, and fastest split. Press BtnA to reset to the ready screen. Hold BtnB to exit to Mode Selection.
* **Timer Operation (Dry Fire Par):**
    * Press BtnA to start the sequence.
    * "Waiting..." is displayed during the random delay (2-5s).
    * The first beep sounds.
    * Subsequent beeps sound based on the configured Par Times. The screen shows "Beep X / Y".
    * Hold BtnA or BtnB during the sequence to cancel and return to the ready screen.
* **Settings Menu:**
    * Hold the top side button (BtnB) from any main mode screen (Mode Select, Ready, Stopped) to enter Settings.
    * Navigate menus using the top/bottom side buttons.
    * Select items/confirm edits with a short press of the front button (BtnA).
    * Go back/cancel edits/exit settings with a long press of the front button (BtnA).
    * Select "Power Off Now" to immediately turn off the device.

## Filesystem Structure (LittleFS)

The LittleFS partition should contain the following files in the root directory (`/`) for the boot animation:

* `/1.jpg`
* `/2.jpg`
* `/3.jpg`
* ... (up to `/150.jpg` or however many frames you have, respecting `MAX_BOOT_JPG_FRAMES`)

## Model Printed and Attached to a Blue Gun

* ![Attached](https://github.com/jcarletto27/HeyManNiceShotTimer/blob/main/images/PXL_20250506_164010292.MP.jpg?raw=true)
* ![Mounted in Lanyard Mode](https://github.com/jcarletto27/HeyManNiceShotTimer/blob/main/images/PXL_20250506_164025303.MP.jpg?raw=true)
* ![Closeup of the mounting mechanism](https://github.com/jcarletto27/HeyManNiceShotTimer/blob/main/images/PXL_20250506_164028005.jpg?raw=true)

## Future Enhancements

* Fully implement IMU data reading and processing for `CALIBRATE_RECOIL`.
* Refine recoil detection logic in Noisy Range mode.
* Add options to save/review shot data strings.
* Implement more sophisticated power management (e.g., deep sleep).

## Contributing

Contributions are welcome! Please feel free to fork the repository, make changes, and submit pull requests.
