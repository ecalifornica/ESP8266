#include <Wire.h>

#include <Adafruit_Sensor.h>
#include <Adafruit_INA219.h>
#include <Adafruit_NeoPixel.h>

#include <DNSServer.h>

#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
// For serving the network config page
#include <ESP8266WebServer.h>

#include "WiFiManager.h"  // https://github.com/tzapu/WiFiManager

// Neopixel
#ifdef __AVR__
  #include <avr/power.h>
#endif

#include "config.h"

// current sensor
Adafruit_INA219 ina219;

// 74ACHT125 level shift
// https://cdn-shop.adafruit.com/product-files/1787/1787AHC125.pdf
#define PIN 4

#define NUM_LEDS 30

#define BRIGHTNESS 50

Adafruit_NeoPixel strip = Adafruit_NeoPixel(NUM_LEDS, PIN, NEO_GRBW + NEO_KHZ800);

int gamma[] = {
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
    0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  1,  1,  1,  1,
    1,  1,  1,  1,  1,  1,  1,  1,  1,  2,  2,  2,  2,  2,  2,  2,
    2,  3,  3,  3,  3,  3,  3,  3,  4,  4,  4,  4,  4,  5,  5,  5,
    5,  6,  6,  6,  6,  7,  7,  7,  7,  8,  8,  8,  9,  9,  9, 10,
   10, 10, 11, 11, 11, 12, 12, 13, 13, 13, 14, 14, 15, 15, 16, 16,
   17, 17, 18, 18, 19, 19, 20, 20, 21, 21, 22, 22, 23, 24, 24, 25,
   25, 26, 27, 27, 28, 29, 29, 30, 31, 32, 32, 33, 34, 35, 35, 36,
   37, 38, 39, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 50,
   51, 52, 54, 55, 56, 57, 58, 59, 60, 61, 62, 63, 64, 66, 67, 68,
   69, 70, 72, 73, 74, 75, 77, 78, 79, 81, 82, 83, 85, 86, 87, 89,
   90, 92, 93, 95, 96, 98, 99,101,102,104,105,107,109,110,112,114,
  115,117,119,120,122,124,126,127,129,131,133,135,137,138,140,142,
  144,146,148,150,152,154,156,158,160,162,164,167,169,171,173,175,
  177,180,182,184,186,189,191,193,196,198,200,203,205,208,210,213,
  215,218,220,223,225,228,231,233,236,239,241,244,247,249,252,255 };





//
struct BarfDataType {
    char serialNumber[32];

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
    sensorReadings.serialNumber = myWiFiManager->getConfigPortalSSID();
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

    // Neopixel
    strip.setBrightness(BRIGHTNESS);
    strip.begin();
    strip.show(); // Initialize all pixels to 'off'
}


//
void loop() {
    lightshow();
}
void sensor_loop() {
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
    String sensor_id_string = "sensor_id=";
    upload += sensor_id_string + sensorReadings.serialNumber;
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


// Adafruit Neopixel example
void lightshow() {
  // Some example procedures showing how to display to the pixels:
  colorWipe(strip.Color(255, 0, 0), 50); // Red
  colorWipe(strip.Color(0, 255, 0), 50); // Green
  colorWipe(strip.Color(0, 0, 255), 50); // Blue
  colorWipe(strip.Color(0, 0, 0, 255), 50); // White

  whiteOverRainbow(20,75,5);  

  pulseWhite(10); 

  // fullWhite();
  // delay(2000);

  rainbowFade2White(3,3,1);

  rainbowCycle(601000);
}

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait) {
  for(uint16_t i=0; i<strip.numPixels(); i++) {
    strip.setPixelColor(i, c);
    strip.show();
    delay(wait);
  }
}

void pulseWhite(uint8_t wait) {
  for(int j = 0; j < 256 ; j++){
      for(uint16_t i=0; i<strip.numPixels(); i++) {
          strip.setPixelColor(i, strip.Color(0,0,0, gamma[j] ) );
        }
        delay(wait);
        strip.show();
      }

  for(int j = 255; j >= 0 ; j--){
      for(uint16_t i=0; i<strip.numPixels(); i++) {
          strip.setPixelColor(i, strip.Color(0,0,0, gamma[j] ) );
        }
        delay(wait);
        strip.show();
      }
}


void rainbowFade2White(uint8_t wait, int rainbowLoops, int whiteLoops) {
  float fadeMax = 100.0;
  int fadeVal = 0;
  uint32_t wheelVal;
  int redVal, greenVal, blueVal;

  for(int k = 0 ; k < rainbowLoops ; k ++){
    
    for(int j=0; j<256; j++) { // 5 cycles of all colors on wheel

      for(int i=0; i< strip.numPixels(); i++) {

        wheelVal = Wheel(((i * 256 / strip.numPixels()) + j) & 255);

        redVal = red(wheelVal) * float(fadeVal/fadeMax);
        greenVal = green(wheelVal) * float(fadeVal/fadeMax);
        blueVal = blue(wheelVal) * float(fadeVal/fadeMax);

        strip.setPixelColor( i, strip.Color( redVal, greenVal, blueVal ) );

      }

      //First loop, fade in
      if(k == 0 && fadeVal < fadeMax-1) {
          fadeVal++;
      }

      //Last loop, fade out
      else if(k == rainbowLoops - 1 && j > 255 - fadeMax ){
          fadeVal--;
      }
        strip.show();
        delay(wait);
    }
  }
  delay(500);

  for(int k = 0 ; k < whiteLoops ; k ++){
    for(int j = 0; j < 256 ; j++){
        for(uint16_t i=0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, strip.Color(0,0,0, gamma[j] ) );
          }
          strip.show();
        }
        delay(2000);
    for(int j = 255; j >= 0 ; j--){
        for(uint16_t i=0; i < strip.numPixels(); i++) {
            strip.setPixelColor(i, strip.Color(0,0,0, gamma[j] ) );
          }
          strip.show();
        }
  }
  delay(500);
}

void whiteOverRainbow(uint8_t wait, uint8_t whiteSpeed, uint8_t whiteLength ) {
  
  if(whiteLength >= strip.numPixels()) whiteLength = strip.numPixels() - 1;

  int head = whiteLength - 1;
  int tail = 0;

  int loops = 3;
  int loopNum = 0;

  static unsigned long lastTime = 0;


  while(true){
    for(int j=0; j<256; j++) {
      for(uint16_t i=0; i<strip.numPixels(); i++) {
        if((i >= tail && i <= head) || (tail > head && i >= tail) || (tail > head && i <= head) ){
          strip.setPixelColor(i, strip.Color(0,0,0, 255 ) );
        }
        else{
          strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
        }
        
      }

      if(millis() - lastTime > whiteSpeed) {
        head++;
        tail++;
        if(head == strip.numPixels()){
          loopNum++;
        }
        lastTime = millis();
      }

      if(loopNum == loops) return;
    
      head%=strip.numPixels();
      tail%=strip.numPixels();
        strip.show();
        delay(wait);
    }
  }
  
}
void fullWhite() {
  
    for(uint16_t i=0; i<strip.numPixels(); i++) {
        strip.setPixelColor(i, strip.Color(0,0,0, 255 ) );
    }
      strip.show();
}


void rainbowCycle(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256 * 5; j++) { // 5 cycles of all colors on wheel
    for(i=0; i< strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel(((i * 256 / strip.numPixels()) + j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

void rainbow(uint8_t wait) {
  uint16_t i, j;

  for(j=0; j<256; j++) {
    for(i=0; i<strip.numPixels(); i++) {
      strip.setPixelColor(i, Wheel((i+j) & 255));
    }
    strip.show();
    delay(wait);
  }
}

// Input a value 0 to 255 to get a color value.
// The colours are a transition r - g - b - back to r.
uint32_t Wheel(byte WheelPos) {
  WheelPos = 255 - WheelPos;
  if(WheelPos < 85) {
    return strip.Color(255 - WheelPos * 3, 0, WheelPos * 3,0);
  }
  if(WheelPos < 170) {
    WheelPos -= 85;
    return strip.Color(0, WheelPos * 3, 255 - WheelPos * 3,0);
  }
  WheelPos -= 170;
  return strip.Color(WheelPos * 3, 255 - WheelPos * 3, 0,0);
}

uint8_t red(uint32_t c) {
  return (c >> 8);
}
uint8_t green(uint32_t c) {
  return (c >> 16);
}
uint8_t blue(uint32_t c) {
  return (c);
}


