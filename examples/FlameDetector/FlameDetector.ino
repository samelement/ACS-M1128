/* 
 *  Gas Leakage Alarm example with ESP8266-01 for sammy version 1.1.0:
 *  
 *  Publish message "/sensor/lpg" with value "true" whenever GPIO0 (board pin 5) triggered with low signal.
 *  
 */
 
#include "M1128.h"

#define PROJECT_NAME "Flame Alarm"
#define PROJECT_MODEL "SAM-FLA01"
#define FIRMWARE_NAME "FLA01"
#define FIRMWARE_VERSION "1.00"

#define DEBUG true //true if you want to debug.
#define DEBUG_BAUD 9600 //debug baud rate

#define DEVELOPER_ROOT "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "FlameAlarm"
#define WIFI_DEFAULT_PASS "abcd1234"

#define DEVICE_PIN_RESET 3

#define DEVICE_PIN_DEFSTATE HIGH //initial/default pin state
#define DEVICE_PIN_INPUT 0 //GPIO pin input
#define DEVICE_PIN_OUTPUT 2 // pin GPIO2 for output

HardwareSerial *SerialDEBUG = &Serial;
M1128 iot;

bool pinLastState = DEVICE_PIN_DEFSTATE;
bool pinCurrentState = DEVICE_PIN_DEFSTATE;

void setup() {
  if (DEBUG) {
    //SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY); // for ESP8266
    SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1); // for ESP32
    while (!SerialDEBUG);
    SerialDEBUG->println("Initializing..");
  }
  pinMode(DEVICE_PIN_INPUT,INPUT);
  pinMode(DEVICE_PIN_OUTPUT,OUTPUT);
  digitalWrite(DEVICE_PIN_OUTPUT, DEVICE_PIN_DEFSTATE);
  pinMode(DEVICE_PIN_RESET, FUNCTION_3);
  iot.pinReset = DEVICE_PIN_RESET;
  iot.prod = true;
  iot.cleanSession = true;
  iot.setWill = true;
  iot.apConfigTimeout = 300000;
  iot.wifiConnectTimeout = 120000;
  iot.devConfig(DEVELOPER_ROOT,DEVELOPER_USER,DEVELOPER_PASS);
  iot.wifiConfig(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS);
  iot.name = PROJECT_NAME;
  iot.model = PROJECT_MODEL;
  iot.fw.name = FIRMWARE_NAME;
  iot.fw.version = FIRMWARE_VERSION;
  iot.resettable = true;
  iot.restartable = true;
  
  iot.onConnect = callbackOnConnect;
  iot.onAPConfigTimeout = callbackOnAPConfigTimeout;
  iot.onWiFiConnectTimeout = callbackOnWiFiConnectTimeout;  
  
  iot.init(DEBUG?SerialDEBUG:NULL);
  delay(10);
}

void loop() {
  iot.loop();
  checkSensor();
}

void callbackOnConnect() {
  initPublish();    
}

void callbackOnAPConfigTimeout() {
  iot.restart();
}

void callbackOnWiFiConnectTimeout() {
  iot.restart();
}

void checkSensor() {
  pinCurrentState = digitalRead(DEVICE_PIN_INPUT);
  if (pinLastState==!DEVICE_PIN_DEFSTATE && pinCurrentState!=pinLastState) { // alarm ON   
    digitalWrite(DEVICE_PIN_OUTPUT, !DEVICE_PIN_DEFSTATE); // make noisy board alarm
    if (iot.mqtt->connected()) iot.mqtt->publish(iot.constructTopic("sensor/flame"), "true", true); 
  } else if (pinLastState==DEVICE_PIN_DEFSTATE && pinCurrentState!=pinLastState) {  // alarm OFF  
    digitalWrite(DEVICE_PIN_OUTPUT, DEVICE_PIN_DEFSTATE); // turn off board alarm
    if (iot.mqtt->connected()) iot.mqtt->publish(iot.constructTopic("sensor/flame"), "false", true); 
  }
  pinLastState = pinCurrentState;  
}

void initPublish() {
  if (iot.mqtt->connected()) { 
    iot.mqtt->publish(iot.constructTopic("$nodes"), "sensor", false);
  
  //define node "sensor"
    iot.mqtt->publish(iot.constructTopic("sensor/$name"), "Sensor", false);
    iot.mqtt->publish(iot.constructTopic("sensor/$type"), "Sensor-01", false);
    iot.mqtt->publish(iot.constructTopic("sensor/$properties"), "flame", false);

    iot.mqtt->publish(iot.constructTopic("sensor/flame/$name"), "FLAME", false);
    iot.mqtt->publish(iot.constructTopic("sensor/flame/$settable"), "false", false);
    iot.mqtt->publish(iot.constructTopic("sensor/flame/$retained"), "true", false);
    iot.mqtt->publish(iot.constructTopic("sensor/flame/$datatype"), "boolean", false); 
  }
}
