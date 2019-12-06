[![N|Solid](https://www.samelement.com/img/logo/logo-final-100x100.png)](https://www.samelement.com/img/logo/logo-final-100x100.png)
# ACS-M1128 SAM Element IoT WiFi Connectivity

SAM Element is an IoT platform. Visit our [website](https://www.samelement.com) to get to know more.

## Quick Links & Requirements
  - [Knowledgebase](https://ask.samelement.com)
  - [ESP8266 Documentation](https://arduino-esp8266.readthedocs.io/en/latest/index.html)
  - [Arduino ESP8266 Board Library](https://github.com/esp8266/Arduino)
  - [Arduino ESP32 Board Library](https://github.com/espressif/arduino-esp32)
  - [Arduino ESP8266FS uploader - plugin](https://github.com/esp8266/arduino-esp8266fs-plugin)
  - [Arduino ESP32FS uploader - plugin](https://github.com/me-no-dev/arduino-esp32fs-plugin)

## TESTED (Latest Version):
  - Tested with ESP8266 Board library v2.5.2
  - Tested with ESP32 Board library v1.0.4
  - It has come to our attention that some may experience sketch data upload problem when using ESP8266 board library v2.6.1 with ESP8266FS uploader plugin v0.4.0. Please follow this [issue](https://github.com/samelement/ACS-M1128/issues/1) to get solution.

## Caution:
  - You must maintain publish rate at minimum 10s.
  - SAM Element may suspend an account when an abusive behaviour detected.

## WiFi Connectivity
  - Supported board: ESP32, ESP8266.  
  - Easy connect to SAM Element IoT platform.
  - Ready to use some working examples for real application.
  - WiFi configuration page ready.
  - Auto Access Point.
  - Embedded factory reset.
  - Custom serial number.
  - Secure connection.
  - You just need to focus on sensors/actuators, we handle the rest.

# How to Use

Example connection below (and examples folder) is done for ESP8266-01. You may need to made some adjustment for different board.
### Define default values
```sh
// get your account details in your dashboard
// DO NOT use existing developer user & pass in example as we do not guarantee it will work and for how long.
#define DEVELOPER_ROOT "<insert your developer root>" 
#define DEVELOPER_USER "<insert your API Device username>"
#define DEVELOPER_PASS "<insert your API Device password>"

#define WIFI_DEFAULT_SSID "MySmartDevice" // create your own custom SSID
#define WIFI_DEFAULT_PASS "abcd1234" // put a password 8-63 chars.
```

### Define IoT object & other necessary objects
```sh
HardwareSerial *SerialDEBUG = &Serial; // optional if you wish to debug
M1128 iot;
```

### Basic Initialization
```sh
void setup() {  
  // pass your developer details
  iot.devConfig(DEVELOPER_ID,DEVELOPER_USER,DEVELOPER_PASS);
  // pass your default SSID config. Optional params are: IPAddress localip, IPAddress gateway, IPAddress subnet
  iot.wifiConfig(WIFI_DEFAULT_SSID,WIFI_DEFAULT_PASS); 
  iot.init(); // pass client
}
```

### Advanced Initialization
```sh
  // If you want to debug, initialize your SerialDEBUG
  SerialDEBUG->begin(DEBUG_BAUD, SERIAL_8N1, SERIAL_TX_ONLY);
  while (!SerialDEBUG);
  SerialDEBUG->println("Initializing..");
  
  pinMode(3, FUNCTION_3); // this will set GPIO3 (RX) to be used as input
  iot.setId("ABCDEXFGH"); // set device serial number, default is retrieved from ESP.getChipId()

  // When ESP is sleeping, pin reset will only works when device is waking up.
  // When ESP is sleeping forever, the only way to make the pin factory reset to work is by trigger it while you turn it on. 
  iot.pinReset = 3; // optional to set the factory reset pin to GPIO3, default is GPIO3
  
  // autoAP is an option to allow ESP automatically set up as AP. Default value is false
  // If set to true, when first turn on, it will go to AP if wifi connect failed.
  // Other way to go to AP is by trigger pin reset.
  iot.autoAP = false;
  
  //set whether you want to use sandbox or production server. default is sandbox (prod=false).
  iot.prod = true;
  
  //set the expire of access token in seconds. default is 43200 = 12 hours. max is 49 days.
  iot.accessTokenExp = 43200; // 12 hours
  
  //set whether mqtt will init in clean session. default  is false.
  iot.cleanSession = true;
  
  //set whether you like to use LWT message. default is true.
  iot.setWill = true;
  
  // optional set wifi connect trial before going to AP mode, default is 1  
  iot.wifiConnectRetry = 2; 
  
  // apConfigTimeout is a timeout for ESP when it works as soft AP.
  // use apConfigTimeout for low battery powered device to make sure ESP not work as AP too long. 
  // apConfigTimeout is in ms. Default is 0, which means no timeout.
  // When apConfigTimeout has passed, it will trigger onAPConfigTimeout.
  iot.apConfigTimeout = 300000;

  // if apConfigTimeout > 0 and and apConfigTimeout has passed, it will trigger a callback you can define here.
  // if this callback is not defined then after timeout it will goes to deep sleep.
  iot.onAPConfigTimeout = callbackOnAPConfigTimeout; 
  
  // wifiConnectTimeout is a timeout for ESP to keep try to connect to a WiFi AP.
  // wifiConnectTimeout is in ms. Default is 0, which means no timeout.
  // When wifiConnectTimeout has passed, it will trigger onWiFiConnectTimeout.
  iot.wifiConnectTimeout = 120000;

  // if wifiConnectTimeout > 0 and and wifiTimeout has passed, it will trigger a callback you can define here.
  // if this callback is not defined then after timeout it will goes to deep sleep.
  iot.onWiFiConnectTimeout = callbackOnWiFiConnectTimeout; 
  
  iot.onReceive = callbackOnReceive; // optional callback when message received
  iot.onReset =  callbackOnReset; // optional callback when device factory reset pressed
  iot.onConnect = callbackOnConnect; // optional callback when connected to server
  iot.onReconnect = callbackOnReconnect; // optional callback when reconnected to server
  iot.onWiFiConfigChanged = callbackOnWiFiConfigChanged; // optional callback when wifi config changed

  ESP.wdtEnable(8000); // if you wish to enable watchdog
  iot.init(SerialDEBUG); //use debug (optional).
```

### Define necessary callbacks you wish to use
```sh
void callbackOnReceive(char* topic, byte* payload, unsigned int length) {
    // your codes
}
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
void callbackOnAPConfigTimeout() {
    // ESP.deepSleep(0);
    // iot.restart(); // call to restart via software
    // iot.reset(); // call to reset the wifi configuration saved in ESP, this will trigger onReset()
    // your codes
}
void callbackOnWiFiConnectTimeout() {
    // ESP.deepSleep(0);
    // iot.restart(); // call to restart via software
    // iot.reset(); // call to reset the wifi configuration saved in ESP, this will trigger onReset()
    // your codes
}
```

### Codes in Loop
```sh
void loop() {
  ESP.wdtFeed(); // if you enable watchdog, you have to feed it
  iot.loop();
  // your other codes
}
```

# How to Flash

In every newly created project, there are 2 steps to upload your project:
* Upload WiFi Configuration Interface files with SPIFFS Data Uploader, via menu Tools -> ESP8266 Sketch Data Upload.
* Upload your sketch as usual through Sketch -> Upload (or via Upload button)

