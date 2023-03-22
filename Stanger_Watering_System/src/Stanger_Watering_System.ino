/*
 * Project L11_04_SeeedSensors
 * Description: Smart Watering System that takes inspiration from Stranger Things. Uses neopixels to dynamicly dysplay the plants nees
 * Author: Caden Gamache
 * Date: 03/17/2023 - 
 * 
 ********* NOTES *********
 * Neopixel1 = Bedroom
 * Neopixel2 = Kitchen
 * Neopixel3 = Hall Right
 * Neopixel4 = Hall Middle
 * Neopixel5 = Hall Left
 * Neopixel6 = Bathroom
 * Neopixel7 = Meditation
 * Neopixel8 = Living room
 * Neopixel9 = Fireplace
`*/

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
TCPClient TheClient;
Adafruit_MQTT_SPARK mqtt(&TheClient,AIO_SERVER,AIO_SERVERPORT,AIO_USERNAME,AIO_KEY); 

/********************* Feeds *********************/  
Adafruit_MQTT_Subscribe sub_buttonOnOff = Adafruit_MQTT_Subscribe(&mqtt, AIO_USERNAME "/feeds/buttonOnOff"); 
Adafruit_MQTT_Publish pub_concentration = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/concentration");
Adafruit_MQTT_Publish pub_airQuality = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/airQuality");
Adafruit_MQTT_Publish pub_tempF = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/tempF");
Adafruit_MQTT_Publish pub_humidity = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/humidity");
Adafruit_MQTT_Publish pub_pressure = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/pressure");
Adafruit_MQTT_Publish pub_moisture = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/moisture");
Adafruit_MQTT_Publish pub_buttonOnOff = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/buttonOnOff");
Adafruit_MQTT_Publish pub_currentRoom = Adafruit_MQTT_Publish(&mqtt, AIO_USERNAME "/feeds/currentRoom");

/******************* Variables *******************/
// Pins
const int DUSTPIN = D8;
const int AIRQUALITYPIN = A0;
const int MOISTUREPIN = A5;
const int BUTTONPIN = D2;
const int PUMPPPIN = D3;
const int NEOPIXPIN = TX;

//misc
int pubNum;

// Generic loop variables
int i, j, k;

// Bools
bool buttonOnOff, subButtonOnOff, fireUpDown;

// Timers
unsigned int pubTime, airTime, dustTime, fireTime, flickerTime, plantTime, moistureTime;
unsigned int sampleTime = 30000;

// Dust Sensor
unsigned int duration, lowPulseOccupancy;
float ratio, concentration;

// Air Quality Sensor
int quality, airValue;

// BME Sensor
bool status;
int tempF, pressureinHG, humidity;

// Moisture Sensor
int moisture;

// Neopixels
  // Setup
  const int PIXNUM = 46;
  int brightness = 25;
  // Fireplace
  int randomNum1, randomNum2, randomBright;
  int fireMax;
  // Room Management
  int currentPlantPos, lastFavRoom, randomRoom, lastRoom;

/********************* Objects *********************/ 
AirQualitySensor airQualityPatrol(AIRQUALITYPIN);
Adafruit_BME280 bme;
Adafruit_SSD1306 display(D4);
Adafruit_NeoPixel neopix(PIXNUM,NEOPIXPIN,WS2812B);

/******************** Functions ********************/
// Checks if wifi is connected and re-connects if false. If true return without doing anything
void MQTT_connect();
// Sends a small ping every two minutes to minimize disconnection chance
bool MQTT_ping();
// Fills a range of pixels instantly
void pixelFillInstant(int start, int end,int color);
// Creepy flickering light effect that goes off when a condition is met
void creepyFlicker();
// Checks the plants current room and turns on a neopixel
void lightPlantPos(int plantPos);

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

  // Setup MQTT subscription
  mqtt.subscribe(&sub_buttonOnOff);

  // Pins
  pinMode(DUSTPIN,INPUT);
  pinMode(MOISTUREPIN,INPUT);
  pinMode(BUTTONPIN,INPUT);
  pinMode(PUMPPPIN,OUTPUT);

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
  neopix.setBrightness(brightness);
  lastFavRoom = 100;
  fireMax = random(10,20);

  // Timers
  fireTime = millis();

}

void loop() {
  MQTT_connect();
  MQTT_ping();
  display.clearDisplay();
  buttonOnOff = digitalRead(D2);

  // Resets the plants position to the meditation room with an 90% chance, otherwise set to kitchen.
  randomRoom = random(1,100);
  if (randomRoom > 3) {
    currentPlantPos = 4;
  }
  else {
    currentPlantPos = 1;
  }

  /*********************** Dust Sensor ***********************/
  duration = pulseIn(DUSTPIN, LOW);
  lowPulseOccupancy = lowPulseOccupancy+duration;

  if (millis() - dustTime > sampleTime) {
    ratio = lowPulseOccupancy/(sampleTime*10.0);
    concentration = 1.1*pow(ratio,3)-3.8*pow(ratio,2)+520*ratio+0.62;
    Serial.printf("\nLow Pulse Occupancy: %i\nRatio: %0.2f\nConcentration: %0.2f\n\n",lowPulseOccupancy,ratio,concentration);
    lowPulseOccupancy = 0;

    dustTime = millis();
  }

  // Moves plant position to bedroom if there is to much dust
  if (concentration > 6000) {
    currentPlantPos = 0;
  }

  /******************* Air Quality Sensor *******************/
  if((millis()-airTime > 1000)) {
    quality = airQualityPatrol.slope();
    airValue = airQualityPatrol.getValue();

    Serial.printf("\nQuality: %i Sensor value: %i\n",quality, airValue);
  
    if (quality == AirQualitySensor::FORCE_SIGNAL) {
      Serial.printf("\nHigh pollution! Force signal active.\n");
    }
    else if (quality == AirQualitySensor::HIGH_POLLUTION) {
      Serial.printf("\nHigh pollution!\n");
    }
    else if (quality == AirQualitySensor::LOW_POLLUTION) {
      Serial.printf("\nLow pollution!\n");
    }
    else if (quality == AirQualitySensor::FRESH_AIR) {
      Serial.printf("\nFresh air.\n");
    } 

    airTime = millis();
  }
  // Moves plant position to bedroom if the air quality gets bad
  if (quality == 1 || quality == 2) {
    currentPlantPos = 0;
  }

  /*********************** BME Sensor ***********************/
  tempF = (bme.readTemperature()*1.8 + 32);
  pressureinHG = bme.readPressure()/3386.3886666667;
  humidity = bme.readHumidity();

  Serial.printf("\nTempature Farenheight = %i Pressure inHG = %i Humidity = %i",tempF,pressureinHG,humidity);

  // Moves plant position to living room if it gets to cold or humid
  if (tempF < 70 || humidity > 50) {
    currentPlantPos = 5;
  }

  /******************** Moisture Sensor ********************/
  moisture = analogRead(MOISTUREPIN);
  Serial.printf("\nMoisture value = %i",moisture);
  // Moves plant position to bathroom if it gets to dry
  if (millis()-moistureTime > 10000) {
    if (moisture > 2800 || buttonOnOff || subButtonOnOff) {
      currentPlantPos = 3;
      digitalWrite(D3,HIGH);
      delay(500);
      digitalWrite(D3,LOW);
      moistureTime = millis();
      subButtonOnOff = 0;
      buttonOnOff = 0;
    }
  }

  /*********************** Subscriptions ***********************/
  Adafruit_MQTT_Subscribe *subscription;
  while ((subscription = mqtt.readSubscription(100))) {
    if (subscription == &sub_buttonOnOff) {
      subButtonOnOff = atof((char *)sub_buttonOnOff.lastread);
    }
  }

  /*********************** Publishes ***********************/
  if((millis()-pubTime > 1500)) {
    pubNum++;
    if(mqtt.Update()) {
      if (buttonOnOff) {pub_buttonOnOff.publish(buttonOnOff); Serial.printf("\nPublishing %i \n",buttonOnOff);}
      if (pubNum == 1) {pub_concentration.publish(concentration); Serial.printf("\nPublishing %0.2f \n",concentration);}
      if (pubNum == 2) {pub_airQuality.publish(airValue); Serial.printf("\nPublishing %i \n",airValue); }
      if (pubNum == 3) {pub_tempF.publish(tempF); Serial.printf("\nPublishing %i \n",tempF); }
      if (pubNum == 4) {pub_pressure.publish(pressureinHG); Serial.printf("\nPublishing %i \n",pressureinHG);}
      if (pubNum == 5) {pub_humidity.publish(humidity); Serial.printf("\nPublishing %i \n",humidity); } 
      if (pubNum == 6) {pub_moisture.publish(moisture); Serial.printf("\nPublishing %i \n",moisture);}
      if (pubNum == 7) {pub_currentRoom.publish(currentPlantPos); Serial.printf("\nPublishing %i \n",currentPlantPos);}
    }
    if (pubNum == 7) {
      pubNum = 0;
    }
    pubTime = millis();
  }

  /************************ Display ************************/
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(5,15);
  display.printf("Tempature F = %i\n\n Humidity = %i\n\n CurrentRoom = %i",tempF,humidity,currentPlantPos);
  display.display();

  /*********************** Neopixels ***********************/
  //Ambient Fire lighting
  if (k == fireMax) {
    randomNum1 = random(153,155);
    randomNum2 = random(78,80);
    randomBright = random(15,25);
    k = 0;
    fireMax = random(200,300);
    fireTime = millis();
  }
  if (k == fireMax/2) {fireUpDown = !fireUpDown; fireTime = millis();}
  neopix.setBrightness(randomBright);
  if (fireUpDown) {
    neopix.setColor(8,(randomNum1-(millis()-fireTime)/30000),(randomNum2-(millis()-fireTime)/30000),0);
    neopix.show();
    k++;
    Serial.printf("up");
  }
  else {
    neopix.setColor(8,(randomNum1+(millis()-fireTime)/30000),(randomNum2+(millis()-fireTime)/30000),0);
    neopix.show();
    k++;
    Serial.printf("Down");
  }

  // Every 3 seconds checks if the plant position has changed and updates neopixels only if it's different
  Serial.printf("\nplant pos = %i\n",currentPlantPos);
  if (millis()-plantTime > 3000 && currentPlantPos != lastRoom) {
    if (currentPlantPos != lastRoom && currentPlantPos != 4 && currentPlantPos != 1) {
      creepyFlicker();
    }
    lightPlantPos(currentPlantPos);
    lastFavRoom = randomRoom;
    plantTime = millis();
    lastRoom = currentPlantPos;
  }

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

void pixelFillInstant(int start, int end, int color) {
  int neopixNum;
  for ( neopixNum = start; neopixNum <= end; neopixNum ++) {
    neopix.setPixelColor(neopixNum, color);
    neopix.show();
  }
}

void creepyFlicker() {
  int neopixelOnOff;
  int flick, flick2, flick3, flick4, flick5;

  for (i = 0;i<random(10,20);i++) {
    // Stores an array with random on or off states and lights up neopixels accordingly
    flick = random(0,8);
    flick2 = random(0,8);
    flick3 = random(0,8);
    flick4 = random(0,8);
    flick5 = random(0,8);
    Serial.printf("\nflicks: %i %i %i %i %i",flick, flick2, flick3, flick4, flick5);
    neopixelOnOff = random(0,100);
    Serial.printf("\nonOff %i",neopixelOnOff);
    if (neopixelOnOff > 30) {
      neopix.setPixelColor(flick, 0xFEAA00);
      neopix.setPixelColor(flick2, 0xFEAA00);
      neopix.setPixelColor(flick3, 0xFEAA00);
      neopix.setPixelColor(flick4, 0xFEAA00);
      neopix.setPixelColor(flick5, 0xFEAA00);
    }
    else {
      neopix.setPixelColor(flick, 0x000000);
      neopix.setPixelColor(flick2, 0x000000);
      neopix.setPixelColor(flick3, 0x000000);
      neopix.setPixelColor(flick4, 0x000000);
      neopix.setPixelColor(flick5, 0x000000);
    }
    neopix.show();
    //Randomly decides to turn off the current pixel or leave it on
    neopixelOnOff = random(0,1);
    if (neopixelOnOff == 0) {
      delay(random(10,125));
      neopix.setPixelColor(flick, 0x000000);
      neopix.setPixelColor(flick2, 0x000000);
      neopix.setPixelColor(flick3, 0x000000);
      neopix.setPixelColor(flick4, 0x000000);
      neopix.setPixelColor(flick5, 0x000000);
    }
    neopix.show();
    delay(random(0,70));
  }
  pixelFillInstant(0,7,0x000000);
}

void lightPlantPos(int plantPos) {
  static int CURRENTPLANTPOS [6] {0,1,2,5,6,7};
  
  // Resets lights to off
  pixelFillInstant(0,7,0x000000);
  // Meditation didn't change
  if (CURRENTPLANTPOS[plantPos] == 6 && lastFavRoom > 10) {
    // Lights up updated current room
    neopix.setPixelColor(6, 0xFFFFFF);
    neopix.show();
    Serial.printf("\nPlant is still deep in meditation\n");
    return;
  }
  // Kitchen didn't change
  if (CURRENTPLANTPOS[plantPos] == 1 && lastFavRoom < 11) {
    // Lights up updated current room
    neopix.setPixelColor(1, 0x00FE00);
    neopix.show();
    Serial.printf("\nPlant is still munching on a snack\n");
    return;
  }

  // Bedroom
  if (CURRENTPLANTPOS[plantPos] == 0) {
    // Lights up updated current room
    neopix.setPixelColor(0, 0xFEFE00);
    neopix.show();
    Serial.printf("\nPlant is taking a breather in bed\n");
  }
  // Kitchen
  if (CURRENTPLANTPOS[plantPos] == 1 && lastFavRoom > 10) {
    // Lights up updated current room
    neopix.setPixelColor(1, 0x00FE00);
    neopix.show();
    Serial.printf("\nPlant is getting a snack\n");
  }
  // Bathroom
  if (CURRENTPLANTPOS[plantPos] == 5) {
    // Lights up updated current room
    neopix.setPixelColor(5, 0x00A0FE);
    neopix.show();
    Serial.printf("\nPlant is in the bathroom\n");
  }
  // Meditation
  if (CURRENTPLANTPOS[plantPos] == 6 && lastFavRoom < 11) {
    pixelFillInstant(2,4,0x000000);
    neopix.show();
    // Lights up updated current room
    neopix.setPixelColor(6, 0xFFFFFF);
    neopix.show();
    Serial.printf("\nPlant is Meditating\n");
  }
  // Livingroom
  if (CURRENTPLANTPOS[plantPos] == 7) {
    // Lights up updated current room
    neopix.setPixelColor(7, 0xFE0000);
    neopix.show();
    Serial.printf("\nPlant is chilling by the fire\n");
  }
}