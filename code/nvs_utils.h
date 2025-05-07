#ifndef NVS_UTILS_H
#define NVS_UTILS_H

#include <Preferences.h> // Already in globals.h but good for clarity
#include <Arduino.h>     // For String

void loadSettings();
void saveSettings();
void savePeakVoltage(float voltage);

#endif // NVS_UTILS_H
