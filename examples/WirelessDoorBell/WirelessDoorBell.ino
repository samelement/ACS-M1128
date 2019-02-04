#include <EEPROM.h>
#include "M1128.h"

#define SETTING_START 0
#define SETTING_CODE 78 //0-255 range (a byte)
#define SETTING_SIZE 128

#define DEBUG true
#define DEBUG_BAUD 9600

#define DEVELOPER_ID "your developer id"
#define DEVELOPER_USER "your developer api username"
#define DEVELOPER_PASS "your developer api password"

#define WIFI_DEFAULT_SSID "SmartBell"
#define WIFI_DEFAULT_PASS "abcd1234"

#define DEVICE_PIN_BUTTON_DEFSTATE LOW
#define DEVICE_PIN_BUTTON_INPUT 0
#define DEVICE_PIN_BUTTON_OUTPUT 2

#define PAYLOAD_DELIMITER "/"
#define PAYLOAD_BUFFER_SIZE 501 //max mqtt string

WiFiClient wclient;
PubSubClient client(wclient, MQTT_BROKER_HOST, MQTT_BROKER_PORT);
HardwareSerial SerialDEBUG = Serial;
M1128 obj;

bool pinButtonLastState = DEVICE_PIN_BUTTON_DEFSTATE;
bool pinButtonCurrentState = DEVICE_PIN_BUTTON_DEFSTATE;

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
  client.set_callback(callback);
  pinMode(DEVICE_PIN_BUTTON_INPUT,INPUT);
  pinMode(DEVICE_PIN_BUTTON_OUTPUT,OUTPUT);
  digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, DEVICE_PIN_BUTTON_DEFSTATE);
  pinMode(3, FUNCTION_3);
  obj.pinReset = 3;
  obj.devConfig(DEVELOPER_ID,DEVELOPER_USER,DEVELOPER_PASS);
  obj.wifiConfig(storage.wifi_ssid,storage.wifi_pass);
  obj.wifiConfigAP(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS);
  obj.init(client,true,SerialDEBUG); //pass client, set clean_session=true, use debug.
  if (obj.isReady && client.connected()) {
    initPublish();    
    initSubscribe();
  }
  delay(10);
}

void loop() {
  obj.loop();
  checkBellButton();
  if (obj.onReconnect()) initSubscribe();
  if (obj.onReset()) {
    clearConfig();
    delay(1000);
    obj.restart();
  }
  if (obj.onWiFiConfigChanged()) {
    saveConfig();
    delay(1000);
    obj.restart();
  }
}

void callback(const MQTT::Publish& pub) {
  if (DEBUG) {
    SerialDEBUG.print(F("Receiving topic: "));
    SerialDEBUG.println(pub.topic());
    SerialDEBUG.print("With value: ");
    SerialDEBUG.println(pub.payload_string());
  }
  if (pub.topic()==obj.constructTopic("reset") && pub.payload_string()=="true") obj.reset();
  else if (pub.topic()==obj.constructTopic("restart") && pub.payload_string()=="true") obj.restart();
  else if (pub.topic()==obj.constructTopic("bell/button/set") && pub.payload_string()=="true") bellMe();
}

void checkBellButton() {
  if (!client.connected()) return;
  pinButtonCurrentState = digitalRead(DEVICE_PIN_BUTTON_INPUT);
  if (pinButtonLastState==DEVICE_PIN_BUTTON_DEFSTATE && pinButtonCurrentState!=pinButtonLastState) {    
    client.publish(MQTT::Publish(obj.constructTopic("bell/button"), "true").set_retain(false).set_qos(1)); 
  }
  pinButtonLastState = pinButtonCurrentState;  
}

void bellMe() {
  digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, !DEVICE_PIN_BUTTON_DEFSTATE);
  delay(300);
  digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, DEVICE_PIN_BUTTON_DEFSTATE);
  client.publish(MQTT::Publish(obj.constructTopic("bell/button"), "true").set_retain(false).set_qos(1));   
}

void initPublish() {
  if (client.connected()) {    
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "init").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$sammy"), "1.0.0").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$name"), "Wireless Bell").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$model"), "SAM-WDB01").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$mac"), WiFi.macAddress()).set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$localip"), WiFi.localIP().toString()).set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/name"), "WDB01").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$fw/version"), "1.00").set_retain().set_qos(1));    
    client.publish(MQTT::Publish(obj.constructTopic("$reset"), "true").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$restart"), "true").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("$nodes"), "bell").set_retain().set_qos(1));
  
  //define node "bell"
    client.publish(MQTT::Publish(obj.constructTopic("bell/$name"), "Bell").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("bell/$type"), "Bell-01").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("bell/$properties"), "button").set_retain().set_qos(1));

    client.publish(MQTT::Publish(obj.constructTopic("bell/button/$name"), "Bell Button").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("bell/button/$settable"), "true").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("bell/button/$retained"), "false").set_retain().set_qos(1));
    client.publish(MQTT::Publish(obj.constructTopic("bell/button/$datatype"), "boolean").set_retain().set_qos(1));  

  // set device to ready
    client.publish(MQTT::Publish(obj.constructTopic("$state"), "ready").set_retain().set_qos(1));  
  }
}

void initSubscribe() {
  if (client.connected()) {
    //default, must exist
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("reset"),2));  
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("restart"),2));  

    // subscribe listen
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("bell/button/set"),2));  
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
