#ifndef CONFIG_H
#define CONFIG_H

#include "src/WiFiManager/WiFiManager.h"   
#include "src/ArduinoJson-v6.18.5.h"       

// ---------------------------- Pins -----------------------------------------------------

#define BUTTON_PIN     0
#define pmsTX          14 // GPIO14, was D5
#define pmsRX          12 // GPIO12, was D6

// ------------------------- Device -----------------------------------------------------
String         firmwareVersion         = "2.2.0";
const char*    firmwareVersionPortal   = "<p>Firmware Version: 2.2.0</p>";
char           klimerkoID[32];

// -------------------------- WiFi ------------------------------------------------------
const int      wifiReconnectInterval   = 60;
bool           wifiConnectionLost      = true;
unsigned long  wifiReconnectLastAttempt;

// ------------------- WiFi Configuration Portal ----------------------------------------
char const     *wifiConfigPortalPassword = "ConfigMode"; // Password for WiFi Configuration Portal WiFi Network
const int      wifiConfigTimeout         = 1800;         // Seconds before WiFi Configuration expires
unsigned long  wifiConfigActiveSince;

// -------------------------- MQTT ------------------------------------------------------
const char*    MQTT_SERVER             = "api.allthingstalk.io";
const uint16_t MQTT_PORT               = 1883;
const char*    MQTT_PASSWORD           = "arbitrary";
uint16_t       MQTT_MAX_MESSAGE_SIZE   = 2048;
char           deviceId[32], deviceToken[64];

const int      mqttReconnectInterval   = 30; // Seconds between retries
bool           mqttConnectionLost      = true;
unsigned long  mqttReconnectLastAttempt;

char*          PM1_ASSET               = "pm1";
char*          PM2_5_ASSET             = "pm2-5";
char*          PM10_ASSET              = "pm10";
char*          AQ_ASSET                = "air-quality";
char*          TEMPERATURE_ASSET       = "temperature";
char*          TEMP_OFFSET_ASSET       = "temperature-offset";
char*          HUMIDITY_ASSET          = "humidity";
char*          PRESSURE_ASSET          = "pressure";
char*          INTERVAL_ASSET          = "interval";
char*          FIRMWARE_ASSET          = "firmware";
char*          WIFI_SIGNAL_ASSET       = "wifi-signal";

// -------------------------- BUTTON ------------------------------------------------------
const int      buttonLongPressTime     = 15000; // (milliseconds) Everything above this is considered a long press
const int      buttonMediumPressTime   = 1000;  // (milliseconds) Everything above this and below long press time is considered a medium press
const int      buttonShortPressTime    = 50;    // (milliseconds) Everything above this and below medium press time is considered a short press
unsigned long  buttonPressedTime       = 0;
unsigned long  buttonReleasedTime      = 0;
bool           buttonPressed           = false;
bool           buttonLongPressDetected = false;
int            buttonLastState         = HIGH;
int            buttonCurrentState;

// -------------------------- LED ---------------------------------------------------------
bool           ledState                = false;
bool           ledSuccessBlink         = false;
const int      ledBlinkInterval        = 1000;  // (milliseconds) How often to blink LED when there's no connection
unsigned long  ledLastUpdate;

// --------------------- SENSORS (GENERAL) ------------------------------------------------
uint8_t        dataPublishInterval     = 15;    // [MINUTES] Default sensor data sending interval
const uint8_t  sensorAverageSamples    = 10;    // Number of samples used to average values from sensors
const int      sensorRetriesUntilConsideredOffline = 3;
bool           dataPublishFailed       = false; // Keeps track if a payload has failed to send so we can retry
unsigned long  sensorReadTime, dataPublishTime;

// -------------------------- PMS7003 -----------------------------------------------------
const uint8_t  pmsWakeBefore           = 30;    // [SECONDS] Seconds PMS sensor should be active before reading it
bool           pmsSensorOnline         = true;
int            pmsSensorRetry          = 0;
bool           pmsNoSleep              = false;
bool           pmsWoken                = false;
const char     *airQuality, *airQualityRaw;
int            avgPM1, avgPM25, avgPM10;

// -------------------------- BME280 -----------------------------------------------------
bool           bmeSensorOnline                = true;
int            bmeSensorRetry                 = 0;
float          bmeTemperatureOffset           = -4;   // Default temperature offset
char           bmeTemperatureOffsetChar[8];           // Used for WiFi Configuration Portal and memory
const char     bmeTemperatureOffsetDefault[8] = "-4"; // Used for WiFi Configuration Portal and memory
const int      bmeTemperatureOffsetMax        = 25;
const int      bmeTemperatureOffsetMin        = -25;
float          avgTemperature, avgHumidity, avgPressure;

// -------------------------- MEMORY -----------------------------------------------------
const uint16_t EEPROM_attStartAddress  = 0;
const uint16_t EEPROMsize              = 256;

#endif