#ifndef SETTINGS_H
#define SETTINGS_H

void settings();
void resetOnlyLogs();
void settingsDefaultMoistLevels();
void settingsReset();
void startCalibrationTask();
void startPumpTestTask();
void testSensors();
void calibrateMoistureSensorBlocking();
void pumpTestBlocking();
int runInitialSetup();
void runButtonsSetup();
int readSensor(int sensor);
int calibrateSoilMoistureSensor();
void resetData();
void activatePumps();
void createBootLogEntry();

#endif SETTINGS_H
