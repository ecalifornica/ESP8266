#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_INA219.h>

#include <DNSServer.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
// For serving the network config page
#include <ESP8266WebServer.h>

#include "WiFiManager.h"  // https://github.com/tzapu/WiFiManager

#include "config.h"

Adafruit_INA219 ina219;

//
struct BarfDataType {
    int timestamp;
    
    float shuntvoltage;
    float busvoltage;
    float current_mA;
    float loadvoltage;
    
    int rawSoilMoisture = 0;
    int soilMoisture = 0;
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
    digitalWrite(SOIL_SENSOR_POWER, HIGH);
    delay(100);
    variableName.rawSoilMoisture = analogRead(A0);
    digitalWrite(SOIL_SENSOR_POWER, LOW);
    // map soil moisture readings to decimal.
    variableName.soilMoisture = map(variableName.rawSoilMoisture, 0, 1023, 0, 100);
}


// Network config
void configModeCallback (WiFiManager *myWiFiManager) {
    Serial.println("Entered config mode");
    Serial.println(WiFi.softAPIP());
    //if you used auto generated SSID, print it
    Serial.println(myWiFiManager->getConfigPortalSSID());
}


//
void setup() {
    Serial.begin(115200);
    
    // wifi connection
    // WiFiManager
    // Local intialization. Once its business is done, there is no need to keep it around
    // TODO: read library
    WiFiManager wifiManager;
    // reset saved network settings - for testing
    //wifiManager.resetSettings();
    
    //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
    wifiManager.setAPCallback(configModeCallback);
    
    //fetches ssid and pass and tries to connect
    //if it does not connect it starts an access point with the specified name
    //here  "AutoConnectAP"
    //and goes into a blocking loop awaiting configuration
    if(!wifiManager.autoConnect()) {
        Serial.println("failed to connect and hit timeout");
        //reset and try again, or maybe put it to deep sleep
        ESP.reset();
        delay(1000);
    }
    Serial.print("wifi connected, ip address: ");
    // TODO: wifiManager.localIP()
    //Serial.println(WiFi.localIP());
    
    // current sensor
    uint32_t currentFrequency;
    ina219.begin();
    
    // status led
    pinMode(LED_PIN, OUTPUT);
    // soil moisture sensor power
    pinMode(SOIL_SENSOR_POWER, OUTPUT);
}


//
void loop() {
    digitalWrite(LED_PIN, HIGH);
    delay(50);
    digitalWrite(LED_PIN, LOW);
    
    // TODO: Network/server timestamp? Do we even need a timestamp from the device?
    sensorReadings.timestamp = millis();
    
    readINA219(sensorReadings);
    // TODO: graceful shutdown if voltage drops too low
    
    readSoilMoisture(sensorReadings);
    
    // is concatenating resource intensive?
    /*
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
     */
    
    // this is ugly
    String soil_moisture = "soil_moisture=";
    String upload = soil_moisture + sensorReadings.soilMoisture;
    String bus_voltage = "&bus_voltage=";
    String shunt_string = "&shunt_voltage=";
    String load_string = "&load_voltage=";
    String current_string = "&current_string=";
    String raw_soil_string = "&raw_soil_moisture=";
    upload += bus_voltage + sensorReadings.busvoltage;
    upload += shunt_string + sensorReadings.shuntvoltage;
    upload += load_string + sensorReadings.loadvoltage;
    upload += current_string + sensorReadings.current_mA;
    upload += raw_soil_string + sensorReadings.rawSoilMoisture;
    Serial.println(upload);
    
    // GET
    HTTPClient http;
    http.begin(API_IP, API_PORT, API_URL);
    auto httpCode = http.GET();
    Serial.println(httpCode);
    String payload = http.getString();
    Serial.println(payload);
    
    
    // POST sensor data to API
    Serial.println("HTTPClient");
    //HTTPClient http;
    Serial.println("http.begin");
    http.begin(API_IP, API_PORT, API_URL);
    // TODO: json
    Serial.println("addHeader");
    http.addHeader("Content-Type", "application/x-www-form-urlencoded");
    Serial.println("http.POST");
    httpCode = http.POST(upload);
    Serial.print("POST http code: ");
    Serial.println(httpCode);
    // ?
    Serial.println("writeToStream");
    http.writeToStream(&Serial);
    Serial.println("http.end");
    http.end();
    
    //http.POST("{\"value\": 20}");
    // TODO: properly concatenate with sensor readings
    //http.POST("title=foo&body=bar&userId=1");
    
    // TODO: close wifi connection?
    // TODO: delay and/or interrupt trigger
    //delay(1000);
    Serial.println("yield");
    yield();
    // sleep for five minutes
    Serial.println("sleep");
    ESP.deepSleep(300 * 1000000, WAKE_RF_DEFAULT);
}
