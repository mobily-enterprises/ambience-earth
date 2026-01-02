#ifndef SETTINGS_H
#define SETTINGS_H

void settings();
void resetOnlyLogs();
void settingsDefaultMoistLevels();
void settingsReset();
void testSensors();
void calibrateMoistureSensor();
void pumpTest();
void calibrateWeightSensor();
void calibrateFullWeight();
int runInitialSetup();
void runButtonsSetup();
int readSensor(int sensor);
int calibrateSoilMoistureSensor();
void resetData();
void activatePumps();
void createBootLogEntry();

#endif SETTINGS_H
