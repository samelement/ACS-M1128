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

#define DEVELOPER_ID "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "SmartDHT22"
#define WIFI_DEFAULT_PASS "abcd1234"

WiFiClientSecure wclientSecure;
PubSubClient client(wclientSecure, MQTT_BROKER_HOST, MQTT_BROKER_PORT_TLS);
HardwareSerial *SerialDEBUG = &Serial;
M1128 obj;

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
  client.set_callback(callbackOnReceive);
  pinMode(DHTPIN,INPUT);
  pinMode(3, FUNCTION_3);
  obj.pinReset = 3;
  obj.apTimeout = 120000;
  obj.wifiTimeout = 120000;
  obj.wifiClientSecure = &wclientSecure;
  obj.devConfig(DEVELOPER_ID,DEVELOPER_USER,DEVELOPER_PASS);
  obj.wifiConfig(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS);
  obj.onConnect = callbackOnConnect;  
  obj.onReconnect = callbackOnReconnect;
  obj.onAPTimeout = callbackOnAPTimeout;
  obj.onWiFiTimeout = callbackOnWiFiTimeout;
  ESP.wdtEnable(8000);
  initSensors();
  obj.init(client,true,true,SerialDEBUG); //pass client, set clean_session=true, set lwt=true, use debug.
  delay(10);
}

void loop() {
  ESP.wdtFeed();
  obj.loop();
  if (obj.isReady) measureSensors();
}

void initSensors() {
  SerialDEBUG->println("DHTxx Unified Sensor Example");
  // Print temperature sensor details.
  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  SerialDEBUG->println("------------------------------------");
  SerialDEBUG->println("Temperature");
  SerialDEBUG->print  ("Sensor:       "); Serial.println(sensor.name);
  SerialDEBUG->print  ("Driver Ver:   "); Serial.println(sensor.version);
  SerialDEBUG->print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  SerialDEBUG->print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println(" *C");
  SerialDEBUG->print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println(" *C");
  SerialDEBUG->print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println(" *C");  
  SerialDEBUG->println("------------------------------------");
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  SerialDEBUG->println("------------------------------------");
  SerialDEBUG->println("Humidity");
  SerialDEBUG->print  ("Sensor:       "); Serial.println(sensor.name);
  SerialDEBUG->print  ("Driver Ver:   "); Serial.println(sensor.version);
  SerialDEBUG->print  ("Unique ID:    "); Serial.println(sensor.sensor_id);
  SerialDEBUG->print  ("Max Value:    "); Serial.print(sensor.max_value); Serial.println("%");
  SerialDEBUG->print  ("Min Value:    "); Serial.print(sensor.min_value); Serial.println("%");
  SerialDEBUG->print  ("Resolution:   "); Serial.print(sensor.resolution); Serial.println("%");  
  SerialDEBUG->println("------------------------------------");
  // Set delay between sensor readings based on sensor details.
  sensorDelayMS = sensor.min_delay / 1000;
}

void measureSensors() {
  // Delay between measurements.
  sensorCurMillis = millis();
  if ((sensorCurMillis - sensorPrevMillis > sensorDelayMS) || (sensorCurMillis - sensorPrevMillis < 0)) {
    sensorPrevMillis = sensorCurMillis;
    char result[6]; // Buffer big enough for 7-character float
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
      client.publish(MQTT::Publish(obj.constructTopic("sensor/temp"), result).set_retain(true).set_qos(1));
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
      client.publish(MQTT::Publish(obj.constructTopic("sensor/humid"), result).set_retain(true).set_qos(1));
    }
  }
}

void callbackOnReceive(const MQTT::Publish& pub) {
  if (DEBUG) {
    SerialDEBUG->print(F("Receiving topic: "));
    SerialDEBUG->println(pub.topic());
    SerialDEBUG->print("With value: ");
    SerialDEBUG->println(pub.payload_string());
  }
  if (pub.topic()==obj.constructTopic("reset") && pub.payload_string()=="true") obj.reset();
  else if (pub.topic()==obj.constructTopic("restart") && pub.payload_string()=="true") obj.restart();
}

void callbackOnConnect() {
  initPublish();    
  initSubscribe();
}

void callbackOnReconnect() {
  initSubscribe();
}

void callbackOnAPTimeout() {
  obj.restart();
}

void callbackOnWiFiTimeout() {
  ESP.deepSleep(300000000); // sleep for 5 minutes
}

void publishState(const char* state) {
  if (client.connected()) client.publish(MQTT::Publish(obj.constructTopic("$state"), state).set_retain().set_qos(1));  
}

void initPublish() {
  if (client.connected()) {    
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "init").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$sammy"), "1.0.0").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$name"), "Smart DHT22").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$model"), "SAM-DHT22").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$mac"), WiFi.macAddress()).set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$localip"), WiFi.localIP().toString()).set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/name"), "DHT22").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/version"), "1.00").set_retain().set_qos(1));    
    client.publish(MQTT::Publish(obj.constructTopic("$reset"), "true").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$restart"), "true").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$nodes"), "sensor").set_retain().set_qos(1));
  
  //define node "sensor"
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$name"), "Sensor").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$type"), "Sensor-01").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$properties"), "temp,humid").set_retain().set_qos(1));

    client.publish(MQTT::Publish(obj.constructTopic("sensor/temp/$name"), "Temperature").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/temp/$settable"), "false").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/temp/$retained"), "true").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/temp/$datatype"), "float").set_retain().set_qos(1));  
    client.publish(MQTT::Publish(obj.constructTopic("sensor/temp/$unit"), "°C").set_retain().set_qos(1));  
    client.publish(MQTT::Publish(obj.constructTopic("sensor/temp/$format"), "-40:125").set_retain().set_qos(1));  

    client.publish(MQTT::Publish(obj.constructTopic("sensor/humid/$name"), "Humidity").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/humid/$settable"), "false").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/humid/$retained"), "true").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/humid/$datatype"), "float").set_retain().set_qos(1));    
    client.publish(MQTT::Publish(obj.constructTopic("sensor/humid/$unit"), "%").set_retain().set_qos(1));  
    client.publish(MQTT::Publish(obj.constructTopic("sensor/humid/$format"), "0:100").set_retain().set_qos(1));  
  }
}

void initSubscribe() {
  if (client.connected()) {
    //default, must exist
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("reset"),2));  
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("restart"),2));  
  }
  publishState("ready");
}
