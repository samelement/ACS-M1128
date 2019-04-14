#include "M1128.h"

#define DEBUG true
#define DEBUG_BAUD 9600

#define DEVELOPER_ID "1"
#define DEVELOPER_USER "dmI0OkvoFRLRzHu3J3tEWQbIXQwDeF9q"
#define DEVELOPER_PASS "dyUiAb1cjkS8FRrokTXxtY1s4DUmOJsa"

#define WIFI_DEFAULT_SSID "SmartBell"
#define WIFI_DEFAULT_PASS "abcd1234"

#define DEVICE_PIN_BUTTON_DEFSTATE HIGH // default is HIGH, it means active LOW.
#define DEVICE_PIN_OUTPUT_DEFSTATE LOW
#define DEVICE_PIN_BUTTON_INPUT 0  // pin GPIO0 for input
#define DEVICE_PIN_BUTTON_OUTPUT 2 // pin GPio2 for output

WiFiClientSecure wclientSecure;
PubSubClient client(wclientSecure, MQTT_BROKER_HOST, MQTT_BROKER_PORT_TLS);
HardwareSerial SerialDEBUG = Serial;
M1128 obj;

bool pinButtonLastState = DEVICE_PIN_BUTTON_DEFSTATE;
bool pinButtonCurrentState = DEVICE_PIN_BUTTON_DEFSTATE;
bool checkFinished = false, currentState, prevState, onceOnly = false;
uint8_t counter = 0, count = 0;
unsigned long currentMillis, currentMillis1, prevMillis, prevMillis1;

void setup()
{
  if (DEBUG) {
    SerialDEBUG.begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);
    while (!SerialDEBUG);
    SerialDEBUG.println("Initializing..");
  }
  client.set_callback(callbackOnReceive);
  pinMode(DEVICE_PIN_BUTTON_INPUT, INPUT_PULLUP);
  pinMode(DEVICE_PIN_BUTTON_OUTPUT, OUTPUT);
  digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, DEVICE_PIN_OUTPUT_DEFSTATE);
  pinMode(3, FUNCTION_3);
  obj.pinReset = 3;
  obj.apTimeout = 120000;
  obj.wifiClientSecure = &wclientSecure;
  obj.devConfig(DEVELOPER_ID, DEVELOPER_USER, DEVELOPER_PASS);
  obj.wifiConfig(WIFI_DEFAULT_SSID, WIFI_DEFAULT_PASS);
  obj.onConnect = callbackOnConnect;
  obj.onReconnect = callbackOnReconnect;
  ESP.wdtEnable(8000);
  obj.init(client, true, SerialDEBUG); //pass client, set clean_session=true, use debug.
  delay(10);
  prevMillis = millis();
  prevMillis1 = millis();
  onceOnly = false;
}

void loop()
{
  ESP.wdtFeed();
  obj.loop();
  checkBellButton();
}

void callbackOnReceive(const MQTT::Publish &pub)
{
  if (DEBUG)
  {
    SerialDEBUG.print(F("Receiving topic: "));
    SerialDEBUG.println(pub.topic());
    SerialDEBUG.print("With value: ");
    SerialDEBUG.println(pub.payload_string());
  }
  if (pub.topic() == obj.constructTopic("reset") && pub.payload_string() == "true")
    obj.reset();
  else if (pub.topic() == obj.constructTopic("restart") && pub.payload_string() == "true")
    obj.restart();
  else if (pub.topic() == obj.constructTopic("bell/button/set") && pub.payload_string() == "true")
    bellMe();
}

void callbackOnConnect()
{
  initPublish();
  initSubscribe();
}

void callbackOnReconnect()
{
  initSubscribe();
}

void callbackOnAPTimeout() {
  //ESP.deepSleep(300000000); // sleep for 5 minutes
  obj.restart();
}

void checkBellButton()
{
  currentState = digitalRead(DEVICE_PIN_BUTTON_INPUT);
  currentMillis = millis();
  if ((millis() % 1000) == 0)
    SerialDEBUG.println(count);

  if (currentMillis - prevMillis <= 4000 && onceOnly == false) // LOOP IF IT IS LESS THAN (<=)
  {
    if (currentState != prevState && onceOnly == false)
    {
      count++;
    }
    if (count > 15 && onceOnly == false) // setting pulse count trigger 10 to 15 ;
    {
      client.publish(MQTT::Publish(obj.constructTopic("bell/button"), "true").set_retain(false).set_qos(1));
      SerialDEBUG.println("THIS TRIGGERED!");
      onceOnly = true;
    }
  }
  else
  {
    onceOnly = false;
    count = 0;
    if (prevState == currentState)
      count = 0;
    prevMillis = currentMillis;
  }
  prevState = currentState;
}

void bellMe()
{
  for (int a = 0; a < 3; a++)
  {
    digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, !DEVICE_PIN_OUTPUT_DEFSTATE);
    delay(150);
    digitalWrite(DEVICE_PIN_BUTTON_OUTPUT, DEVICE_PIN_OUTPUT_DEFSTATE);
    delay(150);
  }  
}

void publishState(const char *state)
{
  if (client.connected())
    client.publish(MQTT::Publish(obj.constructTopic("$state"), state).set_retain().set_qos(1));
}

void initPublish()
{
  if (client.connected())
  {
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
  }
}

void initSubscribe()
{
  if (client.connected())
  {
    //default, must exist
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("reset"), 2));
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("restart"), 2));

    // subscribe listen
    client.subscribe(MQTT::Subscribe().add_topic(obj.constructTopic("bell/button/set"), 2));
  }
  publishState("ready");
}
