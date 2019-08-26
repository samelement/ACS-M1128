/* 
 *  DHT22 sensor example with ESP8266-01:
 *  
 *  1. Publish message every sensorDelayMS to "/sensor/temp" and "/sensor/humid" for temperature and humidity.
 *  2. Connect DHT22 data out to GPIO0.
 *  3. This example use Adafruit unified sensor library for DHT22 reading, hence you must install it first.
 *  
 */

#include "M1128.h"
#include <Adafruit_Sensor.h>
#include <DHT.h>
#include <DHT_U.h>

#define DEBUG true
#define DEBUG_BAUD 9600

#define DEVELOPER_ROOT "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "SmartDHT22"
#define WIFI_DEFAULT_PASS "abcd1234"

HardwareSerial *SerialDEBUG = &Serial;
M1128 iot;

#define DHTPIN            0         // Pin which is connected to the DHT sensor.
#define DHTTYPE           DHT22     // DHT 22 (AM2302)
DHT_Unified dht(DHTPIN, DHTTYPE);
unsigned int sensorDelayMS = 0;
unsigned int sensorCurMillis = 0;
unsigned int sensorPrevMillis = 0;

void setup() {
  if (DEBUG) {
    SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);
    while (!SerialDEBUG);
    SerialDEBUG->println("Initializing..");
  }
  pinMode(DHTPIN,INPUT);
  pinMode(3, FUNCTION_3);
  iot.pinReset = 3;
  iot.prod = false;
  iot.cleanSession = true;
  iot.setWill = true;
  iot.apConfigTimeout = 300000;
  iot.wifiConnectTimeout = 120000;
  iot.devConfig(DEVELOPER_ROOT,DEVELOPER_USER,DEVELOPER_PASS);
  iot.wifiConfig(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS);
  
  iot.onReceive = callbackOnReceive;
  iot.onConnect = callbackOnConnect;
  iot.onReconnect = callbackOnReconnect;
  iot.onAPConfigTimeout = callbackOnAPConfigTimeout;
  iot.onWiFiConnectTimeout = callbackOnWiFiConnectTimeout;  
  
  ESP.wdtEnable(8000);      
  initSensors();
  iot.init(DEBUG?SerialDEBUG:NULL);
  delay(10);  
}

void loop() {
  ESP.wdtFeed();
  iot.loop();
  if (iot.isReady) measureSensors();
}

void initSensors() {
  SerialDEBUG->println("DHTxx Unified Sensor Example");
  // Print temperature sensor details.
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  SerialDEBUG->println("------------------------------------");
  SerialDEBUG->println("Temperature");
  SerialDEBUG->print  ("Sensor:       "); SerialDEBUG->println(sensor.name);
  SerialDEBUG->print  ("Driver Ver:   "); SerialDEBUG->println(sensor.version);
  SerialDEBUG->print  ("Unique ID:    "); SerialDEBUG->println(sensor.sensor_id);
  SerialDEBUG->print  ("Max Value:    "); SerialDEBUG->print(sensor.max_value); SerialDEBUG->println(" *C");
  SerialDEBUG->print  ("Min Value:    "); SerialDEBUG->print(sensor.min_value); SerialDEBUG->println(" *C");
  SerialDEBUG->print  ("Resolution:   "); SerialDEBUG->print(sensor.resolution); SerialDEBUG->println(" *C");  
  SerialDEBUG->println("------------------------------------");
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  SerialDEBUG->println("------------------------------------");
  SerialDEBUG->println("Humidity");
  SerialDEBUG->print  ("Sensor:       "); SerialDEBUG->println(sensor.name);
  SerialDEBUG->print  ("Driver Ver:   "); SerialDEBUG->println(sensor.version);
  SerialDEBUG->print  ("Unique ID:    "); SerialDEBUG->println(sensor.sensor_id);
  SerialDEBUG->print  ("Max Value:    "); SerialDEBUG->print(sensor.max_value); SerialDEBUG->println("%");
  SerialDEBUG->print  ("Min Value:    "); SerialDEBUG->print(sensor.min_value); SerialDEBUG->println("%");
  SerialDEBUG->print  ("Resolution:   "); SerialDEBUG->print(sensor.resolution); SerialDEBUG->println("%");  
  SerialDEBUG->println("------------------------------------");
  // Set delay between sensor readings based on sensor details.
  //sensorDelayMS = sensor.min_delay / 1000;
  sensorDelayMS = 60000;
}

void measureSensors() {
  // Delay between measurements.
  sensorCurMillis = millis();
  if ((sensorCurMillis - sensorPrevMillis > sensorDelayMS) || (sensorCurMillis - sensorPrevMillis < 0)) {
    sensorPrevMillis = sensorCurMillis;
    char result[6]; // Buffer big enough for 6-character float
    // Get temperature event and print its value.
    sensors_event_t event;  
    dht.temperature().getEvent(&event);
    if (isnan(event.temperature)) {
      SerialDEBUG->println("Error reading temperature!");
    }
    else {
      SerialDEBUG->print("Temperature: ");
      SerialDEBUG->print(event.temperature);
      SerialDEBUG->println(" °C");
      dtostrf(event.temperature, 4, 2, result);
      iot.mqtt->publish(iot.constructTopic("sensor/temp"), result, true);
    }
    // Get humidity event and print its value.
    dht.humidity().getEvent(&event);
    if (isnan(event.relative_humidity)) {
      SerialDEBUG->println("Error reading humidity!");
    }
    else {
      SerialDEBUG->print("Humidity: ");
      SerialDEBUG->print(event.relative_humidity);
      SerialDEBUG->println("%");
      dtostrf(event.relative_humidity, 4, 2, result);
      iot.mqtt->publish(iot.constructTopic("sensor/humid"), result, true);
    }
  }
}

void callbackOnReceive(char* topic, byte* payload, unsigned int length) {
  String strPayload;
  strPayload.reserve(length);
  for (uint32_t i = 0; i < length; i++) strPayload += (char)payload[i];

  if (DEBUG) {
    SerialDEBUG->print(F("Receiving topic: "));
    SerialDEBUG->println(topic);
    SerialDEBUG->print("With value: ");
    SerialDEBUG->println(strPayload);
  }
  if (topic==iot.constructTopic("reset") && strPayload=="true") iot.reset();
  else if (topic==iot.constructTopic("restart") && strPayload=="true") iot.restart();
}

void callbackOnConnect() {
  initPublish();    
  initSubscribe();
}

void callbackOnReconnect() {
  initSubscribe();
}

void callbackOnAPConfigTimeout() {
  iot.restart();
}

void callbackOnWiFiConnectTimeout() {
  iot.restart();
  //ESP.deepSleep(300000000); // sleep for 5 minutes
}

void publishState(const char* state) {
  if (iot.mqtt->connected()) iot.mqtt->publish(iot.constructTopic("$state"), state, true);  
}

void initPublish() {
  if (iot.mqtt->connected()) {    
    iot.mqtt->publish(iot.constructTopic("$state"), "init", false);
    iot.mqtt->publish(iot.constructTopic("$sammy"), "1.0.0", false);
    iot.mqtt->publish(iot.constructTopic("$name"), "Smart DHT22", false);
    iot.mqtt->publish(iot.constructTopic("$model"), "SAM-DHT22", false);
    iot.mqtt->publish(iot.constructTopic("$mac"), WiFi.macAddress().c_str(), false);
    iot.mqtt->publish(iot.constructTopic("$localip"), WiFi.localIP().toString().c_str(), false);
    iot.mqtt->publish(iot.constructTopic("$fw/name"), "DHT22", false);
    iot.mqtt->publish(iot.constructTopic("$fw/version"), "1.00", false);    
    iot.mqtt->publish(iot.constructTopic("$reset"), "true", false);
    iot.mqtt->publish(iot.constructTopic("$restart"), "true", false);
    iot.mqtt->publish(iot.constructTopic("$nodes"), "sensor", false);
  
  //define node "sensor"
    iot.mqtt->publish(iot.constructTopic("sensor/$name"), "Sensor", false);
    iot.mqtt->publish(iot.constructTopic("sensor/$type"), "Sensor-01", false);
    iot.mqtt->publish(iot.constructTopic("sensor/$properties"), "temp,humid", false);

    iot.mqtt->publish(iot.constructTopic("sensor/temp/$name"), "Temperature", false);
    iot.mqtt->publish(iot.constructTopic("sensor/temp/$settable"), "false", false);
    iot.mqtt->publish(iot.constructTopic("sensor/temp/$retained"), "true", false);
    iot.mqtt->publish(iot.constructTopic("sensor/temp/$datatype"), "float", false);  
    iot.mqtt->publish(iot.constructTopic("sensor/temp/$unit"), "°C", false);  
    iot.mqtt->publish(iot.constructTopic("sensor/temp/$format"), "-40:125", false);  

    iot.mqtt->publish(iot.constructTopic("sensor/humid/$name"), "Humidity", false);
    iot.mqtt->publish(iot.constructTopic("sensor/humid/$settable"), "false", false);
    iot.mqtt->publish(iot.constructTopic("sensor/humid/$retained"), "true", false);
    iot.mqtt->publish(iot.constructTopic("sensor/humid/$datatype"), "float", false);    
    iot.mqtt->publish(iot.constructTopic("sensor/humid/$unit"), "%", false);  
    iot.mqtt->publish(iot.constructTopic("sensor/humid/$format"), "0:100", false);  
  }
}

void initSubscribe() {
  if (iot.mqtt->connected()) { 
    iot.mqtt->subscribe(iot.constructTopic("reset"),1);  
    iot.mqtt->subscribe(iot.constructTopic("restart"),1);  
  }
  publishState("ready");
}
