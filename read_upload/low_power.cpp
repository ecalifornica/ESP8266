// What is the proper indentation?
// What do docstrings look like?
// What is the flake8 of C++?

#include <Wire.h>

#include <Adafruit_Sensor.h>

#include <Adafruit_INA219.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "config.h"

Adafruit_INA219 ina219;

//
struct BarfDataType {
    int timestamp;
    
    float shuntvoltage;
    float busvoltage;
    float current_mA;
    float loadvoltage;
    
    float soilMoisture;
};

BarfDataType sensorReadings;


//
void readINA219(BarfDataType &variableName) {
    float shuntvoltage = 0;
    float busvoltage = 0;
    float current_mA = 0;
    float loadvoltage = 0;
    
    // Take a look at the ina219 library.
    variableName.shuntvoltage = ina219.getShuntVoltage_mV();
    variableName.busvoltage = ina219.getBusVoltage_V();
    variableName.current_mA = ina219.getCurrent_mA();
    variableName.loadvoltage = busvoltage + (shuntvoltage / 1000);
}


//
void readSoilMoisture(BarfDataType &variableName) {
    float _soilMoisture = analogRead(A0);
    
    // map soil moisture readings to decimal.
    variableName.soilMoisture = map(_soilMoisture, 0, 1023, 0, 100);
}


//
void setup() {
    Serial.begin(115200);
    delay(100);
   
    // current sensor
    uint32_t currentFrequency;
    ina219.begin();
   
    // wifi connection
    Serial.print("connecting to ");
    Serial.print(SSID);
    // TODO: read WiFi library
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("wifi connected, ip address: ");
    Serial.println(WiFi.localIP());
   
    // status led
    pinMode(12, OUTPUT);
    // soil moisture sensor power
    pinMode(14, OUTPUT);
}


//
void loop() {
    digitalWrite(12, HIGH);
    delay(50);
    digitalWrite(12, LOW);
   
    // TODO: Network/server timestamp? Do we even need a timestamp from the device?
    sensorReadings.timestamp = millis();
    
    readINA219(sensorReadings);
    // TODO: graceful shutdown if voltage drops too low
    
    readSoilMoisture(sensorReadings);
   
    // is concatenating resource intensive?
    Serial.print("Bus Voltage:   ");
    Serial.print(sensorReadings.busvoltage);
    Serial.println(" V");
    Serial.print("Shunt Voltage: ");
    Serial.print(sensorReadings.shuntvoltage);
    Serial.println(" mV");
    Serial.print("Load Voltage:  ");
    Serial.print(sensorReadings.loadvoltage);
    Serial.println(" V");
    Serial.print("Current:       ");
    Serial.print(sensorReadings.current_mA);
    Serial.println(" mA");
    Serial.println("");
    Serial.print("Soil moisture: ");
    Serial.println(sensorReadings.soilMoisture);
   
    // POST sensor data to API
    HTTPClient http;
    http.begin(HOST);
    // TODO: json
    //http.addHeader("Content-Type", "text/plain");
    //http.POST("{\"value\": 20}");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    // TODO: properly concatenate with sensor readings
    http.POST("title=foo&body=bar&userId=1");
    // ?
    http.writeToStream(&Serial);
    http.end();
    
    yield();
    ESP.deepSleep(5 * 1000000, WAKE_RF_DEFAULT);
    // TODO: close wifi connection?
    // TODO: delay and/or interrupt trigger
    //delay(100);
}