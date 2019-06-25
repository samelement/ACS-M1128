/* 
 *  Gas Leakage Alarm example with ESP8266-01:
 *  
 *  Publish message "/sensor/lpg" with value "true" whenever GPIO0 (board pin 5) triggered with low signal.
 *  
 */
 
#include "M1128.h"

#define DEBUG true //true if you want to debug.
#define DEBUG_BAUD 9600 //debug baud rate

#define DEVELOPER_ID "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "GasAlarm"
#define WIFI_DEFAULT_PASS "abcd1234"

#define DEVICE_PIN_DEFSTATE LOW //initial/default pin state
#define DEVICE_PIN_INPUT 0 //GPIO pin input

WiFiClientSecure wclientSecure;
PubSubClient client(wclientSecure,MQTT_BROKER_HOST,MQTT_BROKER_PORT_TLS);
HardwareSerial *SerialDEBUG = &Serial;
M1128 obj;

bool pinLastState = DEVICE_PIN_DEFSTATE;
bool pinCurrentState = DEVICE_PIN_DEFSTATE;

void setup() {
  if (DEBUG) {
    SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);
    while (!SerialDEBUG);
    SerialDEBUG->println("Initializing..");
  }
  client.set_callback(callbackOnReceive);
  pinMode(DEVICE_PIN_INPUT,INPUT);
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
  checkSensor();
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

void callbackOnAPConfigTimeout() {
  obj.restart();
}

void callbackOnWiFiConnectTimeout() {
  obj.restart();
}

void checkSensor() {
  if (!client.connected()) return;
  pinCurrentState = digitalRead(DEVICE_PIN_INPUT);
  if (pinLastState==DEVICE_PIN_DEFSTATE && pinCurrentState!=pinLastState) {    
    client.publish(MQTT::Publish(obj.constructTopic("sensor/lpg"), "true").set_retain().set_qos(1)); 
  } else if (pinLastState==!DEVICE_PIN_DEFSTATE && pinCurrentState!=pinLastState) {    
    client.publish(MQTT::Publish(obj.constructTopic("sensor/lpg"), "false").set_retain().set_qos(1)); 
  }
  pinLastState = pinCurrentState;  
}

void publishState(const char* state) {
  if (client.connected()) client.publish(MQTT::Publish(obj.constructTopic("$state"), state).set_retain().set_qos(1));  
}

void initPublish() {
  if (client.connected()) {    
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "init").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$sammy"), "1.0.0").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$name"), "Gas Alarm").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$model"), "SAM-GLA01").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$mac"), WiFi.macAddress()).set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$localip"), WiFi.localIP().toString()).set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/name"), "GLA01").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/version"), "1.00").set_retain(false).set_qos(1));    
    client.publish(MQTT::Publish(obj.constructTopic("$reset"), "true").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$restart"), "true").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$nodes"), "sensor").set_retain(false).set_qos(1));
  
  //define node "bell"
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$name"), "Sensor").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$type"), "Sensor-01").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$properties"), "lpg").set_retain(false).set_qos(1));

    client.publish(MQTT::Publish(obj.constructTopic("sensor/lpg/$name"), "LPG").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/lpg/$settable"), "false").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/lpg/$retained"), "true").set_retain(false).set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/lpg/$datatype"), "boolean").set_retain(false).set_qos(1));  

  // set device to ready
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "ready").set_retain(false).set_qos(1));  
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
