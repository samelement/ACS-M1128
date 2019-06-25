/* 
 *  Smart relay example with ESP8266-01:
 *  
 *  1. Receive message "/relay/onoff/set" with value "true" or "false".
 *  2. When "true" it will trigger LOW to GPIO2, otherwise trigger HIGH.
 *  
 */

#include "M1128.h"

#define DEBUG true
#define DEBUG_BAUD 9600

#define DEVELOPER_ID "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "SmartRelay"
#define WIFI_DEFAULT_PASS "abcd1234"

#define DEVICE_PIN_BUTTON_DEFSTATE HIGH // default is HIGH, it means active LOW.
#define DEVICE_PIN_BUTTON_OUTPUT 2 // pin GPIO2 for output

WiFiClientSecure wclientSecure;
PubSubClient client(wclientSecure, MQTT_BROKER_HOST, MQTT_BROKER_PORT_TLS);
HardwareSerial *SerialDEBUG = &Serial;
M1128 obj;

void setup() {
  if (DEBUG) {
    SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);
    while (!SerialDEBUG);
    SerialDEBUG->println("Initializing..");
  }
  client.set_callback(callbackOnReceive);
  pinMode(DEVICE_PIN_BUTTON_OUTPUT,OUTPUT);
  //digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, DEVICE_PIN_BUTTON_DEFSTATE);
  pinMode(3, FUNCTION_3);
  obj.pinReset = 3;
  obj.apConfigTimeout = 300000;
  obj.wifiConnectTimeout = 120000;
  obj.wifiClientSecure = &wclientSecure;
  obj.devConfig(DEVELOPER_ID,DEVELOPER_USER,DEVELOPER_PASS);
  obj.wifiConfig(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS);
  obj.onConnect = callbackOnConnect;  
  obj.onReconnect = callbackOnReconnect;
  obj.onAPConfigTimeout = callbackOnAPConfigTimeout;
  obj.onWiFiConnectTimeout = callbackOnWiFiConnectTimeout;
  ESP.wdtEnable(8000);
  obj.init(client,true,true,SerialDEBUG); //pass client, set clean_session=true, set lwt=true, use debug.
  delay(10);
}

void loop() {
  ESP.wdtFeed();
  obj.loop();
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
  else if (pub.topic()==obj.constructTopic("relay/onoff/set") && pub.payload_string()=="true") switchMe(!DEVICE_PIN_BUTTON_DEFSTATE);
  else if (pub.topic()==obj.constructTopic("relay/onoff/set") && pub.payload_string()=="false") switchMe(DEVICE_PIN_BUTTON_DEFSTATE);
}

void callbackOnConnect() {
  initPublish();    
  initSubscribe();
}

void callbackOnReconnect() {
  initSubscribe();
}

void callbackOnAPConfigTimeout() {
  obj.restart();
}

void callbackOnWiFiConnectTimeout() {
  obj.restart();
  //ESP.deepSleep(300000000); // sleep for 5 minutes
}

void switchMe(bool sm) {
  digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, sm);
  client.publish(MQTT::Publish(obj.constructTopic("relay/onoff"), sm?"false":"true").set_retain().set_qos(1));   
}

void publishState(const char* state) {
  if (client.connected()) client.publish(MQTT::Publish(obj.constructTopic("$state"), state).set_retain().set_qos(1));  
}

void initPublish() {
  if (client.connected()) {    
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "init").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$sammy"), "1.0.0").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$name"), "Smart Relay").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$model"), "SAM-SMR01").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$mac"), WiFi.macAddress()).set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$localip"), WiFi.localIP().toString()).set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/name"), "WDB01").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/version"), "1.00").set_retain(false).set_qos(1));    
    client.publish(MQTT::Publish(obj.constructTopic("$reset"), "true").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$restart"), "true").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$nodes"), "relay").set_retain(false).set_qos(1));
  
  //define node "relay"
    client.publish(MQTT::Publish(obj.constructTopic("relay/$name"), "Relay").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("relay/$type"), "Relay-01").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("relay/$properties"), "onoff").set_retain(false).set_qos(1));

    client.publish(MQTT::Publish(obj.constructTopic("relay/onoff/$name"), "Relay On Off").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("relay/onoff/$settable"), "true").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("relay/onoff/$retained"), "true").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("relay/onoff/$datatype"), "boolean").set_retain(false).set_qos(1));  
  }
}

void initSubscribe() {
  if (client.connected()) {
    //default, must exist
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("reset"),2));  
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("restart"),2));  

    // subscribe listen
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("relay/onoff/set"),2));  
  }
  publishState("ready");
}
