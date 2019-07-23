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

void M1128::wifiConfig(const char* ap_ssid, const char* ap_pass) {
  _wifi_ap_ssid = ap_ssid;
  _wifi_ap_pass = ap_pass;
}

void M1128::wifiConfig(const char* ap_ssid, const char* ap_pass, IPAddress localip, IPAddress gateway, IPAddress subnet) {
  _wifi_ap_ssid = ap_ssid;
  _wifi_ap_pass = ap_pass;
  _wifi_ap_localip = localip;
  _wifi_ap_gateway = gateway;
  _wifi_ap_subnet = subnet;
}

void M1128::init(PubSubClient &mqttClient) {
  init(mqttClient,_mqttCleanSession,_mqttSetWill);
}

void M1128::init(PubSubClient &mqttClient, bool cleanSession) {
  init(mqttClient,cleanSession,_mqttSetWill);
}

void M1128::init(PubSubClient &mqttClient, bool cleanSession, bool setWill) {
  init(mqttClient,cleanSession,setWill,NULL);
}

void M1128::init(PubSubClient &mqttClient, bool cleanSession, bool setWill, Stream *serialDebug) {
  SPIFFS.begin();
  pinMode(pinReset, INPUT_PULLUP);  
  if (serialDebug) {
    _serialDebug = serialDebug;
    _serialDebug->flush();
  }
  _mqttClient = &mqttClient;
  _mqttCleanSession = cleanSession;
  _mqttSetWill = setWill;
  if (!_checkResetButton()) _initNetwork(false);
  if (_serialDebug) _serialDebug->println(F("End of initialization, ready for loop.."));
}

void M1128::loop() {
  if (WiFi.status() != WL_CONNECTED) {
    WiFi.reconnect();
    _wifi_ap_server.handleClient();    
  } else {
    _softAPStartMillis = 0;
    _wifiFailStartMillis = 0;
    _mqttConnect();
  }
  
  _checkResetButton();
  
  if (_softAPStartMillis>0 && apConfigTimeout > 0) { // if WiFi in SoftAP mode
    _softAPCurrentMillis = millis();
    if ((_softAPCurrentMillis - _softAPStartMillis) > apConfigTimeout) {   
      _softAPStartMillis = 0;
      if (_serialDebug) _serialDebug->println(F("Exceeded apConfigTimeout.."));
      if (onAPConfigTimeout!=NULL) {
        if (_serialDebug) _serialDebug->println(F("Triggering onAPConfigTimeout().."));
        onAPConfigTimeout();
      } else {
        if (_serialDebug) _serialDebug->println(F("Going to deep sleep.."));
        ESP.deepSleep(0);
      }
    }
  } else if (WiFi.status() != WL_CONNECTED && wifiConnectTimeout > 0) { // if not in AP Mode (normal mode) but fail to connect to wifi and there is a fail timeout
    _wifiFailCurrentMillis = millis();
    if ((_wifiFailCurrentMillis - _wifiFailStartMillis) > wifiConnectTimeout) {
      _wifiFailStartMillis = 0;
      if (_serialDebug) _serialDebug->println(F("Exceeded wifiConnectTimeout.."));
      if (onWiFiConnectTimeout!=NULL) {
        if (_serialDebug) _serialDebug->println(F("Triggering onWiFiConnectTimeout().."));
        onWiFiConnectTimeout();
      } else {
        if (_serialDebug) _serialDebug->println(F("Going to deep sleep.."));
        ESP.deepSleep(0);
      }
    }
  }
}

const char* M1128::constructTopic(const char* topic) {
  strcpy(_topic_buf,"");
  strcat(_topic_buf,_dev_id);
  strcat(_topic_buf,PAYLOAD_DELIMITER);
  strcat(_topic_buf,myId());
  strcat(_topic_buf,PAYLOAD_DELIMITER);
  strcat(_topic_buf,topic);
  if (_serialDebug) {
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
  if (_serialDebug) _serialDebug->println(F("Restoring to factory setting.."));
  WiFi.disconnect(true);
  delay(1000);
  _initNetwork(true);
  if (onReset!=NULL) onReset();
}

void M1128::restart() {
  _mqttClient->publish(MQTT::Publish(constructTopic("$state"), "disconnected").set_retain().set_qos(1));
  if (_serialDebug) _serialDebug->println("Restarting..");
  delay(1000);
  ESP.restart();
}

void M1128::_initNetwork(bool goAP) {
  _retrieveDeviceId();
  if (_wifiConnect()) {
    if (wifiClientSecure!=NULL) {
      if (SPIFFS.exists(MQTT_PATH_CA)) {
        File ca = SPIFFS.open(MQTT_PATH_CA, "r");
        if (ca && wifiClientSecure->loadCACert(ca)) { if (_serialDebug) _serialDebug->println(F("CA Certificate loaded..!")); }
        else if (_serialDebug) _serialDebug->println(F("CA Certificate load failed..!"));          
        ca.close();
      }
    }
    if (_serialDebug) _serialDebug->println(F("M1128 initialization succeed!"));
    _mqttConnect();    
  } else {
    if (_wifiConnectRetryVal < wifiConnectRetry-1) {
      delay(3000);
      if (_serialDebug) _serialDebug->println(F("Trying to connect again..."));
      _wifiConnectRetryVal++;
      _initNetwork(goAP);
    } else {
      _wifiConnectRetryVal = 0;
      if (_serialDebug) _serialDebug->println(F("M1128 initialization failed!"));      
      if (autoAP || goAP) {
        if (autoAP && _serialDebug) _serialDebug->println(F("autoAP is enable, I will go to AP now..!"));      
        if (goAP && _serialDebug) _serialDebug->println(F("Factory reset pressed, I will go to AP now..!"));      
        _wifiSoftAP();
      }
    }
  }
}

bool M1128::_checkResetButton() {
  bool res = false;
  uint8_t pinResetButtonNow = digitalRead(pinReset);
  if (_pinResetButtonLast == HIGH && pinResetButtonNow == LOW) {
    reset();
    res = true;
  }
  _pinResetButtonLast = pinResetButtonNow;
  return res;
}

bool M1128::_wifiConnect() {
  return _wifiConnect(NULL,NULL);
}
  
bool M1128::_wifiConnect(const char* ssid, const char* password) {
  bool res = false;
  WiFi.mode(WIFI_STA);
  if (ssid!=NULL) {
    _startWiFi = true;
    if (_serialDebug) {
      _serialDebug->println(F("Connecting to WiFi setting:"));
      _serialDebug->print(F("SSID: "));
      _serialDebug->println(ssid);
      _serialDebug->print(F("Password: "));
      _serialDebug->println(password);
    }
    if (password!=NULL && strlen(password)==0) password=NULL;
    if (WiFi.status() == WL_CONNECTED) WiFi.disconnect(true);
    WiFi.begin(ssid,password);
    if (WiFi.waitForConnectResult() == WL_CONNECTED) res = true;
  } else if (WiFi.status() != WL_CONNECTED) {
    _startWiFi = true;
    if (_serialDebug) _serialDebug->println(F("Connecting to the last WiFi setting..."));
    WiFi.begin();
    if (WiFi.waitForConnectResult() == WL_CONNECTED) res=true;
  } else if (WiFi.status() == WL_CONNECTED) {
    res = true;
  }

  if (_serialDebug) {
    _serialDebug->print(F("My Device Id: "));
    _serialDebug->println(myId());
    if (res) {
      _serialDebug->println(F("WiFi connected"));
      _serialDebug->print(F("Local IP Address: "));
      _serialDebug->println(WiFi.localIP());    
    } else {
      _serialDebug->println(F("WiFi connect failed..!"));      
    }
  }

  isReady = res;
  return res;
}

bool M1128::_wifiSoftAP() {
  bool res = false;
  if (_serialDebug) {
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
  if (_serialDebug) {
    if (res) {
      _serialDebug->println(F("Soft AP successfully setup!"));
      _serialDebug->print(F("Soft AP IP Address: "));
      _serialDebug->println(WiFi.softAPIP());   
    } else _serialDebug->println(F("Soft AP setup failed!"));
  }
  if (res) _softAPStartMillis = millis();
  return res;
}

bool M1128::_mqttConnect() {
  bool res = false;
  if (!_mqttClient->connected()) {
    if (_serialDebug) _serialDebug->println(F("Connecting to MQTT server"));
    MQTT::Connect con(myId());
    con.set_clean_session(_mqttCleanSession);
    con.set_auth(_dev_user, _dev_pass);
    con.set_keepalive(MQTT_KEEPALIVE);
    if (_mqttSetWill) con.set_will(constructTopic(MQTT_WILL_TOPIC), MQTT_WILL_VALUE, MQTT_WILL_QOS, MQTT_WILL_RETAIN);
    if (_mqttClient->connect(con)) {
      if (_serialDebug) _serialDebug->println(F("Connected to MQTT server"));
      if (_startWiFi) {
        if (onConnect!=NULL) onConnect(); 
        _startWiFi = false;
      } else if (onReconnect!=NULL) onReconnect();     
    } else {
      if (_serialDebug) _serialDebug->println(F("Could not connect to MQTT server"));   
    }
  }
  if (_mqttClient->connected()) {
    _mqttClient->loop();  
    res = true; 
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
    if (_serialDebug) {
      _serialDebug->println(F("New WIFI Configuration received:"));
      _serialDebug->print(F("SSID: "));
      _serialDebug->println(_wifi_ap_server.arg("ssid").c_str());
      _serialDebug->print(F("Password: "));
      _serialDebug->println(_wifi_ap_server.arg("password").c_str());
    }
    _wifi_ap_server.send(200);
    WiFi.softAPdisconnect(true);
    _softAPStartMillis = 0;
    if (_wifiConnect(_wifi_ap_server.arg("ssid").c_str(),_wifi_ap_server.arg("password").c_str())) _mqttConnect();
    if (onWiFiConfigChanged!=NULL) onWiFiConfigChanged();
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
