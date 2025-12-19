#include "Sensors.h"

HardwareSerial pmsSerial(1); 
PMS pms(pmsSerial);
PMS::DATA pmsData;

Adafruit_BME280 bme;

movingAvg pm1(sensorAverageSamples);
movingAvg pm25(sensorAverageSamples);
movingAvg pm10(sensorAverageSamples);
movingAvg temp(sensorAverageSamples);
movingAvg hum(sensorAverageSamples);
movingAvg pres(sensorAverageSamples);

float avgTemperature = 0, avgHumidity = 0, avgPressure = 0;
int avgPM1 = 0, avgPM25 = 0, avgPM10 = 0;

void initSensors() {

  pmsSerial.begin(9600, SERIAL_8N1, PMS_RX, PMS_TX);
  
  if (!bme.begin(OLED_ADDR)) {
    Serial.println("Could not find a valid BME280 sensor, check wiring!");
  }

  // Inicijalizacija svih movingAvg objekata
  pm1.begin();
  pm25.begin();
  pm10.begin();
  temp.begin();
  hum.begin();
  pres.begin();
  
  Serial.println("Senzori inicijalizovani.");
}

void pmsPower(bool state) {
  if (state) pms.wakeUp();
  else pms.sleep();
}

void readPMS() {
  if (pms.readUntil(pmsData)) {
    avgPM1 = pm1.reading(pmsData.PM_AE_UG_1_0);
    avgPM25 = pm25.reading(pmsData.PM_AE_UG_2_5);
    avgPM10 = pm10.reading(pmsData.PM_AE_UG_10_0);
    
    Serial.print("PMS očitavanje: PM2.5 = ");
    Serial.println(avgPM25);
  }
}

void readBME() {
  float t = bme.readTemperature() + temperatureOffset; 
  float h = bme.readHumidity();
  float p = bme.readPressure() / 100.0F;

  avgTemperature = temp.reading(t * 100) / 100.0;
  avgHumidity = hum.reading(h * 100) / 100.0;
  avgPressure = pres.reading(p * 100) / 100.0;
  
  Serial.print("BME očitavanje: Temp = ");
  Serial.println(avgTemperature);
}

long readIntervalMillis() {
  return (long)dataPublishInterval * 60 * 1000;
}

void sensorLoop() { // Reads and publishes sensor data and wakes up pms sensor in predefined intervals
  // Check if it's time to wake up PMS7003
  if (millis() - sensorReadTime >= readIntervalMillis() - (pmsWakeBefore * 1000) && !pmsWoken && pmsSensorOnline) {
    Serial.println("[PMS] Now waking up Air Quality Sensor");
    pmsPower(true);
  }
  
  // Read sensor data
  if (millis() - sensorReadTime >= readIntervalMillis()) {
    sensorReadTime = millis();
    readSensorData();
  }

  // Send average sensor data
  if (millis() - dataPublishTime >= dataPublishInterval * 60000) {
    if (!wifiConnectionLost) {
      if (!mqttConnectionLost) {
        dataPublishFailed = false;
        dataPublishTime = millis();
        publishSensorData();
      } else {
        if (!dataPublishFailed) {
          Serial.println("[DATA] Can't send sensor data because Klimerko is not connected to AllThingsTalk");
          dataPublishFailed = true;
        }
      }
    } else {
      if (!dataPublishFailed) {
        Serial.println("[DATA] Can't send sensor data because Klimerko is not connected to WiFi");
        dataPublishFailed = true;
      }
    }
  }
}

void readSensorData() {
  Serial.println("------------------------------DATA------------------------------");
  readPMS();
  readBME();
  Serial.println("----------------------------------------------------------------");
  if (!pmsNoSleep && pmsSensorOnline) {
    Serial.print("[PMS] Air Quality Sensor will sleep until ");
    Serial.print(pmsWakeBefore);
    Serial.println(" seconds before next reading.");
    pmsPower(false);
  }
}

void publishSensorData() {
  char JSONmessageBuffer[512];
  DynamicJsonDocument doc(512);
  if (pmsSensorOnline) {
    JsonObject airQualityJson = doc.createNestedObject(AQ_ASSET);
    airQualityJson["value"] = airQuality;
    JsonObject pm1Json = doc.createNestedObject(PM1_ASSET);
    pm1Json["value"] = avgPM1;
    JsonObject pm25Json = doc.createNestedObject(PM2_5_ASSET);
    pm25Json["value"] = avgPM25;
    JsonObject pm10Json = doc.createNestedObject(PM10_ASSET);
    pm10Json["value"] = avgPM10;
  } else {
    Serial.println("[DATA] Won't send Air Quality Sensor (PMS7003) data because it seems to be offline.");
  }
  if (bmeSensorOnline) {
    JsonObject temperatureJson = doc.createNestedObject(TEMPERATURE_ASSET);
    temperatureJson["value"] = avgTemperature;
    JsonObject humidityJson = doc.createNestedObject(HUMIDITY_ASSET);
    humidityJson["value"] = avgHumidity;
    JsonObject pressureJson = doc.createNestedObject(PRESSURE_ASSET);
    pressureJson["value"] = avgPressure;
  } else {
    Serial.println("[DATA] Won't send Temperature/Humidity/Pressure Sensor (BME280) data because it seems to be offline.");
  }
  JsonObject firmwareJson = doc.createNestedObject(FIRMWARE_ASSET);
  firmwareJson["value"] = firmwareVersion;
  JsonObject wifiJson = doc.createNestedObject(WIFI_SIGNAL_ASSET);
  wifiJson["value"] = wifiSignal();
  serializeJson(doc, JSONmessageBuffer);

  char topic[128];
  snprintf(topic, sizeof topic, "%s%s%s", "device/", deviceId, "/state");
  mqtt.publish(topic, JSONmessageBuffer, false);
  Serial.print("[DATA] Published sensor data to AllThingsTalk: ");
  Serial.println(JSONmessageBuffer);
}

void readPMS() { // Function that reads data from the PMS7003
  while (pmsSerial.available()) { pmsSerial.read(); }
  pms.requestRead(); // Now get the real data
  
  if (pms.readUntil(data)) {
    int PM1 = data.PM_AE_UG_1_0;
    int PM2_5 = data.PM_AE_UG_2_5;
    int PM10 = data.PM_AE_UG_10_0;

    avgPM1 = pm1.reading(PM1);
    avgPM25 = pm25.reading(PM2_5);
    avgPM10 = pm10.reading(PM10);

    // Assign a text value of how good the air is based on current value
    // http://www.amskv.sepa.gov.rs/kriterijumi.php
    if (PM10 <= 20) {
      airQualityRaw = "Excellent";
    } else if (PM10 >= 21 && PM10 <= 40) {
      airQualityRaw = "Good";
    } else if (PM10 >= 41 && PM10 <= 50) {
      airQualityRaw = "Acceptable";
    } else if (PM10 >= 51 && PM10 <= 100) {
      airQualityRaw = "Polluted";
    } else if (PM10 > 100) {
      airQualityRaw = "Very Polluted";
    }

    // Assign a text value of how good the air is based on average value
    // http://www.amskv.sepa.gov.rs/kriterijumi.php
    if (avgPM10 <= 20) {
      airQuality = "Excellent";
    } else if (avgPM10 >= 21 && avgPM10 <= 40) {
      airQuality = "Good";
    } else if (avgPM10 >= 41 && avgPM10 <= 50) {
      airQuality = "Acceptable";
    } else if (avgPM10 >= 51 && avgPM10 <= 100) {
      airQuality = "Polluted";
    } else if (avgPM10 > 100) {
      airQuality = "Very Polluted";
    }

    // Print via SERIAL
    Serial.print("Air Quality is ");
    Serial.print(airQualityRaw);
    Serial.print(" (Average: ");
    Serial.print(airQuality);
    Serial.println(")");
    Serial.print("PM 1:          ");
    Serial.print(PM1);
    Serial.print(" µg/m³ (Average: ");
    Serial.print(avgPM1);
    Serial.println(")");
    Serial.print("PM 2.5:        ");
    Serial.print(PM2_5);
    Serial.print(" µg/m³ (Average: ");
    Serial.print(avgPM25);
    Serial.println(")");
    Serial.print("PM 10:         ");
    Serial.print(PM10);
    Serial.print(" µg/m³ (Average: ");
    Serial.print(avgPM10);
    Serial.println(")");

    pmsSensorRetry = 0;
    if (!pmsSensorOnline) {
      pmsSensorOnline = true;
      Serial.println("[PMS] Air Quality Sensor (PMS7003) seems to be back online!");
    }
  } else {
    if (pmsSensorOnline) {
      Serial.println("[PMS] Air Quality Sensor (PMS7003) returned no data on data request this time.");
      pmsSensorRetry++;
      if (pmsSensorRetry > sensorRetriesUntilConsideredOffline) {
        pmsSensorOnline = false;
        Serial.println("[PMS] Air Quality Sensor (PMS7003) seems to be offline!");
        pm1.reset();
        pm25.reset();
        pm10.reset();
        initPMS();
      }
    } else {
      Serial.println("[PMS] Air Quality Sensor (PMS7003) is offline.");
      initPMS();
    }
  }
}

void readBME() { // Function for reading data from the BME280 Sensor
  float temperatureRaw = bme.readTemperature();
  float temperature    = temperatureRaw + bmeTemperatureOffset;
  float humidityRaw    = bme.readHumidity();
  float humidity       = humidityRaw * exp(243.12 * 17.62 * (temperatureRaw - temperature) / (243.12 + temperatureRaw) / (243.12 + temperature)); // Compensates the RH in accordance to temperature offset so the RH isn't wrong when the temp is offset
  float pressure       = bme.readPressure() / 100.0F;

  if (temperatureRaw > -100 && temperatureRaw < 150 && humidity >= 0 && humidity <= 100) {
    avgTemperature = temp.reading(temperature*100);
    avgTemperature = avgTemperature/100;
    avgHumidity    = hum.reading(humidity*100);
    avgHumidity    = avgHumidity/100;
    avgPressure    = pres.reading(pressure*100);
    avgPressure    = avgPressure/100;

    Serial.print("Temperature:   ");
    Serial.print(temperature);
    Serial.print("°C (Average: ");
    Serial.print(avgTemperature);
    Serial.print(", Raw: ");
    Serial.print(temperatureRaw);
    Serial.print(", Offset: ");
    Serial.print(bmeTemperatureOffset);
    Serial.println(")");
    Serial.print("Humidity:      ");
    Serial.print(humidity);
    Serial.print(" % (Average: ");
    Serial.print(avgHumidity);
    Serial.print(", Raw: ");
    Serial.print(humidityRaw);
    Serial.println(")");
    Serial.print("Pressure:      ");
    Serial.print(pressure);
    Serial.print(" mbar (Average: ");
    Serial.print(avgPressure);
    Serial.println(")");

    bmeSensorRetry = 0;
    if (!bmeSensorOnline) {
      bmeSensorOnline = true;
      Serial.println("[BME] Temperature/Humidity/Pressure Sensor (BME280) is back online!");
    }
  } else {
    if (bmeSensorOnline) {
      Serial.println("[BME] Temperature/Humidity/Pressure Sensor (BME280) returned no data this time.");
      bmeSensorRetry++;
      if (bmeSensorRetry > sensorRetriesUntilConsideredOffline) {
        bmeSensorOnline = false;
        Serial.println("[BME] Temperature/Humidity/Pressure Sensor (BME280) seems to be offline!");
        temp.reset();
        hum.reset();
        pres.reset();
        initBME();
      }
    } else {
      Serial.println("[BME] Temperature/Humidity/Pressure Sensor (BME280) is offline.");
      initBME();
    }
  }
}

void pmsPower(bool state) { // Controls sleep state of PMS sensor
  if (state) {
    pms.wakeUp();
  //  pms.passiveMode(); // ovo ne pripada ovde
    pmsWoken = true;
  } else {
    pmsSerial.flush();
    unsigned long now = millis();
    while(millis() < now + 100);
    pmsWoken = false;
    pms.sleep();
  }
}