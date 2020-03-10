/* 
 *  Smart relay example with ESP8266-01 for sammy version 1.1.0:
 *  
 *  1. Receive message "/relay/onoff/set" with value "true" or "false".
 *  2. When "true" it will trigger LOW to GPIO2, otherwise trigger HIGH.
 *  
 */

//RTC example use this library
#include <Wire.h>
#include "RTClib.h"
RTC_DS1307 rtc; //work with DS1307 and DS3231
//============================

#include "M1128.h"

#define PROJECT_NAME "Smart Relay"
#define PROJECT_MODEL "SAM-SMR01"
#define FIRMWARE_NAME "WDB01"
#define FIRMWARE_VERSION "1.00"

#define DEBUG true
#define DEBUG_BAUD 9600

#define DEVELOPER_ROOT "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "SmartRelay"
#define WIFI_DEFAULT_PASS "abcd1234"

#define DEVICE_PIN_RESET 3

#define DEVICE_PIN_BUTTON_DEFSTATE HIGH // default is HIGH, it means active LOW.
#define DEVICE_PIN_BUTTON_OUTPUT 2 // pin GPIO2 for output

HardwareSerial *SerialDEBUG = &Serial;
M1128 iot;
String sendData="";

void setup() {
  //============================================================================
  rtc.begin();
  //adjust the RTC once, commented this if didn't want to adjust the time
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));// auto update from computer time
  //rtc.adjust(DateTime(2020, 2, 20, 0, 50, 0));// to set the time manualy 
  //=============================================================================
  if (DEBUG) {
    //SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY); // for ESP8266
    SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1); // for ESP32
    while (!SerialDEBUG);
    SerialDEBUG->println("Initializing..");
  }
  pinMode(DEVICE_PIN_BUTTON_OUTPUT,OUTPUT);
  //digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, DEVICE_PIN_BUTTON_DEFSTATE);
  pinMode(DEVICE_PIN_RESET, FUNCTION_3);
  iot.pinReset = DEVICE_PIN_RESET;
  iot.prod = true;
  iot.cleanSession = false;
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
  
  iot.onReceive = callbackOnReceive;
  iot.onConnect = callbackOnConnect;  
  iot.onReconnect = callbackOnReconnect;
  iot.onAPConfigTimeout = callbackOnAPConfigTimeout;
  iot.onWiFiConnectTimeout = callbackOnWiFiConnectTimeout;

  iot.init(DEBUG?SerialDEBUG:NULL);
  delay(10);
}

void loop() {
  iot.loop();
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
  if (strcmp(topic,iot.constructTopic("relay/onoff/set"))==0 && strPayload=="true") switchMe(!DEVICE_PIN_BUTTON_DEFSTATE,true);
  else if (strcmp(topic,iot.constructTopic("relay/onoff/set"))==0 && strPayload=="false") switchMe(DEVICE_PIN_BUTTON_DEFSTATE,true);
  else if (strcmp(topic,iot.constructTopic("relay/onoff"))==0 && strPayload=="true") switchMe(!DEVICE_PIN_BUTTON_DEFSTATE,false);
  else if (strcmp(topic,iot.constructTopic("relay/onoff"))==0 && strPayload=="false") switchMe(DEVICE_PIN_BUTTON_DEFSTATE,false);  
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

void switchMe(bool sm, bool publish) {
  digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, sm);
  if (sm) sendData = "false;"+loggingDate();
  else sendData = "true;"+loggingDate();
  if (publish && iot.mqtt->connected()) iot.mqtt->publish(iot.constructTopic("relay/onoff"), sendData.c_str(), true);   
}

void initPublish() {
  if (iot.mqtt->connected()) {       
    iot.mqtt->publish(iot.constructTopic("$nodes"), "relay", false);
  
  //define node "relay"
    iot.mqtt->publish(iot.constructTopic("relay/$name"), "Relay", false);
    iot.mqtt->publish(iot.constructTopic("relay/$type"), "Relay-01", false);
    iot.mqtt->publish(iot.constructTopic("relay/$properties"), "onoff", false);

    iot.mqtt->publish(iot.constructTopic("relay/onoff/$name"), "Relay On Off", false);
    iot.mqtt->publish(iot.constructTopic("relay/onoff/$settable"), "true", false);
    iot.mqtt->publish(iot.constructTopic("relay/onoff/$retained"), "true", false);
    iot.mqtt->publish(iot.constructTopic("relay/onoff/$datatype"), "boolean", false);
  }
}

void initSubscribe() {
  if (iot.mqtt->connected()) {
    // subscribe listen
    iot.mqtt->subscribe(iot.constructTopic("relay/onoff"),1);  
    iot.mqtt->subscribe(iot.constructTopic("relay/onoff/set"),1);
  }
}

//get datetime data and reformat to yyyy-mm-dd HH:mm:ss
String loggingDate(){
  DateTime now = rtc.now();
  String ret = String(now.year())+"-"+twoDigit(now.month())+"-"+twoDigit(now.day());
  ret = ret+" "+twoDigit(now.hour())+":"+twoDigit(now.minute())+":"+twoDigit(now.second());
  return ret;
}

//convert 0-9 to 00-09
String twoDigit(int num){
  int firstDigit = floor(num/10);
  int secondDigit = num % 10;
  return String(firstDigit)+String(secondDigit);
}
