/*
  M1128.h - WiFi Connectivity SAM Element IoT Platform.
  SAM Element
  https://samelement.com
*/

#define USING_AXTLS
#include <time.h>
#include <EEPROM.h>
#include "PubSubClient.h"
#include "FS.h"

#if defined(ESP8266)

#include <base64.h>
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <ESP8266httpUpdate.h>
// force use of AxTLS (BearSSL is now default)
#include <WiFiClientSecureAxTLS.h>
using namespace axTLS;

#elif defined(ESP32)

#include <esp_wifi.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include "SPIFFS.h"
#include <HTTPUpdate.h>

#endif


#ifndef M1128_h
#define M1128_h

#define PATH_CA "/ca.crt"
#define AUTH_SERVER_HOST "iot.samelement.com" //production
#define AUTH_SERVER_HOST_SBX "iot-sbx.samelement.com" //sandbox
#define AUTH_SERVER_PORT 443
#define AUTH_SERVER_PATH "/auth"
#define AUTH_TIMEOUT 15000
#define AUTH_ACCESS_TOKEN_EXP 43200 // 43200s = 12 hours, max 49 days

#define MQTT_BROKER_HOST "iot.samelement.com" //production
#define MQTT_BROKER_HOST_SBX "iot-sbx.samelement.com" //sandbox
#define MQTT_BROKER_PORT 8883
#define MQTT_WILL_TOPIC "$state"
#define MQTT_WILL_VALUE "lost"
#define MQTT_WILL_QOS 1
#define MQTT_WILL_RETAIN true
#define MQTT_KEEPALIVE 15
#define MQTT_PWD "jwt"
#define MQTT_TOPIC_DELIMITER "/"
#define MQTT_PAYLOAD_SIZE 501

#define PIN_RESET 3
#define WIFI_RETRY 1
#define AP_TIMEOUT 0 // in ms. 0 is no limit. 
#define WIFI_FAIL_TIMEOUT 0 // in ms. 0 is no limit.

#if defined(ESP8266) || defined(ESP32)
#include <functional>
typedef std::function<void()> callbackFunction;
typedef std::function<void(char*, uint8_t*, unsigned int)> callbackReceive;
#else
typedef void (*callbackFunction) ();
typedef void (*callbackReceive)(char*, uint8_t*, unsigned int);
#endif

class M1128 {
  public:
    M1128();
	
    class firmware {
      public:
        const char* name;
        const char* version;
    };
    firmware fw;
    const char* sammy = "1.1.0";
    const char* model;
    const char* name;
    
    uint8_t pinReset = PIN_RESET;
    uint8_t wifiConnectRetry = WIFI_RETRY;
    uint32_t apConfigTimeout = AP_TIMEOUT;
    uint32_t wifiConnectTimeout = WIFI_FAIL_TIMEOUT;
    void init();
    void init(Stream *serialDebug);
    bool isReady = false;
    bool autoAP = false;
    bool secure = true;
    bool prod = false;
    bool cleanSession = false;
    bool setWill = true;
	bool resettable = false;
	bool restartable = false;
    uint32_t accessTokenExp = AUTH_ACCESS_TOKEN_EXP;
	
    PubSubClient *mqtt;
    
    callbackFunction onReset;
    callbackFunction onConnect;    
    callbackFunction onReconnect;
    callbackFunction onWiFiConfigChanged;
    callbackFunction onAPConfigTimeout;
    callbackFunction onWiFiConnectTimeout;
    callbackReceive onReceive;

    void reset();
    void restart();
    bool refreshAuth();
    void loop();
    void devConfig(const char* dev_id, const char* dev_user, const char* dev_pass);
    void wifiConfig(const char* ap_ssid, const char* ap_pass);
    void wifiConfig(const char* ap_ssid, const char* ap_pass, IPAddress localip, IPAddress gateway, IPAddress subnet);
    const char* myId();
    void setId(const char* id);
    const char* constructTopic(const char* topic);
	
  private:
    Stream *_serialDebug;
    IPAddress _wifi_ap_localip;
    IPAddress _wifi_ap_gateway;
    IPAddress _wifi_ap_subnet;
   
    char _tokenRefresh[1000];
    char _tokenAccess[1000];
    
    const char *_authHost;
    const char *_mqttHost;
    const char* _dev_id;
    const char* _dev_user;
    const char* _dev_pass;    
    const char* _wifi_ap_ssid;
    const char* _wifi_ap_pass;
    uint8_t _wifiConnectRetryVal = 0;
    uint32_t _softAPStartMillis = 0;
    uint32_t _softAPCurrentMillis = 0;
    uint32_t _wifiFailStartMillis = 0;
    uint32_t _wifiFailCurrentMillis = 0;
    uint32_t _timeStartMillis = 0;
    uint32_t _timeCurrentMillis = 0;
        
    uint8_t _pinResetButtonLast = HIGH;
    const char* _constructTopicModel(const char* topic);
    char _topic_buf[MQTT_PAYLOAD_SIZE];
    char _myAddr[33];
    char _custAddr[33];
    bool _startWiFi = false;
  
    void _initNetwork(bool goAP);
    bool _checkResetButton();
    bool _wifiConnect();
    bool _wifiConnect(const char* ssid, const char* password);
    bool _wifiSoftAP();
    bool _mqttConnect();
    void _retrieveDeviceId();
    String _getContentType(String filename);
    void _handleOnReceive(char* topic, uint8_t* payload, unsigned int length); 
    void _handleWifiConfig();
    void _handleNotFound();
    bool _handleFileRead(String path);
    
    void _initPublish();
    void _initSubscribe();
    void _updateCollectionPublish(String payload);
    char _date[20];
    const char* _dateTime();
    void _twoDigit(int num);
    String _updateCollection;
    const char* _constructUpdateState(const char* status, const char* message, const char* date);
    void _triggerUpdate(String newVersion);
    void _updateExecution(int errorStatus, String urlUpdate);
    char _mode[8] = { 'a', 'u', 't', 'o','\0','\0','\0'};
    
};

#endif
