#ifndef SETTINGS_H
#define SETTINGS_H

void settings();
void initialSetupMenu();
bool initialSetupComplete();
void environmentMenu();
void saveConfigDone();
void resetOnlyLogs();
void settingsReset();
void calibrateMoistureSensor();
void calibrateDripperFlow();
void pumpTest();
void testSensors();
int runInitialSetup();
void runButtonsSetup();
int calibrateSoilMoistureSensor();
void resetData();
void createBootLogEntry();

#endif SETTINGS_H
