#ifndef INPUT_HANDLER_H
#define INPUT_HANDLER_H

#include <M5StickCPlus2.h>
#include "config.h" // For TimerState enum

void handleModeSelectionInput();
void handleSettingsInput();
void handleEditSettingInput();
void handleDeviceStatusInput();
void handleListFilesInput();
void handleCalibrationInput(TimerState calibrationType);
bool checkTimerExitButtons(); // Though its logic is now mainly global

#endif // INPUT_HANDLER_H
