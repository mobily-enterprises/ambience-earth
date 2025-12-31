#ifndef SETTINGS_H
#define SETTINGS_H

void settings();
void resetOnlyLogs();
void settingsDefaultMoistLevels();
void settingsReset();
bool uiTaskActive();
void runUiTask();
void startCalibrationTask();
void startPumpTestTask();
void testSensors();
int runInitialSetup();
void runButtonsSetup();
int readSensor(int sensor);
int calibrateSoilMoistureSensor();
void resetData();
void activatePumps();
void createBootLogEntry();

#endif SETTINGS_H
