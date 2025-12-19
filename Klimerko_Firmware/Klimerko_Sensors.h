#ifndef KLIMERKO_SENSORS_H
#define KLIMERKO_SENSORS_H

#include "Klimerko_Config.h"
#include "src/AdafruitBME280/Adafruit_Sensor.h"
#include "src/AdafruitBME280/Adafruit_BME280.h"
#include "src/pmsLibrary/PMS.h"
#include "src/movingAvg/movingAvg.h"

void initSensors();
void readPMS();
void readBME();
void pmsPower(bool state);
long readIntervalMillis();

extern float avgTemperature, avgHumidity, avgPressure;
extern int avgPM1, avgPM25, avgPM10;

#endif