#include <EEPROM.h>
#include "M1128.h"

#define SETTING_START 0
#define SETTING_CODE 78 //0-255 range (a byte)
#define SETTING_SIZE 128

#define DEBUG true
#define DEBUG_BAUD 9600

#define DEVELOPER_ID "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "SmartMotion"
#define WIFI_DEFAULT_PASS "abcd1234"

#define PAYLOAD_DELIMITER "/"
#define PAYLOAD_BUFFER_SIZE 501 //max mqtt string

WiFiClient wclient;
PubSubClient client(wclient, MQTT_BROKER_HOST, MQTT_BROKER_PORT);
HardwareSerial SerialDEBUG = Serial;
M1128 obj;

struct Setting
{
  char wifi_ssid[33]; //max 32 char
  char wifi_pass[65]; //max 64 char
  uint16_t code;
} storage = {
  "",
  "",
  SETTING_CODE
};

void setup() {
  if (DEBUG) {
    SerialDEBUG.begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);
    while (!SerialDEBUG);
    SerialDEBUG.println("Initializing..");
  }
  loadConfig();
  pinMode(3, FUNCTION_3);
  obj.pinReset = 3;
  obj.devConfig(DEVELOPER_ID,DEVELOPER_USER,DEVELOPER_PASS);
  obj.wifiConfig(storage.wifi_ssid,storage.wifi_pass);
  obj.wifiConfigAP(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS);
  obj.init(client,true,SerialDEBUG); //pass client, set clean_session=true, use debug.
  if (obj.isReady && client.connected()) {
    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion"), "true").set_retain(false).set_qos(1)); 
    initPublish();    
    client.publish(MQTT::Publish(obj.constructTopic("sensor/motion"), "true").set_retain(false).set_qos(1)); 
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "sleeping").set_retain(false).set_qos(1)); 
  }
  delay(10);
  ESP.deepSleep(0);
}

void loop() {
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

void saveConfig() {
  EEPROM.begin(SETTING_SIZE);
  EEPROM.write(0,storage.code);
  for (int i=0;i<32;i++) EEPROM.write(i+1,storage.wifi_ssid[i]); //start from byte 1 for ssid
  for (int i=0;i<64;i++) EEPROM.write(i+33,storage.wifi_pass[i]); //start from byte 33 for pass
  EEPROM.commit();
  EEPROM.end();
}

void loadConfig() {
  EEPROM.begin(SETTING_SIZE);
  bool def = EEPROM.read(SETTING_START)!= SETTING_CODE;
  if (def) {
    EEPROM.end();
    if (DEBUG) SerialDEBUG.println(F("Custom setting not valid. Set to default.."));
    strcpy(storage.wifi_ssid,"");
    strcpy(storage.wifi_pass,"");
    storage.code = SETTING_CODE;
    saveConfig();
  } else {
    for (int i=0;i<32;i++) storage.wifi_ssid[i] = EEPROM.read(i+1); //start from byte 1 for ssid
    for (int i=0;i<64;i++) storage.wifi_pass[i] = EEPROM.read(i+33); //start from byte 33 for pass
    EEPROM.end();
  }
}

void clearConfig() {
  EEPROM.begin(SETTING_SIZE);
  for (unsigned int i=0;i<SETTING_SIZE;i++) { //clear EEPROM
    EEPROM.write(i, 0);
  }
  EEPROM.end();
}
