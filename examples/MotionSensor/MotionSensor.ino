#include "M1128.h"

#define SECURE true
#define DEBUG true
#define DEBUG_BAUD 9600

#define DEVELOPER_ID "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "SmartMotion"
#define WIFI_DEFAULT_PASS "abcd1234"

WiFiClient wclient;
WiFiClientSecure wclientSecure;
PubSubClient client(SECURE?wclientSecure:wclient, MQTT_BROKER_HOST, SECURE?MQTT_BROKER_PORT_TLS:MQTT_BROKER_PORT);
HardwareSerial SerialDEBUG = Serial;
M1128 obj;

void setup() {
  if (DEBUG) {
    SerialDEBUG.begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);
    while (!SerialDEBUG);
    SerialDEBUG.println("Initializing..");
  }
  pinMode(3, FUNCTION_3);
  obj.pinReset = 3;
  if (SECURE) obj.wifiClientSecure = &wclientSecure;  
  obj.devConfig(DEVELOPER_ID,DEVELOPER_USER,DEVELOPER_PASS);
  obj.wifiConfig(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS);
  obj.onConnect = callbackOnConnect;
  ESP.wdtEnable(8000);  
  obj.init(client,true,SerialDEBUG); //pass client, set clean_session=true, use debug.
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

void initPublish() { 
  if (client.connected()) {    
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "init").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$sammy"), "1.0.0").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$name"), "Motion Sensor").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$model"), "SAM-MS01").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$mac"), WiFi.macAddress()).set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$localip"), WiFi.localIP().toString()).set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/name"), "MS01").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/version"), "1.00").set_retain().set_qos(1));    
    client.publish(MQTT::Publish(obj.constructTopic("$nodes"), "sensor").set_retain().set_qos(1));
  
  //define node "bell"
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$name"), "Sensor").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$type"), "Sensor-01").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/$properties"), "motion").set_retain().set_qos(1));

    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion/$name"), "Motion Sensor").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion/$settable"), "false").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion/$retained"), "false").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion/$datatype"), "boolean").set_retain().set_qos(1));  

  // set device to ready
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "ready").set_retain().set_qos(1));  
  }
}
