#ifndef AUDIO_UTILS_H
#define AUDIO_UTILS_H

#include <M5StickCPlus2.h> // For M5 object if used directly, or Arduino types

// Function to reset Bluetooth beep state variables
void reset_bt_beep_state();

// Plays a tone, APPLYING the global Bluetooth audio offset.
// Used for critical timing sounds like timer start beeps.
void playTone(int freq, int duration);

// Plays a tone for immediate UI feedback, IGNORING the global Bluetooth audio offset.
// Schedules BT audio to start as soon as possible.
void playFeedbackTone(int freq, int duration);

// Plays a sequence of tones for success feedback.
void playSuccessBeeps();

// Plays a sequence of tones for unsuccessful/error feedback.
void playUnsuccessBeeps();

// Plays a synchronized tone on buzzer and Bluetooth for calibration.
// 'offsetMs' is the value being tested by the user.
void playSyncCalibrationTone(int freq, int duration, int offsetMs); 

#endif // AUDIO_UTILS_H
