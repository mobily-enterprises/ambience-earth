#ifndef SETTINGS_H
#define SETTINGS_H

void settings();
void maintenance();
void resetOnlyLogs();
void settingsDefaultMoistLevels();
void settingsReset();
bool uiTaskActive();
void runUiTask();
void startCalibrationTask();
void startPumpTestTask();
int runInitialSetup();
void runButtonsSetup();
int readSensor(int sensor);
int calibrateSoilMoistureSensor();
void resetData();
void activatePumps();
void createBootLogEntry();
void testSensors();
#endif UI_H
