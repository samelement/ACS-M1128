#include "M1128.h"

ESP8266WebServer _wifi_ap_server(80);

//M1128
M1128::M1128() { 
  _wifi_ap_localip = IPAddress(192,168,1,1);
  _wifi_ap_gateway = IPAddress(192,168,1,1);
  _wifi_ap_subnet = IPAddress(255,255,255,0);
}

void M1128::devConfig(const char* dev_id, const char* dev_user, const char* dev_pass) {
  _dev_id = dev_id;
  _dev_user = dev_user;
  _dev_pass = dev_pass;
}

void M1128::wifiConfig(char* st_ssid, char* st_pass) {
  _wifi_st_ssid = st_ssid;
  _wifi_st_pass = st_pass;
}

void M1128::wifiConfigAP(const char* ap_ssid, const char* ap_pass) {
  _wifi_ap_ssid = ap_ssid;
  _wifi_ap_pass = ap_pass;
}

void M1128::wifiConfigAP(const char* ap_ssid, const char* ap_pass, IPAddress localip, IPAddress gateway, IPAddress subnet) {
  _wifi_ap_ssid = ap_ssid;
  _wifi_ap_pass = ap_pass;
  _wifi_ap_localip = localip;
  _wifi_ap_gateway = gateway;
  _wifi_ap_subnet = subnet;
}

void M1128::init(PubSubClient &mqttClient, bool cleanSession) {
  _mqttClient = &mqttClient;
  _mqttCleanSession = cleanSession;
  _initNetwork();
}

void M1128::init(PubSubClient &mqttClient, bool cleanSession, Stream &serialDebug) {
  _serialDebug = &serialDebug;
  _debug = true;
  _serialDebug->flush();
  _mqttClient = &mqttClient;
  _mqttCleanSession = cleanSession;
  _initNetwork();
}

void M1128::loop() {
  _onWifiConfigChanged = false;
  _onReset = false;
  _onReconnect = false;
  
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    _wifi_ap_server.handleClient();    
  }
  else _mqttConnect();
  _checkResetButton();
}

const char* M1128::constructTopic(const char* topic) {
  strcpy(_topic_buf,"");
  strcat(_topic_buf,_dev_id);
  strcat(_topic_buf,PAYLOAD_DELIMITER);
  strcat(_topic_buf,myId());
  strcat(_topic_buf,PAYLOAD_DELIMITER);
  strcat(_topic_buf,topic);
  if (_debug) {
    _serialDebug->print(F("Construct topic: "));
    _serialDebug->println(_topic_buf);
  }
  return _topic_buf;
}

const char* M1128::myId() {
  return strlen(_custAddr)>0?_custAddr:_myAddr;
}

void M1128::setId(const char* id) {
  strcpy(_custAddr,id);
}

void M1128::reset() {
  _mqttClient->publish(MQTT::Publish(constructTopic("$state"), "disconnected").set_retain().set_qos(1));
  if (_debug) _serialDebug->println("Restoring to factory setting..");
  WiFi.disconnect(true);
  _onReset = true;
}

void M1128::restart() {
  _mqttClient->publish(MQTT::Publish(constructTopic("$state"), "disconnected").set_retain().set_qos(1));
  if (_debug) _serialDebug->println("Restarting..");
  ESP.restart();
}

bool M1128::onWiFiConfigChanged() {
  return _onWifiConfigChanged;  
}

bool M1128::onReset() {
  return _onReset;  
}

bool M1128::onReconnect() {
  return _onReconnect;  
}

void M1128::_initNetwork() {
  SPIFFS.begin();
  pinMode(pinReset, INPUT_PULLUP);
  _retrieveDeviceId();
  _checkResetButton();
  if (_wifiConnect()) {
    if (wifiClientSecure!=NULL) {
      if (SPIFFS.exists(MQTT_PATH_CA)) {
        File ca = SPIFFS.open(MQTT_PATH_CA, "r");
        if (ca && wifiClientSecure->loadCACert(ca)) if(_debug) _serialDebug->println(F("CA Certificate loaded..!"));
        else if(_debug) _serialDebug->println(F("CA Certificate load failed..!"));          
        ca.close();
      }
    }
    _mqttConnect();
    isReady = true;
  }
  else _wifiSoftAP();

  if(_debug) {
    if (isReady) _serialDebug->println(F("M1128 initialization succeed!"));
    else _serialDebug->println(F("M1128 initialization failed!"));
  }
}

bool M1128::_mqttConnect() {
  bool res = false;
  _onReconnect = false;
  if (!_mqttClient->connected()) {
    if (_debug) _serialDebug->println(F("Connecting to MQTT server"));
    MQTT::Connect con(myId());
    con.set_clean_session(_mqttCleanSession);
    con.set_will(constructTopic(MQTT_WILL_TOPIC), MQTT_WILL_VALUE, MQTT_WILL_QOS, MQTT_WILL_RETAIN);
    con.set_auth(_dev_user, _dev_pass);
    con.set_keepalive(MQTT_KEEPALIVE);
    if (_mqttClient->connect(con)) {
      if (_debug) _serialDebug->println(F("Connected to MQTT server"));
      res = true;
      _onReconnect = true;
    } else {
      if (_debug) _serialDebug->println(F("Could not connect to MQTT server"));   
    }
  }
  if (_mqttClient->connected()) _mqttClient->loop();  
  return res;
}

void M1128::_checkResetButton() {
  uint8_t pinResetButtonNow = digitalRead(pinReset);
  if (_pinResetButtonLast == HIGH && pinResetButtonNow == LOW) {
    reset();
  }
  _pinResetButtonLast = pinResetButtonNow;
}

bool M1128::_wifiConnect() {
  bool res = false;
  if (WiFi.status() != WL_CONNECTED && strlen(_wifi_st_ssid)>0 && strlen(_wifi_st_pass)>0) {
    if (_debug) {
      _serialDebug->print(F("Connecting to "));
      _serialDebug->print(_wifi_st_ssid);
      _serialDebug->println(F("..."));    
    }
    WiFi.mode(WIFI_STA);
    WiFi.begin(_wifi_st_ssid,_wifi_st_pass);
    if (WiFi.waitForConnectResult() == WL_CONNECTED) {
      res = true;
      if (_debug) {
        _serialDebug->println(F("WiFi connected"));
        _serialDebug->print(F("My Device Id: "));
        _serialDebug->println(myId());
        _serialDebug->print(F("Local IP Address: "));
        _serialDebug->println(WiFi.localIP());   
      }    
    }
  }
  return res;
}

bool M1128::_wifiSoftAP() {
  bool res = false;
  if (_debug) {
    _serialDebug->println(F("Setting up default Soft AP.."));        
    _serialDebug->print(F("SSID: "));
    _serialDebug->println(_wifi_ap_ssid);        
    _serialDebug->print(F("Password: "));
    _serialDebug->println(_wifi_ap_pass);        
  }    
  WiFi.mode(WIFI_AP);
  res = WiFi.softAPConfig(_wifi_ap_localip, _wifi_ap_gateway, _wifi_ap_subnet);
  if (res) res = WiFi.softAP(_wifi_ap_ssid,_wifi_ap_pass);
  if (res) {
    _wifi_ap_server.on("/", std::bind(&M1128::_handleWifiConfig, this));
    _wifi_ap_server.onNotFound(std::bind(&M1128::_handleNotFound, this));
    _wifi_ap_server.begin();
  }
  if (_debug) {
    if (res) {
      _serialDebug->println(F("Soft AP successfully setup!"));
      _serialDebug->print(F("Soft AP IP Address: "));
      _serialDebug->println(WiFi.softAPIP());   
    } else _serialDebug->println(F("Soft AP setup failed!"));
  }
  return res;
}

void M1128::_retrieveDeviceId() {
  String addr = String(ESP.getChipId());
  addr.toCharArray(_myAddr,32);
  _myAddr[32]='\0';
}

String M1128::_getContentType(String filename){
  if(filename.endsWith(".htm")) return "text/html";
  else if(filename.endsWith(".html")) return "text/html";
  else if(filename.endsWith(".css")) return "text/css";
  else if(filename.endsWith(".js")) return "application/javascript";
  else if(filename.endsWith(".png")) return "image/png";
  else if(filename.endsWith(".gif")) return "image/gif";
  else if(filename.endsWith(".jpg")) return "image/jpeg";
  else if(filename.endsWith(".ico")) return "image/x-icon";
  else if(filename.endsWith(".xml")) return "text/xml";
  else if(filename.endsWith(".pdf")) return "application/x-pdf";
  else if(filename.endsWith(".zip")) return "application/x-zip";
  else if(filename.endsWith(".gz")) return "application/x-gzip";
  return "text/plain";
}

void M1128::_handleWifiConfig() {
  if (_wifi_ap_server.hasArg("ssid") && _wifi_ap_server.hasArg("password")) {
    strcpy(_wifi_st_ssid,_wifi_ap_server.arg("ssid").c_str());
    strcpy(_wifi_st_pass,_wifi_ap_server.arg("password").c_str());
    if (_debug) {
      _serialDebug->println(F("New WIFI Configuration received:"));
      _serialDebug->print(F("SSID: "));
      _serialDebug->println(_wifi_st_ssid);        
      _serialDebug->print(F("Password: "));
      _serialDebug->println(_wifi_st_pass);        
    }
    _wifi_ap_server.send(200);
    _onWifiConfigChanged = true;
  } else _handleFileRead(_wifi_ap_server.uri());
}

void M1128::_handleNotFound() {
  if (!_handleFileRead(_wifi_ap_server.uri())) {
    _wifi_ap_server.send(404, "text/plain", "FileNotFound");
  }
}

bool M1128::_handleFileRead(String path) { // send the right file to the client (if it exists)
  if (path.endsWith("/")) path += "index.html";         // If a folder is requested, send the index file
  String contentType = _getContentType(path);            // Get the MIME type
  if (SPIFFS.exists(path)) {                            // If the file exists
    File file = SPIFFS.open(path, "r");                 // Open it
    _wifi_ap_server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  return false;                                         // If the file doesn't exist, return false
}
//END OF M1128
