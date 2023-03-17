/******************************************************/
//       THIS IS A GENERATED FILE - DO NOT EDIT       //
/******************************************************/

#include "Particle.h"
#line 1 "c:/Users/cgame/Documents/IoT/Stanger-Watering-System/Stanger_Watering_System/src/Stanger_Watering_System.ino"
/*
 * Project L11_04_SeeedSensors
 * Description: Smart Watering System that takes inspiration from Stranger Things. Uses neopixels to dynamicly dysplay the plants nees
 * Author: Caden Gamache
 * Date: 03/17/2023 - 
 */

/****************** Libraries ******************/ 
#include "credentials.h"

#include <math.h>

#include "Adafruit_MQTT.h"
#include "Adafruit_MQTT/Adafruit_MQTT_SPARK.h"
#include "Adafruit_MQTT/Adafruit_MQTT.h"

#include "Air_Quality_Sensor.h"

#include <Adafruit_BME280.h>

#include <Adafruit_SSD1306.h>
#include <Adafruit_GFX.h>

#include <Neopixel.h>

/****************** Global State ******************/ 
void setup();
void loop();
#line 27 "c:/Users/cgame/Documents/IoT/Stanger-Watering-System/Stanger_Watering_System/src/Stanger_Watering_System.ino"
TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 

/********************* Feeds *********************/  
Adafruit_MQTT_Subscribe sub_buttonOnOff = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/buttonOnOff");
Adafruit_MQTT_Publish pub_concentration = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/concentration");
Adafruit_MQTT_Publish pub_airQuality = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/airQuality");
Adafruit_MQTT_Subscribe pub_buttonOnOff = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/buttonOnOff");

/******************* Variables *******************/
// Pins
const int DUSTPIN = D8;
const int AIRQUALITYPIN = A0;
const int DISPLAYPIN = D2;
const int NEOPIXPIN = TX;

// Buttons
bool buttonOnOff;

// Timers
unsigned int airTime, dustTime, sampleTime = 30000;

// Dust Sensor
unsigned int duration, lowPulseOccupancy;
float ratio, concentration;

// Air Quality Sensor
int quality, sensorValue, i;

// BME Sensor
bool status;
int tempF, pressureinHG, humidity;

// Neopixels
const int PIXNUM = 46;
const int BRIGHTNESS = 25;

/********************* Objects *********************/ 
AirQualitySensor airQualityPatrol(AIRQUALITYPIN);
Adafruit_BME280 bme;
Adafruit_SSD1306 display(DISPLAYPIN);
Adafruit_NeoPixel neopix(PIXNUM,NEOPIXPIN,WS2812B);

/******************* Functions *******************/
// Checks if wifi is connected and re-connects if false. If true return without doing anything
void MQTT_connect();
// Sends a small ping every two minutes to minimize disconnection chance
bool MQTT_ping();
// Fills a range of pixels with an animation from bottom to top
void pixelFill(int start, int end,int color);
// Fills a range of pixels instantly
void pixelFillInstant(int start, int end,int color);

SYSTEM_MODE(SEMI_AUTOMATIC);

void setup() {
  // Serial Monitor
  Serial.begin(9600);
  waitFor(Serial.isConnected,10000);

  // Wifi
  WiFi.on();
  WiFi.connect();
  while(WiFi.connecting()) {
    Serial.printf(".");
  }

  // Pins
  pinMode(DUSTPIN,INPUT);

  // Get current time to measure sample time
  dustTime = millis();
  airTime = millis();

  // Air quality
  while (!Serial);
  Serial.println("Waiting sensor to init...");
  delay(20000);
  if (airQualityPatrol.init()) {
    Serial.println("Sensor ready.");
  }
  else {
    Serial.println("Sensor ERROR!");
  }

  // BME Sensor
  bme.begin(0x76);
  status = bme.begin(0x76);
  if (status == FALSE) {
    Serial.printf("failed to connect bme device");
  }

  // Display
  display.begin(SSD1306_SWITCHCAPVCC,0x3C);
  display.clearDisplay();
  display.display();
  display.setCursor(0,0);
  display.setTextColor(WHITE);
  display.setTextSize(1);

  // Neopixels
  neopix.begin();
  neopix.setBrightness(BRIGHTNESS);

}

void loop() {
  MQTT_connect();
  MQTT_ping();

  /*********************** Dust Sensor ***********************/
  duration = pulseIn(DUSTPIN, LOW);
  lowPulseOccupancy = lowPulseOccupancy+duration;

  if (millis() - dustTime > sampleTime) {
    ratio = lowPulseOccupancy/(sampleTime*10.0);
    concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62;
    Serial.printf("\nLow Pulse Occupancy: %i\nRatio: %0.2f\nConcentration: %0.2f\n\n",lowPulseOccupancy,ratio,concentration);
    lowPulseOccupancy = 0;
    if(mqtt.Update()) {
      pub_concentration.publish(concentration);
      Serial.printf("Publishing %0.2f \n",concentration); 
    }
    dustTime = millis();
  }

  /******************* Air Quality Sensor *******************/
  i++;
  if((millis()-airTime > 1000) && i < 30) {
    quality = airQualityPatrol.slope();
    sensorValue = airQualityPatrol.getValue();

    Serial.printf("Quality: %i Sensor value: %i\n",quality, sensorValue);
  
    if (quality == AirQualitySensor::FORCE_SIGNAL) {
      Serial.printf("High pollution! Force signal active.\n");
    }
    else if (quality == AirQualitySensor::HIGH_POLLUTION) {
      Serial.printf("High pollution!\n");
    }
    else if (quality == AirQualitySensor::LOW_POLLUTION) {
      Serial.printf("Low pollution!\n");
    }
    else if (quality == AirQualitySensor::FRESH_AIR) {
      Serial.printf("Fresh air.\n");
    } 
    if(mqtt.Update()) {
      pub_airQuality.publish(sensorValue);
      Serial.printf("Publishing %0.2f \n",sensorValue); 
    } 
    airTime = millis();
  }
  if (i == 30) {i = 0;}

  /*********************** BME Sensor ***********************/
  tempF = (bme.readTemperature()*1.8 + 32);
  pressureinHG = bme.readPressure()/3386.3886666667;
  humidity = bme.readHumidity();
}

void MQTT_connect() {
  int8_t ret;
 
  // Return if already connected.
  if (mqtt.connected()) {
    return;
  }
 
  Serial.print("Connecting to MQTT... ");
 
  while ((ret = mqtt.connect()) != 0) { // Connect will return 0 for connected
       Serial.printf("Error Code %s\n",mqtt.connectErrorString(ret));
       Serial.printf("Retrying MQTT connection in 5 seconds...\n");
       mqtt.disconnect();
       delay(5000);  // Wait 5 seconds and try again
  }
  Serial.printf("MQTT Connected!\n");
}

bool MQTT_ping() {
  static unsigned int last;
  bool pingStatus;

  if ((millis()-last)>120000) {
      Serial.printf("Pinging MQTT \n");
      pingStatus = mqtt.ping();
      if(!pingStatus) {
        Serial.printf("Disconnecting \n");
        mqtt.disconnect();
      }
      last = millis();
  }
  return pingStatus;
}

void pixelFill(int start, int end,int color) {
  int neopixNum;
  for ( neopixNum = start; neopixNum < end; neopixNum ++) {
    neopix.setPixelColor(neopixNum, color);
    neopix.show();
    delay(25);
  }
}

void pixelFillInstant(int start, int end,int color) {
  int neopixNum;
  for ( neopixNum = start; neopixNum < end; neopixNum ++) {
    neopix.setPixelColor(neopixNum, color);
    neopix.show();
  }
}