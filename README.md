[![N|Solid](https://www.samelement.com/img/logo/logo-final-100x100.png)](https://www.samelement.com/img/logo/logo-final-100x100.png)
# ACS-M1128 SAM Element IoT WiFi Connectivity

SAM Element is an IoT platform. Visit our [website](https://www.samelement.com) to get to know more.

## Quick Links & Requirements
  - [Knowledgebase](https://ask.samelement.com)
  - [ESP8266 Documentation](https://arduino-esp8266.readthedocs.io/en/latest/index.html)
  - [Arduino core for ESP8266 WiFi chip](https://github.com/esp8266/Arduino)
  - [Arduino ESP8266 filesystem uploader](https://github.com/esp8266/arduino-esp8266fs-plugin)
  - [PubSubClient](https://github.com/Imroy/pubsubclient)
  - [SPIFFSReadServer by Ryan Downing](https://github.com/r-downing/SPIFFSReadServer)

## ESP8266 WiFi Connectivity
  - Support for ESP8266 board.
  - Easy connect to SAM Element IoT platform.
  - Ready to use some working examples for real application.
  - WiFi configuration page ready.
  - Auto Access Point.
  - Embedded factory reset.
  - Custom serial number.
  - Secure connection.
  - You just need to focus on sensors/actuators, we handle the rest.

# How to Use

Example connection below is done for ESP8266-01. You may need to made some adjustment for different ESP8266 series.
### Define default values
```sh
// get your account details in your dashboard
// DO NOT use existing developer user & pass in example as we do not guarantee it will work and for how long.
#define DEVELOPER_ID "<insert your developer id>" 
#define DEVELOPER_USER "<insert your API Device username>"
#define DEVELOPER_PASS "<insert your API Device password>"

#define WIFI_DEFAULT_SSID "MySmartDevice" // create your own custom SSID
#define WIFI_DEFAULT_PASS "1234" // put a password
```

### Define necessary objects
```sh
WiFiClientSecure wclientSecure;
PubSubClient client(wclientSecure, MQTT_BROKER_HOST, MQTT_BROKER_PORT_TLS);
HardwareSerial *SerialDEBUG = &Serial; // optional if you wish to debug
M1128 obj;
```

### Initialize objects
```sh
void setup() {
  // If you want to debug, initialize your SerialDEBUG
  SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);
  while (!SerialDEBUG);
  SerialDEBUG->println("Initializing..");
  
  pinMode(3, FUNCTION_3); // this will set GPIO3 (RX) to be used as input
  obj.setId("ABCDEXFGH"); // optional to set device serial number, default is retrieved from ESP.getChipId()

  // When ESP is sleeping, pin reset will only works when device is waking up.
  // When ESP is sleeping forever, the only way to make the pin factory reset to work is by trigger it while you turn it on. 
  obj.pinReset = 3; // optional to set the factory reset pin to GPIO3, default is GPIO3
  
  // autoAP is an option to allow ESP automatically set up as AP. Default value is false
  // If set to true, when first turn on, it will go to AP if wifi connect failed.
  // If set to false, when first turn on, it will go to sleep for wifiFailSleep ms. 
  // Other way to go to AP is by trigger pin reset for at least wifiFailSleep ms.
  obj.autoAP = false;
  obj.wifiFailSleep = 0; //0 means sleep forever.
  
  obj.wifiConnectRetry = 2; // optional set wifi connect trial before going to AP mode, default is 3  
  obj.wifiClientSecure = &wclientSecure;  
  
  // pass your developer details
  obj.devConfig(DEVELOPER_ID,DEVELOPER_USER,DEVELOPER_PASS);

  // pass your default SSID config. Optional params are: IPAddress localip, IPAddress gateway, IPAddress subnet
  obj.wifiConfig(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS); 
  
  obj.onReset =  callbackOnReset; // optional callback
  obj.onConnect = callbackOnConnect; // optional callback
  obj.onReconnect = callbackOnReconnect; // optional callback
  obj.onWiFiConfigChanged = callbackOnWiFiConfigChanged; // optional callback
  
  // apTimeout is a timeout for ESP when it works as soft AP.
  // use apTimeout for low battery powered device to make sure ESP not work as AP too long. 
  // apTimeout is in ms. Default is 0, which means no timeout.
  // When apTimeout has passed, it will trigger onAPTimeout.
  obj.apTimeout = 120000;

  // if apTimeout > 0 and and apTimeout has passed, it will trigger a callback you can define here.
  // if this callback is not defined then after timeout it will goes to deep sleep.
  obj.onAPTimeout = callbackOnAPTimeout; 
  
  // wifiTimeout is a timeout for ESP to keep try to connect to a WiFi AP.
  // wifiTimeout is in ms. Default is 0, which means no timeout.
  // When wifiTimeout has passed, it will trigger onWiFiTimeout.
  obj.wifiTimeout = 120000;

  // if wifiTimeout > 0 and and wifiTimeout has passed, it will trigger a callback you can define here.
  // if this callback is not defined then after timeout it will goes to deep sleep.
  obj.onWiFiTimeout = callbackOnWiFiTimeout; 
  
  ESP.wdtEnable(8000); // if you wish to enable watchdog
  obj.init(client,true,true,SerialDEBUG); // pass client, set clean_session=true, set lwt=true, use debug (optional).
}
```

### Define necessary callbacks you wish to use
```sh
void callbackOnReset() {
    // your codes
}
void callbackOnConnect() {
    // your codes
    // start publish your MQTT message and
    // start subscribe to MQTT topics you wanna to listen
}
void callbackOnReconnect() {
    // your codes
    // if you use set clean_session=true, then you need to re-subscribe here
}
void callbackOnWiFiConfigChanged() {
    // your codes
}
void callbackOnAPTimeout() {
    // ESP.deepSleep(0);
    // obj.restart(); // call to restart via software
    // obj.reset(); // call to reset the wifi configuration saved in ESP, this will trigger onReset()
    // your codes
}
void callbackOnWiFiTimeout() {
    // ESP.deepSleep(0);
    // your codes
}
```

### Codes in Loop
```sh
void loop() {
  ESP.wdtFeed(); // if you enable watchdog, you have to feed it
  obj.loop();
  // your other codes
}
```

# How to Flash

In every newly created project, there are 2 steps to upload your project:
* Upload WiFi Configuration Interface files with SPIFFS Data Uploader, via menu Tools -> ESP8266 Sketch Data Upload.
* Upload your sketch as usual through Sketch -> Upload (or via Upload button)


# License

This code is released under the MIT License.

Arduino IDE is developed and maintained by the Arduino team. The IDE is licensed under GPL.
ESP8266 core includes an xtensa gcc toolchain, which is also under GPL.
Esptool written by Christian Klippel is licensed under GPLv2, currently maintained by Ivan Grokhotkov: https://github.com/igrr/esptool-ck.
Espressif SDK included in this build is under Espressif MIT License.
ESP8266 core files are licensed under LGPL.
SPI Flash File System (SPIFFS) written by Peter Andersson is used in this project. It is distributed under MIT license.
umm_malloc memory management library written by Ralph Hempel is used in this project. It is distributed under MIT license.
axTLS library written by Cameron Rich, built from https://github.com/igrr/axtls-8266, is used in this project. It is distributed under BSD license.
BearSSL library written by Thomas Pornin, built from https://github.com/earlephilhower/bearssl-esp8266, is used in this project. It is distributed under the MIT License.
pubsubclient is used in this project. It is distributed under MIT license.
