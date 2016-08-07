// What is the proper indentation?
// What do docstrings look like?
// What is the flake8 of C++?

#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_LSM303_U.h>
#include <Adafruit_L3GD20_U.h>
#include <Adafruit_9DOF.h>

#include <Adafruit_INA219.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>

#include "config.h"

Adafruit_INA219 ina219;

/* Assign a unique ID to the sensors */
Adafruit_9DOF dof = Adafruit_9DOF();
Adafruit_LSM303_Accel_Unified accel = Adafruit_LSM303_Accel_Unified(30301);
Adafruit_LSM303_Mag_Unified mag = Adafruit_LSM303_Mag_Unified(30302);

/* Update this with the correct SLP for accurate altitude measurements */
float seaLevelPressure = SENSORS_PRESSURE_SEALEVELHPA;


//
struct BarfDataType {
    int timestamp;
    
    float roll;
    float pitch;
    float heading;
    
    float shuntvoltage;
    float busvoltage;
    float current_mA;
    float loadvoltage;
};

BarfDataType sensorReadings;


//
void initIMU() {
    if(!accel.begin()) {
        /* There was a problem detecting the LSM303 ... check your connections */
        Serial.println(F("no LSM303 detected"));
        while(1);
    }
    if(!mag.begin()) {
        /* There was a problem detecting the LSM303 ... check your connections */
        Serial.println("no LSM303 detected");
        while(1);
    }
}


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
void readIMU(BarfDataType &variableName) {
    sensors_event_t accel_event;
    sensors_event_t mag_event;
    sensors_vec_t   orientation;
    
    /* Calculate pitch and roll from the raw accelerometer data */
    accel.getEvent(&accel_event);
    if (dof.accelGetOrientation(&accel_event, &orientation))
    {
        /* 'orientation' should have valid .roll and .pitch fields */
        variableName.roll = orientation.roll;
        variableName.pitch = orientation.pitch;
    }
    
    /* Calculate the heading using the magnetometer */
    mag.getEvent(&mag_event);
    if (dof.magGetOrientation(SENSOR_AXIS_Z, &mag_event, &orientation))
    {
        /* 'orientation' should have valid .heading data now */
        variableName.heading = orientation.heading;
    }
}


//
void setup() {
    Serial.begin(115200);
    delay(100);
   
    // current sensor
    uint32_t currentFrequency;
    ina219.begin();
   
    // imu
    initIMU();
    
    // wifi connection
    // TODO: capitalize constants
    Serial.print("connecting to ");
    Serial.print(ssid);
    // TODO: read WiFi library
    WiFi.begin(ssid, password);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.print("wifi connected, ip address: ");
    Serial.println(WiFi.localIP());
   
    // status led
    pinMode(12, OUTPUT);
}


//
void loop() {
    digitalWrite(12, HIGH);
    delay(50);
    digitalWrite(12, LOW);
   
    // TODO: Network/server timestamp? Do we even need a timestamp from the device?
    sensorReadings.timestamp = millis();
    
    readINA219(sensorReadings);
    readIMU(sensorReadings);
    
    // TODO: graceful shutdown if voltage drops too low
   
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
   
    // units of measurement?
    // Serial.print(F()); flash memory?
    Serial.print("Roll:          ");
    Serial.println(sensorReadings.roll);
    Serial.print("Pitch:         ");
    Serial.println(sensorReadings.pitch);
    Serial.print("Heading:       ");
    Serial.println(sensorReadings.heading);
   
    // POST sensor data to API
    HTTPClient http;
    http.begin(HOST);
    // TODO: json
    //http.addHeader("Content-Type", "text/plain");
    //http.POST("{\"value\": 20}");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    http.POST("title=foo&body=bar&userId=1");
    // ?
    http.writeToStream(&Serial);
    /*
    auto httpCode = http.GET();
    Serial.print("HTTP CODE: ");
    Serial.println(httpCode);
    String payload = http.getString();
    Serial.println(payload);
    */
    //REQUIRE(httpCode == HTTP_CODE_OK);
    http.end();
    
    yield();
    // TODO: close wifi connection?
    // TODO: delay and/or interrupt trigger
    //delay(100);
}