#include "Klimerko_Network.h"
#include "Klimerko_Sensors.h"


WiFiClient espClient;
PubSubClient mqtt(espClient);
WiFiManager wm;

WiFiManagerParameter portalDeviceID("device_id", "AllThingsTalk Device ID", deviceId, 32);
WiFiManagerParameter portalDeviceToken("device_token", "AllThingsTalk Device Token", deviceToken, 64);
WiFiManagerParameter portalTemperatureOffset("temperature_offset", "Temperature Offset", bmeTemperatureOffsetChar, 8);
WiFiManagerParameter portalDisplayFirmwareVersion(firmwareVersionPortal);
WiFiManagerParameter portalDisplayCredits("Firmware Designed and Developed by Vanja Stanic");

WiFiClient networkClient;
PubSubClient mqtt(networkClient);

void wifiConfigLoop() { // Keep WiFi Configurartion mode portal in the loop if it's supposed to be active
  if (wm.getConfigPortalActive()) {
    wm.process();
     if (millis() - wifiConfigActiveSince >= wifiConfigTimeout * 1000) {
       Serial.println("[WIFICONFIG] WiFi Configuration Mode Expired.");
       wifiConfigStop();
     }
  }
}

bool connectWiFi() {
  Serial.print("[WiFi] Connecting to WiFi... ");
  if(!wm.autoConnect(klimerkoID, wifiConfigPortalPassword)) {
    Serial.print("Failed! Reason: ");
    Serial.println(WiFi.status());
    wifiConnectionLost = true;
    return false;
  } else {
    Serial.print("Connected! IP: ");
    Serial.println(WiFi.localIP());
    wifiConnectionLost = false;
    ledSuccessBlink = true;
    return true;
  }
}

void initWiFi() {
  wm.setDebugOutput(false);
  wm.addParameter(&portalDeviceID);
  wm.addParameter(&portalDeviceToken);
  wm.addParameter(&portalTemperatureOffset);
  wm.addParameter(&portalDisplayFirmwareVersion);
  wm.addParameter(&portalDisplayCredits);
  wm.setSaveParamsCallback(saveData);
  wm.setSaveConfigCallback(connectAfterSavingData);
  wm.setWebServerCallback(wifiConfigWebServerStarted);
  wm.setAPCallback(wifiConfigStarted);
  wm.setConfigPortalBlocking(false);
  wm.setConfigPortalTimeout(wifiConfigTimeout);
  wm.setConnectRetries(2);
  wm.setConnectTimeout(5);
  wm.setDarkMode(true);
  wm.setTitle("Klimerko");
  wm.setHostname(klimerkoID);
  wm.setCountry("RS");
  wm.setEnableConfigPortal(false);
  wm.setParamsPage(false);
  wm.setSaveConnect(true);
  wm.setBreakAfterConfig(true);
  wm.setWiFiAutoReconnect(true);
  WiFi.mode(WIFI_STA);
  Serial.print("[MEMORY] WiFi SSID: ");
  if (wm.getWiFiIsSaved()) {
    Serial.println((String)wm.getWiFiSSID());
  } else {
    Serial.println("Nothing in Memory");
  }
  connectWiFi();
}

void maintainWiFi() {
  if (WiFi.status() == WL_CONNECTED) {
    if (wifiConnectionLost) {
      Serial.print("[WiFi] Connection Re-Established! IP: ");
      Serial.println(WiFi.localIP());
      wifiConnectionLost = false;
      ledSuccessBlink = true;
    }
  } else {
    if (!wifiConnectionLost) {
      Serial.print("[WiFi] Connection Lost! Reason: ");
      Serial.println(WiFi.status());
      wifiConnectionLost = true;
    }
    // AutoReconnect handles this, this here exists as backup
    if (millis() - wifiReconnectLastAttempt >= wifiReconnectInterval * 1000 && !wm.getConfigPortalActive()) {
      connectWiFi();
      wifiReconnectLastAttempt = millis();
    }
  }
}

bool initMQTT() {
  mqtt.setBufferSize(MQTT_MAX_MESSAGE_SIZE);
  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setKeepAlive(30);
  mqtt.setCallback(mqttCallback);
  return connectMQTT();
}

bool connectMQTT() {
  if (!wifiConnectionLost) {
    Serial.print("[MQTT] Connecting to AllThingsTalk... ");
    if (mqtt.connect(klimerkoID, deviceToken, MQTT_PASSWORD)) {
      Serial.println("Connected!");
      if (mqttConnectionLost) {
        mqttConnectionLost = false;
        ledSuccessBlink = true;
      }
      mqttSubscribeTopics();
      publishDiagnosticData();
      return true;
    } else {
      Serial.print("Failed! Reason: ");
      Serial.println(mqtt.state());
      mqttConnectionLost = true;
      return false;
    }
  }
  return false;
}

void maintainMQTT() {
  mqtt.loop();
  if (mqtt.connected()) {
    if (mqttConnectionLost) {
      mqttConnectionLost = false;
      publishDiagnosticData();
    }
  } else {
    if (!mqttConnectionLost) {
      if (wifiConnectionLost) {
        Serial.println("[MQTT] Lost connection due to WiFi!");
      } else {
        Serial.print("[MQTT] Lost Connection. Reason: ");
        Serial.println(mqtt.state());
      }
      mqttConnectionLost = true;
    }
    if (millis() - mqttReconnectLastAttempt >= mqttReconnectInterval * 1000 && !wifiConnectionLost) {
      connectMQTT();
      mqttReconnectLastAttempt = millis();
    }
  }
}

void mqttSubscribeTopics() {
  char command_topic[256];
  snprintf(command_topic, sizeof command_topic, "%s%s%s", "device/", deviceId, "/asset/+/command");
  mqtt.subscribe(command_topic);
}

void mqttCallback(char* p_topic, byte* p_payload, unsigned int p_length) {
  Serial.println("[MQTT] Message Received from AllThingsTalk");
  String topic(p_topic);
  
  // Deserialize JSON
  DynamicJsonDocument doc(256);
  char json[256];
  for (int i = 0; i < p_length; i++) {
      json[i] = (char)p_payload[i];
  }
  auto error = deserializeJson(doc, json);
  if (error) {
      Serial.print("[MQTT] Parsing JSON failed. Code: ");
      Serial.println(error.c_str());
      return;
  }

  String asset = extractAssetNameFromTopic(topic);
//  Serial.print("[MQTT] Asset Name: ");
//  Serial.println(asset);

  if (asset == INTERVAL_ASSET) {
    int value = doc["value"];
    changeInterval(value);
  }
}

String wifiSignal() {
  if (!wifiConnectionLost) {
    int signal = WiFi.RSSI();
    String signalString;
    if (signal < -87) {
        signalString = "Horrible";
    } else if (signal >= -87 && signal <= -80) {
        signalString = "Bad";
    } else if (signal > -80 && signal <= -70) {
        signalString = "Decent";
    } else if (signal > -70 && signal <= -55) {
        signalString = "Good";
    } else if (signal > -55) {
        signalString = "Excellent";
    } else {
        signalString = "Error";
    }
    return signalString;
  }
  return "Error";
}