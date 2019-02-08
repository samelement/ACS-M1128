#define USING_AXTLS
#include <EEPROM.h>
#include <time.h>
#include <ESP8266WiFi.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <FS.h>

// force use of AxTLS (BearSSL is now default)
#include <WiFiClientSecureAxTLS.h>
using namespace axTLS;

#ifndef M1128_h
#define M1128_h

#define MQTT_BROKER_HOST "iot.samelement.com"
#define MQTT_BROKER_PORT 1883
#define MQTT_BROKER_PORT_TLS 8883
#define MQTT_WILL_TOPIC "$state"
#define MQTT_WILL_VALUE "lost"
#define MQTT_WILL_QOS 1
#define MQTT_WILL_RETAIN true
#define MQTT_KEEPALIVE 15
#define MQTT_PATH_CA "/ca.crt"

#define PAYLOAD_DELIMITER "/"
#define PAYLOAD_BUFFER_SIZE 501

#define PIN_RESET 3

class M1128 {
  public:
    M1128();

    uint8_t pinReset = PIN_RESET;
    void init(PubSubClient &mqttClient, bool cleanSession);
    void init(PubSubClient &mqttClient, bool cleanSession, Stream &serialDebug);
    bool isReady = false;
    WiFiClientSecure *wifiClientSecure;

    void reset();
    void restart();
    void loop();
    void devConfig(const char* dev_id, const char* dev_user, const char* dev_pass);
    void wifiConfig(const char* ap_ssid, const char* ap_pass);
    void wifiConfig(const char* ap_ssid, const char* ap_pass, IPAddress localip, IPAddress gateway, IPAddress subnet);
    const char* myId();
    void setId(const char* id);
    const char* constructTopic(const char* topic);
    bool onWiFiConfigChanged();
    bool onReconnect();
    bool onReset();
  private:   
    HardwareSerial *_serialBee;
    Stream *_serialDebug;
    PubSubClient *_mqttClient;
    IPAddress _wifi_ap_localip;
    IPAddress _wifi_ap_gateway;
    IPAddress _wifi_ap_subnet;
    
    const char* _dev_id;
    const char* _dev_user;
    const char* _dev_pass;    
    const char* _wifi_ap_ssid;
    const char* _wifi_ap_pass;
    char _wifi_st_ssid[33];
    char _wifi_st_pass[65];
    bool _mqttCleanSession = false;
    bool _onWifiConfigChanged = false;
    bool _onReset = false;
    bool _onReconnect = false;
    
    uint8_t _pinResetButtonLast = HIGH;
    char _topic_buf[PAYLOAD_BUFFER_SIZE];
    char _myAddr[33];
    char _custAddr[33];

    void _initNetwork();
    void _checkResetButton();
    bool _wifiConnect();
    bool _wifiSoftAP();
    bool _mqttConnect();
    void _retrieveDeviceId();
    String _getContentType(String filename);
    void _handleWifiConfig();
    void _handleNotFound();
    bool _handleFileRead(String path);
};

#endif
