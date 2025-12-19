
#ifndef KLIMERKO_NETWORK_H
#define KLIMERKO_NETWORK_H

#include "Klimerko_Config.h"
#include "src/WiFiManager/WiFiManager.h"
#include "src/PubSubClient/PubSubClient.h"

void initWiFi();
void initMQTT();
void maintainWiFi();
void maintainMQTT();
void publishAllData();
void mqttCallback(char* topic, byte* payload, unsigned int length);


extern WiFiManager wm;
extern PubSubClient mqtt;

WiFiManagerParameter portalDeviceID("device_id", "AllThingsTalk Device ID", deviceId, 32);
WiFiManagerParameter portalDeviceToken("device_token", "AllThingsTalk Device Token", deviceToken, 64);
WiFiManagerParameter portalTemperatureOffset("temperature_offset", "Temperature Offset", bmeTemperatureOffsetChar, 8);
WiFiManagerParameter portalDisplayFirmwareVersion(firmwareVersionPortal);
WiFiManagerParameter portalDisplayCredits("Firmware Designed and Developed by Vanja Stanic");
WiFiClient networkClient;
PubSubClient mqtt(networkClient);

#endif