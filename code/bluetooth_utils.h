#ifndef BLUETOOTH_UTILS_H
#define BLUETOOTH_UTILS_H

#include <M5StickCPlus2.h>
#include "BluetoothA2DPSource.h" // Already in globals.h but good for clarity
#include <ESP32BluetoothScanner.h> // Already in globals.h
#include <vector>                  // Already in globals.h
#include "config.h"               // For esp_a2d_connection_state_t if needed directly

// A2DP Callbacks
int32_t get_data_frames(Frame *frames, int32_t frame_count);
void a2dp_connection_state_changed_callback(esp_a2d_connection_state_t state, void *object_instance);
bool a2dp_ssid_callback(const char *ssid, esp_bd_addr_t address, int rrsi);

// Scanner functions
void handleBluetoothScanning();
// displayBluetoothScanResults() is now in display_utils.h/.cpp

#endif // BLUETOOTH_UTILS_H
