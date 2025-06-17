# Hey Man, Nice Shot...Timer
## M5StickCPlus2 Shot Timer

## Description

This project turns an M5StickCPlus2 into a versatile shot timer for live fire and dry fire practice. It utilizes the built-in microphone (or microphone+IMU in Noisy Range mode) to detect shots and provides various timing modes, configuration options, and Bluetooth audio output capabilities.

## Hardware Required

* **M5StickCPlus2:** The core microcontroller and display unit. ([M5Stack Store](https://shop.m5stack.com/products/m5stickc-plus2-esp32-pico-d4))
* **External Buzzer (Optional but Recommended):** Connect an active buzzer to GPIO 25/GND and GPIO 2/GND for louder local audio feedback.
* **Bluetooth Speaker/Headset (Optional):** Any standard A2DP compatible speaker or headset for receiving audio prompts wirelessly.

## Features

* **Multiple Operating Modes:**
    * **Live Fire:** Standard shot timer using microphone detection. Records first shot time and split times. Ignores initial detections faster than a threshold (`MIN_FIRST_SHOT_TIME_MS`) after the start beep audio finishes.
    * **Dry Fire Par:** Audio-prompt mode with a random start delay (2-5s) followed by a sequence of beeps at user-defined intervals (individual par times per beep). Useful for practicing draws and shots against a par time without needing microphone input.
    * **Noisy Range (Sound + Recoil):** Detects shots based on a combination of a sound peak exceeding a threshold *and* a subsequent recoil spike detected by the IMU (Z-axis acceleration) within a short time window. Aims to reduce false positives in loud environments. Ignores initial detections faster than a threshold (`MIN_FIRST_SHOT_TIME_MS`) after the start beep audio finishes.
* **Audio Output Options:**
    * Local Buzzer (Pins G25/G2).
    * Bluetooth A2DP: Stream start beeps, par beeps, and feedback sounds to a connected Bluetooth speaker or headset.
* **Bluetooth Features:**
    * **Device Scanning:** Scan for nearby A2DP devices using the A2DP discovery protocol.
    * **Device Selection:** Choose a preferred Bluetooth audio device from the scan results.
    * **Connection Management:** Connect/Disconnect from the selected device via the menu.
    * **Auto-Reconnect (Optional):** Automatically attempt to connect to the last selected device on startup.
    * **Volume Control:** Adjust the output volume for the connected Bluetooth device.
    * **Audio Offset Calibration:** Adjust a millisecond offset to synchronize the Bluetooth audio beep with the local buzzer beep, compensating for Bluetooth latency.
* **Configurable Settings:**
    * Maximum Shots (Live/Noisy modes)
    * Beep Settings (Duration & Tone/Frequency)
    * Sound Detection Threshold (Live/Noisy modes)
    * Recoil Threshold (Noisy mode)
    * Dry Fire Par Beep Count & Individual Par Times
    * Bluetooth Settings (Device Name, Auto-Reconnect, Volume, Audio Offset)
    * Screen Rotation (0, 1, 2, 3)
    * Enable/Disable Boot Animation
    * Enable/Disable Auto Sleep (1-minute inactivity timer)
* **Calibration:**
    * Calibrate sound threshold based on ambient noise or specific sound source.
    * Calibrate recoil threshold by capturing peak G-force during actual recoil.
    * Calibrate Bluetooth audio offset for synchronization.
* **Device Status Screen:** Displays battery voltage/percentage, charging status, peak recorded battery voltage, IMU accelerometer readings, and LittleFS usage.
* **File System:** Uses LittleFS for storing settings and boot animation images.
* **Boot Animation:** Optionally displays a sequence of JPG images (`/1.jpg`, `/2.jpg`, etc.) from LittleFS on startup. Can be skipped with a button press (BtnA).
* **Low Battery Warning:** Visual indicator and audible alert when battery is low.
* **Power Management:**
    * Immediate Power Off option in the main settings menu.
    * Optional 1-minute auto-sleep timer (light sleep, resets on activity, disabled when BT connected).
* **Multicore Operation:** Uses FreeRTOS to run the buzzer control on Core 0, separating it from the main application logic and display updates on Core 1. A2DP audio generation also typically runs on Core 0 via the library.

## Libraries Required

* **M5StickCPlus2 Library:** (Installs M5Unified and M5GFX as dependencies) - Install via Arduino Library Manager or PlatformIO Library Manager.
* **ESP32-A2DP Library (by pschatzmann):** [link](https://github.com/pschatzmann/ESP32-A2DP)The library used for Bluetooth A2DP source functionality. Follow the instructions from the ESP32-A2DP git repo.
* **Preferences:** Built-in ESP32 library.
* **LittleFS:** Built-in ESP32 library (ensure ESP32 core is up-to-date).
* **M5MicPeakRMS:** [link](https://github.com/jcarletto27/M5MicPeakRMS) The custom library included (`M5MicPeakRMS.h`, `M5MicPeakRMS.cpp`) for microphone peak detection. Place these files in your sketch directory or in your Arduino libraries folder under a subfolder named `M5MicPeakRMS`.

## Setup Instructions

1.  **IDE Setup:**
    * **Arduino IDE:** Install the ESP32 board support package and the required libraries (M5StickCPlus2, ESP32-A2DP).
    * **PlatformIO:** Configure your `platformio.ini` for the `m5stick-c-plus` board and include the libraries under `lib_deps`.
2.  **Board Selection:** Select "M5StickCPlus2" (or the appropriate variant) in your IDE's board manager.
3.  **Partition Scheme:** **Crucially**, select a partition scheme that includes **LittleFS** (e.g., "Default 4MB with LittleFS (...)"). If you change the partition scheme, you must re-upload the main sketch *and* the filesystem image.
4.  **Custom Library:** Ensure the `M5MicPeakRMS.h` and `M5MicPeakRMS.cpp` files are accessible to the compiler (either in the sketch folder or a dedicated library folder).
5.  **Project Structure:** Place all provided `.h` and `.cpp` files (config, globals, display_utils, input_handler, timer_modes, audio_utils, bluetooth_utils, nvs_utils, system_utils) alongside the main `.ino` file in your sketch folder.
6.  **LittleFS Data Upload (Optional - for Boot Animation):**
    * Create a folder named `data` in your main sketch directory (Arduino IDE) or project root (PlatformIO).
    * Place your boot animation JPG files inside this `data` folder, named sequentially: `1.jpg`, `2.jpg`, `3.jpg`, etc.
    * Use the appropriate tool to upload the filesystem image:
        * **Arduino IDE:** Install the [LittleFS ESP32 Arduino upload tool](https://github.com/lorol/arduino-esp32fs-plugin) or use the PlatformIO method.
        * **PlatformIO:** Run the "Upload Filesystem Image" task.
7.  **Compile & Upload:** Compile and upload the main sketch (`.ino` file).

## Usage

* **Boot:** The device boots, initializes, optionally plays the boot animation (skip with BtnA).
* **Mode Selection:** Use side buttons (BtnB/PWR) to scroll, front button (BtnA) to select. Battery and Bluetooth connection status [B] shown top-right.
* **Timer Operation (Live/Noisy):**
    * Press BtnA to show "Ready...".
    * Start beep sounds (Buzzer or BT). Timer starts after `POST_BEEP_DELAY_MS`. Microphone listening starts after calculated `beep_audio_end_time`.
    * Shots detected based on mode criteria.
    * Press BtnA again to manually stop.
    * Hold BtnB to exit to Mode Selection.
    * Results screen shows stats. Press BtnA to reset, Hold BtnB to exit.
* **Timer Operation (Dry Fire Par):**
    * Press BtnA to start.
    * "Waiting..." shown during random delay.
    * Beep sequence plays (Buzzer or BT).
    * Hold BtnA or BtnB to cancel.
* **Settings Menu:**
    * Hold BtnB from Mode Select, Ready, or Stopped screens to enter.
    * Navigate with side buttons, Select/Confirm with BtnA short press.
    * Back/Cancel with BtnA long press.
* **Bluetooth Menu:**
    * **Connect:** Attempts connection to the device stored in `currentBluetoothDeviceName`.
    * **Disconnect:** Disconnects the current A2DP device.
    * **Volume:** Adjust BT audio volume.
    * **BT Audio Offset:** Calibrate synchronization between buzzer and BT audio. Press side buttons to adjust offset; a sync tone plays on both outputs simultaneously (if BT connected). Press BtnA to save.
    * **Auto Reconnect:** Toggle auto-connection on startup.
    * **Scan for Devices:** Initiates A2DP discovery for `BT_SCAN_DURATION_S`. Found devices are listed. Select a device with BtnA to save it as the target and attempt connection immediately. Hold BtnA to cancel scan/exit list.

## Filesystem Structure (LittleFS - Optional)

* `/1.jpg`
* `/2.jpg`
* ... (for boot animation)

## Model Printed and Attached to a Blue Gun

* ![Attached](https://github.com/jcarletto27/HeyManNiceShotTimer/blob/main/images/PXL_20250506_164010292.MP.jpg?raw=true)
* ![Mounted in Lanyard Mode](https://github.com/jcarletto27/HeyManNiceShotTimer/blob/main/images/PXL_20250506_164025303.MP.jpg?raw=true)
* ![Closeup of the mounting mechanism](https://github.com/jcarletto27/HeyManNiceShotTimer/blob/main/images/PXL_20250506_164028005.jpg?raw=true)

## Future Enhancements

* Save/Review shot data strings.
* More sophisticated power management.

## Contributing

Contributions are welcome! Please feel free to fork the repository, make changes, and submit pull requests.
