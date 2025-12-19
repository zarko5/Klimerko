/*  ------------------------------------------- Project "KLIMERKO" ---------------------------------------------
 *  Citizen Air Quality measuring device with cloud monitoring, built at https://descon.me for the whole world.
 *  Programmed, built and maintained by Vanja Stanic // www.vanjastanic.com
 *  ------------------------------------------------------------------------------------------------------------
 *  This is a continued effort from https://descon.me/2018/winning-product/
 *  Supported by ISOC (Internet Society, Belgrade Chapter) // https://isoc.rs
 *  IoT Cloud Services and Communications SDK by AllThingsTalk // www.allthingstalk.com/
 *  3D Case for the device designed and manufactured by Dusan Nikic // nikic.dule@gmail.com
 *  ------------------------------------------------------------------------------------------------------------
 *  This sketch is downloaded from https://github.com/DesconBelgrade/Klimerko
 *  Head over there to read instructions and more about the project.
 *  Do not change anything in here unless you know what you're doing. Just upload this sketch to your device.
 *  You'll configure your WiFi and Cloud credentials once the sketch is uploaded to the device by 
 *  pressing the FLASH button on the NodeMCU for 2 seconds and connecting to Klimerko using any WiFi-enabled device.
 *  ------------------------------------------------------------------------------------------------------------
 *  Textual Air Quality Scale is based on PM10 criteria defined by RS Government (http://www.amskv.sepa.gov.rs/kriterijumi.php)
 *  Excellent (0-20), Good (21-40), Acceptable (41-50), Polluted (51-100), Very Polluted (Over 100)
 */


#include "Klimerko_Config.h"
#include "Klimerko_Network.h"
#include "Klimerko_Sensors.h"
#include "src/AdafruitBME280/Adafruit_Sensor.h"
#include "src/AdafruitBME280/Adafruit_BME280.h"
#include "src/pmsLibrary/PMS.h"
#include "src/movingAvg/movingAvg.h"
#include "src/WiFiManager/WiFiManager.h"
#include "src/PubSubClient/PubSubClient.h"
#include "src/ArduinoJson-v6.18.5.h"
#include <SoftwareSerial.h>
#include <Wire.h>
#include <EEPROM.h>

 
void changeInterval(int interval) { // Changes sensor data reporting interval
  if (interval > 5 && interval <= 60) {
    dataPublishInterval = interval;
    pmsNoSleep = false;
    Serial.print("[DATA] Device reporting interval set to ");
    Serial.print(interval);
    Serial.println(" minutes");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    publishDiagnosticData();
  } else if (interval <= 1) {
    dataPublishInterval = 1;
    pmsNoSleep = true;
    pmsPower(true);
    Serial.println("[DATA] Reporting interval set to 1 minute (minimum).");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    Serial.println("[DATA] This prevents sleeping of Air Quality Sensor and reduces its lifespan.");
    publishDiagnosticData();
  } else if (interval <= 5) {
    dataPublishInterval = interval;
    pmsNoSleep = true;
    pmsPower(true);
    Serial.print("[DATA] Device reporting interval set to ");
    Serial.print(interval);
    Serial.println(" minutes");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    Serial.println("[DATA] This prevents sleeping of Air Quality Sensor and reduces its lifespan.");
    publishDiagnosticData();
  } else if (interval >= 60) {
    dataPublishInterval = 60;
    pmsNoSleep = false;
    Serial.print("[DATA] Device reporting interval set to ");
    Serial.print(dataPublishInterval);
    Serial.println(" minutes");
    Serial.print("[DATA] Sensor data will be read every ");
    Serial.print(readIntervalSeconds());
    Serial.println(" seconds for averaging.");
    publishDiagnosticData();
  }
}

void publishDiagnosticData() { // Publishes diagnostic data to AllThingsTalk
  if (!wifiConnectionLost) {
    if (!mqttConnectionLost) {
      char JSONmessageBuffer[256];
      DynamicJsonDocument doc(256);
      JsonObject dataPublishIntervalJson = doc.createNestedObject(INTERVAL_ASSET);
      dataPublishIntervalJson["value"] = dataPublishInterval;
      JsonObject firmwareJson = doc.createNestedObject(FIRMWARE_ASSET);
      firmwareJson["value"] = firmwareVersion;
      JsonObject wifiJson = doc.createNestedObject(WIFI_SIGNAL_ASSET);
      wifiJson["value"] = wifiSignal();
      JsonObject tempOffsetJson = doc.createNestedObject(TEMP_OFFSET_ASSET);
      tempOffsetJson["value"] = bmeTemperatureOffset;
      serializeJson(doc, JSONmessageBuffer);
    
      char topic[256];
      snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceId, "/state");
      mqtt.publish(topic, JSONmessageBuffer, false);
      Serial.print("[DATA] Published diagnostic data to AllThingsTalk: ");
      Serial.println(JSONmessageBuffer);
    } else {
      Serial.println("[DATA] Can't send diagnostic data because Klimerko is not connected to AllThingsTalk");
    }
  } else {
    Serial.println("[DATA] Can't send diagnostic data because Klimerko is not connected to WiFi");
  }
}

unsigned long readIntervalMillis() {
  unsigned long result = (dataPublishInterval * 60000) / sensorAverageSamples;
  return result;
}

int readIntervalSeconds() {
  int result = (dataPublishInterval * 60) / sensorAverageSamples;
  return result;
}

void restoreData() { // Restores AllThingsTalk credentials from EEPROM as well as temperature offset data
  char okCreds[2+1];
  char okOffset[2+1];
  EEPROM.begin(EEPROMsize);
  EEPROM.get(EEPROM_attStartAddress, deviceId);
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId), deviceToken);
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken), okCreds);
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken)+sizeof(okCreds), bmeTemperatureOffsetChar);
  EEPROM.get(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken)+sizeof(okCreds)+sizeof(bmeTemperatureOffsetChar), okOffset);
  EEPROM.end();
  if (String(okCreds) != String("OK")) {
    deviceId[0] = 0;
    deviceToken[0] = 0;
    Serial.println("[MEMORY] AllThingsTalk Device ID: Nothing in Memory");
  } else {
    Serial.print("[MEMORY] AllThingsTalk Device ID: ");
    Serial.println(deviceId);
//    portalDeviceID.setValue(deviceId, sizeof(deviceId)); // Set WiFi Configuration Portal to show real value
//    Serial.print("[MEMORY] AllThingsTalk Device Token: ");
//    Serial.println(deviceToken);
//    portalDeviceToken.setValue(deviceToken, sizeof(deviceToken)); // Set WiFi Configuration Portal to show real value
  }
  if (String(okOffset) != String("OK")) {
    Serial.print("[MEMORY] Temperature Offset: Nothing in Memory. Using default: ");
    Serial.println(bmeTemperatureOffset);
    portalTemperatureOffset.setValue(bmeTemperatureOffsetChar, sizeof(bmeTemperatureOffsetChar));
  } else {
    bmeTemperatureOffset = atof(bmeTemperatureOffsetChar); // Store the char that was in memory as a double (lazy)
    portalTemperatureOffset.setValue(bmeTemperatureOffsetChar, sizeof(bmeTemperatureOffsetChar)); // Update the value on WiFi Configuration Portal
    Serial.print("[MEMORY] Temperature Offset: ");
    Serial.print(bmeTemperatureOffsetChar);
    Serial.print("°C (Float: ");
    Serial.print(bmeTemperatureOffset);
    Serial.println("°C)");
  }
}

void saveData() { // Saves new ATT credentials in memory and connects to AllThingsTalk
  Serial.println("[MEMORY] Saving data in persistent memory...");
  bool deviceIdCanBeSaved = false;
  bool deviceTokenCanBeSaved = false;
  bool tempOffsetCanBeSaved = false;

  if (sizeof(portalDeviceID.getValue()) >= sizeof(deviceId)) {
    Serial.print("[MEMORY] Won't save Device ID '");
    Serial.print(portalDeviceID.getValue());
    Serial.println("' because it's too long");
  } else if (String(portalDeviceID.getValue()) == "") {
    Serial.println("[MEMORY] Won't save Device ID because it's empty.");
  } else if (String(portalDeviceID.getValue()) == String(deviceId)) {
    Serial.print("[MEMORY] Won't save Device ID '");
    Serial.print(portalDeviceID.getValue());
    Serial.println("' because it's the same as the current one.");
  } else {
    sprintf(deviceId, "%s", portalDeviceID.getValue());
    Serial.print("[MEMORY] Saving Device ID: ");
    Serial.println(deviceId);
    deviceIdCanBeSaved = true;
  }

  if (sizeof(portalDeviceToken.getValue()) >= sizeof(deviceToken)) {
    Serial.print("[MEMORY] Won't save Device Token '");
    Serial.print(portalDeviceToken.getValue());
    Serial.println("' because it's too long");
  } else if (String(portalDeviceToken.getValue()) == "") {
    Serial.println("[MEMORY] Won't save Device Token because it's empty.");
  } else if (String(portalDeviceToken.getValue()) == String(deviceToken)) {
    Serial.print("[MEMORY] Won't save Device Token '");
    Serial.print(portalDeviceToken.getValue());
    Serial.println("' because it's the same as the current one.");
  } else {
    sprintf(deviceToken, "%s", portalDeviceToken.getValue());
    Serial.print("[MEMORY] Saving Device Token: ");
    Serial.println(deviceToken);
    deviceTokenCanBeSaved = true;
  }

  if (sizeof(portalTemperatureOffset.getValue()) >= sizeof(bmeTemperatureOffsetChar)) {
    Serial.print("[MEMORY] Won't save Temperature Offset '");
    Serial.print(portalTemperatureOffset.getValue());
    Serial.println("' because it's too long.");
  } else if (String(portalTemperatureOffset.getValue()) == String(bmeTemperatureOffsetChar)) {
    Serial.println("[MEMORY] Won't save Temperature Offset because it's the same as current value.");
  } else if (!isNumber(portalTemperatureOffset.getValue())) {
    Serial.print("[MEMORY] Won't save Temperature Offset '");
    Serial.print(portalTemperatureOffset.getValue());
    Serial.println("' because it's not a number.");
  } else if (atof(portalTemperatureOffset.getValue()) > bmeTemperatureOffsetMax) {
    Serial.print("[MEMORY] Won't save Temperature Offset '");
    Serial.print(portalTemperatureOffset.getValue());
    Serial.print("' because it's above the maximum of ");
    Serial.println(bmeTemperatureOffsetMax);
  } else if (atof(portalTemperatureOffset.getValue()) < bmeTemperatureOffsetMin) {
    Serial.print("[MEMORY] Won't save Temperature Offset '");
    Serial.print(portalTemperatureOffset.getValue());
    Serial.print("' because it's below the minimum of ");
    Serial.println(bmeTemperatureOffsetMin);
  } else if (String(portalTemperatureOffset.getValue()) == "") {
    Serial.println("[MEMORY] Won't save Temperature Offset because it's empty.");
  } else {
    sprintf(bmeTemperatureOffsetChar, "%s", portalTemperatureOffset.getValue()); // Convert const char* to char array for saving in memory
    portalTemperatureOffset.setValue(bmeTemperatureOffsetChar, sizeof(bmeTemperatureOffsetChar)); // Set WiFi Configuration Portal to show the real value of offset
    bmeTemperatureOffset = atof(bmeTemperatureOffsetChar); // Convert the entered value to double (even though the variable is a float - I know, I know...)
    Serial.print("[MEMORY] Saving Temperature Offset: ");
    Serial.print(bmeTemperatureOffsetChar);
    Serial.print("°C (Float: ");
    Serial.print(bmeTemperatureOffset);
    Serial.println("°C)");
    // Reset average temperature and humidity values in case the offset was changed during device operation since already-existing averaging data would be wrong due to new temperature offset.
    temp.reset();
    hum.reset();
    tempOffsetCanBeSaved = true;
  }

  portalTemperatureOffset.setValue(bmeTemperatureOffsetChar, sizeof(bmeTemperatureOffsetChar)); // Set WiFi Configuration Portal to show real value (in case user entered it wrong and it was disregarded)
  
  if (deviceIdCanBeSaved || deviceTokenCanBeSaved || tempOffsetCanBeSaved) {
    char ok[2+1] = "OK";
    EEPROM.begin(EEPROMsize);
    if (deviceIdCanBeSaved || deviceTokenCanBeSaved) {
      if (deviceIdCanBeSaved) {
        EEPROM.put(EEPROM_attStartAddress, deviceId);
      }
      if (deviceTokenCanBeSaved) {
        EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId), deviceToken);
      }
      EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken), ok);
    }
    if (tempOffsetCanBeSaved) {
      EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken)+sizeof(ok), bmeTemperatureOffsetChar);
      EEPROM.put(EEPROM_attStartAddress+sizeof(deviceId)+sizeof(deviceToken)+sizeof(ok)+sizeof(bmeTemperatureOffsetChar), ok);
    }
    if (EEPROM.commit()) {
      Serial.println("[MEMORY] Data saved.");
      EEPROM.end();
    } else {
      Serial.println("[MEMORY] Data couldn't be saved to memory.");
      EEPROM.end();
    }
  }
}

bool isNumber(const char* value) {
  if (value[0] != '-' && value[0] != '+' && !isDigit(value[0])) {
    return false;
  }
  if (value[1] != '\0') {
    int i = 1;
    do {
      if (!isDigit(value[i]) && value[i] != '.') {
        return false;
      }
      i++;
    } while(value[i] != '\0');
  }
  return true;
}

void connectAfterSavingData() {
  connectMQTT();
}

void factoryReset() { // Deletes WiFi and AllThingsTalk credentials and reboots Klimerko
  for (int i=0;i<40;i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(50);
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
  }
  wm.resetSettings();
  ESP.eraseConfig();
  EEPROM.begin(EEPROMsize);
  for (int i=EEPROM_attStartAddress; i <= sizeof(deviceId)+sizeof(deviceToken)+3+sizeof(bmeTemperatureOffsetChar)+3; i++) {
    EEPROM.write(i, 0);
  }
  EEPROM.commit();
  EEPROM.end();
  Serial.println("[SYSTEM] Klimerko has been factory reset. All data has been erased. Rebooting in 5 seconds.");
  delay(5000);
  ESP.restart();
}

void wifiConfigWebServerStarted() {
  wm.server->on("/exit", wifiConfigStop); // If user presses Exit, turn off WiFi Configuration Portal.
}

void wifiConfigStarted(WiFiManager *wm) { // Called once WiFi Configuration Portal is started
  wifiConfigActiveSince = millis();
}

void wifiConfigStart() { // Starts WiFi Configuration Portal
  if (!wm.getConfigPortalActive()) {
    Serial.println("[WIFICONFIG] Entering WiFi Configuration Mode...");
    wm.startConfigPortal(klimerkoID, wifiConfigPortalPassword);
    Serial.println("[WIFICONFIG] WiFi Configuration Mode Activated!");
  } else {
    Serial.println("[WIFICONFIG] WiFi Configuration Mode already active!");
  }
  Serial.print("[WIFICONFIG] Use your computer or smartphone to connect to WiFi network '");
  Serial.print(klimerkoID);
  Serial.print("' (password: '");
  Serial.print(wifiConfigPortalPassword);
  Serial.println("') to configure your Klimerko.");
}

void wifiConfigStop() { // Stops WiFi Configuration Portal
  if (wm.getConfigPortalActive()) {
    wm.stopConfigPortal();
    Serial.println("[WIFICONFIG] WiFi Configuration Portal has been stopped.");
  } else {
    Serial.println("[WIFICONFIG] Can't stop WiFi Configuration Portal because it's not running.");
  }
}

void buttonLoop() { // Handles the FLASH button and all it's features
  buttonCurrentState = digitalRead(BUTTON_PIN);
  if (buttonLastState == HIGH && buttonCurrentState == LOW) {
    buttonPressedTime = millis();
    buttonPressed = true;
    buttonLongPressDetected = false;
    // Button is being pressed at the moment
  } else if (buttonLastState == LOW && buttonCurrentState == HIGH) {
    buttonReleasedTime = millis();
    buttonPressed = false;
    long buttonPressDuration = buttonReleasedTime - buttonPressedTime;
    if (buttonPressDuration > buttonShortPressTime && buttonPressDuration < buttonMediumPressTime && buttonPressDuration < buttonLongPressTime) {
      Serial.println("[BUTTON] Short Press Detected!");
      // wifiConfigStop(); // Removed as per user request to use short press for display toggle
      displayMode = (displayMode + 1) % 4;
      updateDisplay(); // Update immediately
    } else if (buttonPressDuration > buttonShortPressTime && buttonPressDuration > buttonMediumPressTime && buttonPressDuration < buttonLongPressTime) {
      Serial.println("[BUTTON] Long Press Detected!");
      wifiConfigStart();
    }
  }

  if (buttonPressed && !buttonLongPressDetected) {
    if (millis() - buttonPressedTime > buttonLongPressTime) {
      buttonLongPressDetected = true;
      Serial.println("[BUTTON] Super Long Press Detected!");
      factoryReset();
    }
  }
  buttonLastState = buttonCurrentState;
}

void ledLoop() { // Handles status LED
  if (ledSuccessBlink) {
    for (int i=0;i<6;i++) {
      digitalWrite(LED_BUILTIN, LOW);
      delay(100);
      digitalWrite(LED_BUILTIN, HIGH);
      delay(100);
    }
    ledSuccessBlink = false;
  }
  
  if (wm.getConfigPortalActive()) {
    ledState = true;
  } else {
    if (wifiConnectionLost || mqttConnectionLost) {
      if (millis() - ledLastUpdate >= ledBlinkInterval) {
        ledState = !ledState;
        ledLastUpdate = millis();
      }
    } else {
      ledState = false;
    }
  }
  
  if (ledState) {
    digitalWrite(LED_BUILTIN, LOW);
  } else {
    digitalWrite(LED_BUILTIN, HIGH);
  }
}

String extractAssetNameFromTopic(String topic) {
  const int devicePrefixLength = 38;
  const int stateSuffixLength = 8;
  return topic.substring(devicePrefixLength, topic.length()-stateSuffixLength);
}

void initPMS() {
  pmsSerial.begin(9600);
  pms.passiveMode(); // ovde treba da se settuje passive, kao u primerima od originalne PMS biblioteke
  pmsPower(true);

}

void initBME() {
  bme.begin(0x76);
}

void generateID() {
  snprintf(klimerkoID, sizeof(klimerkoID), "%s%i", "KLIMERKO-", ESP.getChipId());
  Serial.print("[ID] Unique Klimerko ID: ");
  Serial.println(klimerkoID);
}

void initAvg() {
  pm1.begin();
  pm25.begin();
  pm10.begin();
  temp.begin();
  hum.begin();
  pres.begin();
}

void initPins() {
  pinMode(BUTTON_PIN, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}


void setup()  {
  Serial.begin(115200);
  Serial.println("");
  Serial.println(" ------------------------------ Project 'KLIMERKO' ------------------------------");
  Serial.println("|                  https://github.com/DesconBelgrade/Klimerko                    |");
  Serial.println("|                               www.klimerko.org                                 |");
  Serial.print("|                           Firmware Version: ");
  Serial.print(firmwareVersion);
  Serial.println("                              |");
  Serial.println("|    Hold NodeMCU FLASH button for 2 seconds to enter WiFi Configuration Mode.   |");
  Serial.print("| Sensors are read every ");
  Serial.print(readIntervalSeconds());
  Serial.print(" seconds and averages are published every ");
  Serial.print(dataPublishInterval);
  Serial.println(" minutes. |");
  Serial.println(" --------------------------------------------------------------------------------");
  initAvg();
  initPins();
  initPMS();
  initBME();
  generateID();
  restoreData();
  initWiFi();
  initMQTT();

  Wire.begin(0, 2);  // SDA, SCL
  display.begin(OLED_ADDR, true);
  display.clearDisplay();

  // Startup Animation
  for(int i=0; i<64; i+=2) {
      display.drawCircle(64, 64, i, SH110X_WHITE);
      display.display();
      delay(10);
  }
  display.fillScreen(SH110X_WHITE);
  display.display();
  delay(100);
  display.clearDisplay();
  display.setTextColor(SH110X_WHITE);
  drawCenteredText("KLIMERKO", 64, 64, &FreeSansBold12pt7b);
  display.display();
  delay(2000);
  
  Serial.println("");
}

void loop() {
  sensorLoop();
  maintainWiFi();
  maintainMQTT();
  wifiConfigLoop();
  buttonLoop();
  ledLoop();  
  updateDisplay();
}