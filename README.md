# ESP32-C3 I2S Marksman Shot Timer

This project implements a configurable marksman shot timer using a Seeed Studio XIAO ESP32C3, an I2S microphone (INMP441), an SSD1306 OLED display, two buttons, and a piezo buzzer.

It provides a random start delay, measures the time to the first shot after the start beep, and records split times between subsequent shots up to a configurable maximum.

## Features

* **Random Start Delay:** Initiates the timing sequence with a random delay (configurable range, default 1-3 seconds).
* **Loud Start Beep:** Emits a clear start signal using a piezo buzzer (frequency and duration configurable).
* **First Shot Time:** Accurately measures the time from the start beep to the first detected shot using an I2S microphone.
* **Split Times:** Records the time between consecutive shots.
* **Configurable Max Shots:** Set the maximum number of shots to record per sequence via an on-device menu.
* **Configurable Beep:** Adjust the duration (ms) and frequency (Hz) of the start beep via the on-device menu.
* **OLED Display:** Shows current status (Ready, Waiting, Timing), shot times (first shot, last split), total shots, and settings menu.
* **Ambient Noise Calibration:** Measures background noise on startup to improve reliability (though primary detection uses a fixed RMS threshold).
* **Shot Lockout:** Prevents registering echoes or noise immediately after a shot as a new shot (configurable duration).
* **Timing Timeout:** Automatically stops the timer and displays results if no shot is detected within 30 seconds.
* **Persistent Settings:** Saves Max Shots, Beep Duration, and Beep Frequency settings to Non-Volatile Storage (NVS) so they persist across resets.
* **Button Interface:**
    * Button 1 (Black): Start Timer / Reset from Results / Increase Value or Cycle Next in Settings.
    * Button 2 (Green): Stop Timer / Decrease Value or Cycle Previous in Settings / Hold from Idle to Enter Settings.

## Hardware Components

* **Microcontroller:** Seeed Studio XIAO ESP32C3
* **Microphone:** I2S MEMS Microphone (e.g., INMP441)
* **Display:** 128x64 SSD1306 OLED Display (I2C interface)
* **Buttons:** 2 x Tactile Push Buttons
* **Buzzer:** Piezo Buzzer
* **Wiring:** Jumper wires, potentially pull-down resistors (if not using internal `INPUT_PULLDOWN`). (*Note: The current code uses `INPUT_PULLDOWN`*)

## Required Arduino Libraries

* **Wire:** (Built-in) For I2C communication with the display.
* **Adafruit GFX Library:** Install via Arduino Library Manager.
* **Adafruit SSD1306:** Install via Arduino Library Manager.
* **Preferences:** (Built-in for ESP32) For saving settings to NVS.

*(ESP32 Board Support Package must be installed in the Arduino IDE)*

## Setup & Wiring

1.  **Install Libraries:** Ensure all the required libraries listed above are installed in your Arduino IDE.
2.  **Board Selection:** Select the correct XIAO ESP32C3 board in the Arduino IDE.
3.  **Wiring:** Connect the components according to the `#define` statements at the top of the `.ino` file:
    * **Buttons:** Connect Button 1 to D0 (GPIO1) and GND. Connect Button 2 to D1 (GPIO2) and GND. (Code uses `INPUT_PULLDOWN`, so no external resistors needed).
    * **Buzzer:** Connect one leg of the buzzer to D6 (GPIO7) and the other leg to D3 (GPIO4) for differential drive (or one leg to D6 and the other to GND if preferred, adjust code accordingly).
    * **OLED Display:** Connect SDA to D4 (GPIO5), SCL to D5 (GPIO6), VCC to 3V3, and GND to GND.
    * **I2S Microphone (INMP441 Example):**
        * SCK (BCLK) to D8 (GPIO8)
        * WS (LRCL) to D9 (GPIO9)
        * SD (DOUT) to D10 (GPIO10)
        * VCC to 3V3
        * GND to GND

## Usage

1.  **Power On:** The device will boot, display a splash screen, and perform ambient noise calibration. *Keep the area quiet during calibration.*
2.  **Calibration Success/Failure:** If calibration succeeds, success beeps will play, and the "READY" screen appears. If it fails, a "CALIB FAILED!" message is shown, and the device halts.
3.  **Start Timer:** Press Button 1 (Black) on the "READY" screen.
4.  **Waiting:** The display shows "Waiting..." during the random delay period.
5.  **Beep & Timing:** The timer beeps (using configured settings), and the display shows "Shoot!". Timing starts precisely when the beep begins.
6.  **Shot Detection:** The timer listens for sounds exceeding the `SHOT_THRESHOLD_RMS`. The time to the first shot and subsequent split times are recorded and briefly displayed.
7.  **Stop Timing:**
    * The timer stops automatically after the `currentMaxShots` number of shots are recorded.
    * Press Button 2 (Green) at any time during the "Timing..." phase to manually stop.
    * The timer stops automatically if 30 seconds pass without a shot being registered after the beep.
8.  **Display Results:** The screen shows the time of the first shot and the last recorded split time, along with the total number of shots detected.
9.  **Reset:** Press Button 1 (Black) from the results screen to return to the "READY" screen.
10. **Settings Menu:**
    * From the "READY" screen, **press and hold** Button 2 (Green) for >1 second to enter the settings menu.
    * The current setting being edited is shown (Max Shots, Beep MS, Beep Hz).
    * **Short press** Button 1 (Black) to increase the value.
    * **Short press** Button 2 (Green) to decrease the value.
    * **Hold** Button 1 (Black) to cycle to the *next* setting.
    * **Hold** Button 2 (Green) to cycle to the *previous* setting.
    * Wait 5 seconds without pressing any buttons to automatically save all settings and return to the "READY" screen.

## Tuning

* **`SHOT_THRESHOLD_RMS`:** This value is crucial for reliable shot detection. The default value is a placeholder based on a previous calculation and **must be tuned** for your specific microphone, environment, and firearm caliber.
    * Temporarily uncomment the `Serial.printf("RMS: %.2f\n", currentRms);` line in the `readMicrophone()` function.
    * Run the timer and observe the RMS values printed in the Serial Monitor for actual shots vs. background noise or handling noise.
    * Set `SHOT_THRESHOLD_RMS` to a value slightly below the typical RMS reading of a real shot but well above ambient noise and handling sounds.
* **`SHOT_LOCKOUT_MICROS`:** Adjust this value (in microseconds) if the timer registers echoes as separate shots (increase lockout) or if it misses very fast follow-up shots (decrease lockout, carefully).

