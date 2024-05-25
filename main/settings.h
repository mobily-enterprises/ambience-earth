#ifndef SETTINGS_H
#define SETTINGS_H

void settings();
void maintenance();
void resetOnlyLogs();
bool isActionActive(int8_t actionId); 
Conditions inputConditions(Conditions *initialConditions, char verb, int8_t choiceId);
void settingsEditActions();
void settingsDefaultMoistLevels();
void settingsCalibrate();
int runInitialSetup();
int calibrateTrayWaterLevelSensors();
int readSensor(int sensor);
int calibrateSoilMoistureSensor();
void setFeedFrom();
void setAutoDrain();
void resetData();
void activatePumps();
void createBootLogEntry();
#endif UI_H