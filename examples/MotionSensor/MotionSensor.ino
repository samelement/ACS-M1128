/* 
 *  Motion sensor example with ESP8266-01:
 *  
 *  1. Publish message "/sensor/motion" with value "true" whenever sensor triggered.
 *  2. When sensor is being triggered, it must send low signal to pin restart ESP (Board pin 6)
 *  3. ESP will immidiately publish all neccessary messages on bootup and followed publish "/sensor/motion"
 *  4. ESP will then goes to deep sleep to keep the power low.
 *  
 */

#include "M1128.h"

#define DEBUG true
#define DEBUG_BAUD 9600

#define DEVELOPER_ID "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "SmartMotion"
#define WIFI_DEFAULT_PASS "abcd1234"

WiFiClientSecure wclientSecure;
PubSubClient client(wclientSecure,MQTT_BROKER_HOST,MQTT_BROKER_PORT_TLS);
HardwareSerial *SerialDEBUG = &Serial;
M1128 obj;

void setup() {
  if (DEBUG) {
    SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);
    while (!SerialDEBUG);
    SerialDEBUG->println("Initializing..");
  }
  pinMode(3, FUNCTION_3);
  obj.pinReset = 3;
  obj.apConfigTimeout = 300000;
  obj.wifiConnectTimeout = 120000;  
  obj.wifiClientSecure = &wclientSecure;  
  obj.devConfig(DEVELOPER_ID,DEVELOPER_USER,DEVELOPER_PASS);
  obj.wifiConfig(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS);
  obj.onConnect = callbackOnConnect;
  obj.onAPConfigTimeout = callbackOnAPConfigTimeout;
  obj.onWiFiConnectTimeout = callbackOnWiFiConnectTimeout;  
  ESP.wdtEnable(8000);  
  obj.init(client,true,false,SerialDEBUG); //pass client, set clean_session=true, don't use lwt, use debug.
  delay(10);
}

void loop() {
  yield();
  ESP.wdtFeed();
  if (!obj.isReady) obj.loop();
}

void callbackOnConnect() {
  client.publish(MQTT::Publish(obj.constructTopic("sensor/motion"), "true").set_retain(false).set_qos(1)); 
  initPublish();    
  client.publish(MQTT::Publish(obj.constructTopic("$state"), "sleeping").set_retain().set_qos(1)); 
  ESP.deepSleep(0);
}

void callbackOnAPConfigTimeout() {
  ESP.deepSleep(0); // going to deep sleep forever
}

void callbackOnWiFiConnectTimeout() {
  ESP.deepSleep(0); // going to deep sleep forever
}

void initPublish() { 
  if (client.connected()) {    
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "init").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$sammy"), "1.0.0").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$name"), "Motion Sensor").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$model"), "SAM-MS01").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$mac"), WiFi.macAddress()).set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$localip"), WiFi.localIP().toString()).set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/name"), "MS01").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/version"), "1.00").set_retain(false).set_qos(1));    
    client.publish(MQTT::Publish(obj.constructTopic("$nodes"), "sensor").set_retain(false).set_qos(1));
  
  //define node "bell"
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$name"), "Sensor").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$type"), "Sensor-01").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$properties"), "motion").set_retain(false).set_qos(1));

    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion/$name"), "Motion Sensor").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion/$settable"), "false").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion/$retained"), "false").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion/$datatype"), "boolean").set_retain(false).set_qos(1));  

  // set device to ready
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "ready").set_retain(false).set_qos(1));  
  }
}
