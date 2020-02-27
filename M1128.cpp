/*
  M1128.cpp - WiFi Connectivity SAM Element IoT Platform.
  SAM Element
  https://samelement.com
*/

#include "M1128.h"


#pragma GCC diagnostic push
#pragma GCC diagnostic ignored  "-Wdeprecated-declarations"
WiFiClientSecure _wifiClient;
#pragma GCC diagnostic pop

#if defined(ESP8266)
ESP8266WebServer _wifi_ap_server(80);
#elif defined(ESP32)
WebServer _wifi_ap_server(80);
#endif

PubSubClient _mqttClient(_wifiClient);
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

void M1128::init() {
  init(NULL);
}

void M1128::init(Stream *serialDebug) {
  SPIFFS.begin();
  pinMode(pinReset, INPUT_PULLUP);  
  if (serialDebug) {
    _serialDebug = serialDebug;
    _serialDebug->flush();
  } 
 
  if (prod) {
    _authHost = AUTH_SERVER_HOST;
    _mqttHost = MQTT_BROKER_HOST;
  } else {
    _authHost = AUTH_SERVER_HOST_SBX;
    _mqttHost = MQTT_BROKER_HOST_SBX;
  }
  
  _mqttClient.setServer(_mqttHost,MQTT_BROKER_PORT);
  using namespace std::placeholders; 
  _mqttClient.setCallback(std::bind(&M1128::_handleOnReceive, this, _1, _2, _3));
  mqtt = &_mqttClient;
  
  if (!_checkResetButton()) _initNetwork(false);
  configTime(0, 0, "pool.ntp.org", "time.nist.gov"); //get real time from server format: configTime(timezone,dst,server1,server2)
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
  strcat(_topic_buf,MQTT_TOPIC_DELIMITER);
  strcat(_topic_buf,myId());
  strcat(_topic_buf,MQTT_TOPIC_DELIMITER);
  strcat(_topic_buf,topic);
  if (_serialDebug) {
    _serialDebug->print(F("Construct topic: "));
    _serialDebug->println(_topic_buf);
  }
  return _topic_buf;
}

const char* M1128::_constructTopicModel(const char* topic) {
  strcpy(_topic_buf,"");
  strcat(_topic_buf,_dev_id);
  strcat(_topic_buf,MQTT_TOPIC_DELIMITER);
  strcat(_topic_buf,model);
  strcat(_topic_buf,MQTT_TOPIC_DELIMITER);
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
  _mqttClient.publish(constructTopic("$state"), "disconnected", true);
  if (_serialDebug) _serialDebug->println(F("Restoring to factory setting.."));
  #if defined(ESP32)
    // some ESP32 didn't erased after disconnect(true), this will fix it.
    //esp_wifi_restore();
    WiFi.disconnect(true);
    WiFi.begin("0","0");    
  #endif
  WiFi.disconnect(true);
  delay(1000);
  _initNetwork(true);
  if (onReset!=NULL) onReset();
}

void M1128::restart() {
  _mqttClient.publish(constructTopic("$state"), "disconnected", true);
  if (_serialDebug) _serialDebug->println("Restarting..");
  delay(1000);
  ESP.restart();
}

bool M1128::refreshAuth() {
  if (_serialDebug) _serialDebug->println(F("Connect to auth server to get token: "));
  
  #if defined(ESP8266)
  
    if (!_wifiClient.connect(_authHost, AUTH_SERVER_PORT)) {
      if (_serialDebug) _serialDebug->println("Connect failed.");
      return false;
    }
    memset(_tokenAccess, 0, sizeof(_tokenAccess));
    memset(_tokenRefresh, 0, sizeof(_tokenRefresh));
    char tmp[strlen(_dev_user)+strlen(_dev_pass)+2];
    strcpy(tmp,_dev_user);
    strcat(tmp,":");
    strcat(tmp,_dev_pass);
   
    if (_serialDebug) _serialDebug->println(F("Connected to auth server. Getting token.."));
  
    _wifiClient.setTimeout(AUTH_TIMEOUT);
    _wifiClient.println(String("GET ") + AUTH_SERVER_PATH + " HTTP/1.1\r\n" +
      "Host: " + (_authHost) + "\r\n" +
      "Authorization: Basic " + base64::encode(tmp,false) + "\r\n" +
      "cache-control: no-cache\r\n" + 
      "Connection: close\r\n\r\n");
    
    _wifiClient.find("\r\n\r\n");
    char *__token = _tokenAccess;
    uint16_t __i = 0;
    while (_wifiClient.connected() || _wifiClient.available()) {
      ESP.wdtFeed();
      char c = _wifiClient.read();
      if (c=='|') {
        __token[__i] = '\0';
        __token = _tokenRefresh;
        __i = 0;
      } else {
        __token[__i]=c;
        __i++;
      }
    }
    if (__i>0) __token[__i-1] = '\0';

  #elif defined(ESP32)
  
    memset(_tokenAccess, 0, sizeof(_tokenAccess));
    memset(_tokenRefresh, 0, sizeof(_tokenRefresh));
    HTTPClient https;
    if (https.begin(_wifiClient, "https://" + String(_authHost) + AUTH_SERVER_PATH)) {
      
      if (_serialDebug) _serialDebug->println(F("Connected to auth server. Getting token.."));
      
      https.setAuthorization(_dev_user,_dev_pass);
      int httpCode = https.GET();    
      // httpCode will be negative on error
      if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY) {
          String payload = https.getString();
          uint16_t __i = payload.indexOf('|');
          strcpy(_tokenAccess,payload.substring(0,__i-1).c_str());
          strcpy(_tokenRefresh,payload.substring(__i+1,payload.length()-1).c_str());
      } else if (_serialDebug) _serialDebug->printf("Connect failed with error: %s\n", https.errorToString(httpCode).c_str());
    
      https.end();
    } else if (_serialDebug) _serialDebug->println("Connect failed.");
  
  #endif  

  if (strlen(_tokenAccess)>0 && strlen(_tokenRefresh)>0) _timeStartMillis = millis();
  if (_serialDebug) _serialDebug->println(F("Leaving auth server."));
  return true;
}

void M1128::_initNetwork(bool goAP) {
  _retrieveDeviceId();
  if (_wifiConnect()) {
    if (SPIFFS.exists(PATH_CA)) {
      File ca = SPIFFS.open(PATH_CA, "r");
      #if defined(ESP8266)
        if (ca && _wifiClient.loadCACert(ca)) { if (_serialDebug) _serialDebug->println(F("CA Certificate loaded..!")); }
        else if (_serialDebug) _serialDebug->println(F("CA Certificate load failed..!"));
      #elif defined(ESP32)
        if (ca && _wifiClient.loadCACert(ca, ca.size())) { if (_serialDebug) _serialDebug->println(F("CA Certificate loaded..!")); }
        else if (_serialDebug) _serialDebug->println(F("CA Certificate load failed..!"));
      #endif
      ca.close();
    } else if (_serialDebug) _serialDebug->println(F("CA Certificate is not available."));  
    if (_serialDebug) _serialDebug->println(F("M1128 initialization succeed!"));
    refreshAuth();
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
  _timeCurrentMillis = millis();
  int64_t tframe = _timeCurrentMillis - _timeStartMillis;
  if (tframe > accessTokenExp*1000 || tframe < 0 || _timeStartMillis==0) refreshAuth();
  
  if (!_mqttClient.connected()) {
    if (_serialDebug) _serialDebug->println(F("Connecting to MQTT server"));

    bool res = false;
    if (setWill) res = _mqttClient.connect(myId(),_tokenAccess, MQTT_PWD, constructTopic(MQTT_WILL_TOPIC), MQTT_WILL_QOS, MQTT_WILL_RETAIN, MQTT_WILL_VALUE, cleanSession);
    else res = _mqttClient.connect(myId(),_tokenAccess, MQTT_PWD);    
    
    if (res) {
      if (_serialDebug) _serialDebug->println(F("Connected to MQTT server"));
      if (_startWiFi) {
		if (onConnect!=NULL) {
		  _initPublish();
		  onConnect(); 
		  _initSubscribe();
		}
		_startWiFi = false;
      } 
	  else if (onReconnect!=NULL) {
		onReconnect();     
		_initSubscribe();
	  }
      if (mqtt->connected()) _mqttClient.publish(constructTopic("$state"), "ready", true);
    } else {
      if (_serialDebug) _serialDebug->println(F("Could not connect to MQTT server"));   
    }
  } else  {
    _mqttClient.loop();    
    res = true; 
  }
  return res;
}

void M1128::_retrieveDeviceId() {
  String addr = String(_dev_id) + "S";
  #if defined(ESP8266)
    addr = addr + String(ESP.getChipId());
  #elif defined(ESP32)
    addr = addr + String((uint32_t)(ESP.getEfuseMac() >> 32));
  #endif
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

void M1128::_initPublish(){
  char sizeNow[3];
  dtostrf(floor(ESP.getSketchSize()/1000)+1, 3, 0, sizeNow);
  if (mqtt->connected()) {
    _mqttClient.publish(constructTopic("$state"), "init", false);
    _mqttClient.publish(constructTopic("$sammy"), sammy, false);
    _mqttClient.publish(constructTopic("$name"), name, false);
    _mqttClient.publish(constructTopic("$model"), model, false);
    _mqttClient.publish(constructTopic("$mac"), WiFi.macAddress().c_str(), false);
    _mqttClient.publish(constructTopic("$localip"), WiFi.localIP().toString().c_str(), false);
    _mqttClient.publish(constructTopic("$fw/name"), fw.name, false);
    _mqttClient.publish(constructTopic("$fw/version"), fw.version, false);
    _mqttClient.publish(constructTopic("$fw/size"), sizeNow, false);
    EEPROM.begin(512);
    int add = 0;
    unsigned char k;
    k=EEPROM.read(add);
    if (k == 'm' || k == 'a'){
      _mode[0]=k;
      add++;
      while(k != '\0' && add<500)   //Read until null character
      {    
        k=EEPROM.read(add);
        _mode[add]=k;
        add++;
      }
      _mode[add]='\0';
    }
    _mqttClient.publish(constructTopic("$fw/update/mode"), _mode, true);
  }
}

void M1128::_handleOnReceive(char* topic, uint8_t* payload, unsigned int length) {
  String strPayload;
  strPayload.reserve(length);
  for (uint32_t i = 0; i < length; i++) strPayload += (char)payload[i];
  if (strcmp(topic,constructTopic("$fw/update/mode/set"))==0 && strPayload) {
    strPayload.toCharArray(_mode,7);
    _mqttClient.publish(constructTopic("$fw/update/mode"), _mode, true);
    EEPROM.begin(512);
    for(int i=0;i<8;i++){
      EEPROM.write(i,_mode[i]);
    }
    EEPROM.commit();
  }
  else if (strcmp(topic,constructTopic("$fw/update/trigger"))==0 && strPayload) _triggerUpdate(strPayload);
  else if (strcmp(topic,_constructTopicModel("$fw/updates"))==0 && strPayload) _updateCollectionPublish(strPayload);
  else if (strcmp(topic,constructTopic("$fw/update/state"))==0 && strPayload.substring(0,strPayload.indexOf(";",0)).indexOf("updating",0) >= 0) _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("completed",fw.version, _dateTime()),true);
  else if (onReceive!=NULL) onReceive(topic, payload, length);
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
    if (_wifiConnect(_wifi_ap_server.arg("ssid").c_str(),_wifi_ap_server.arg("password").c_str())) {
      _timeStartMillis = 0;
      _mqttConnect();
    }
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
    _wifi_ap_server.sendHeader("Content-Length", (String)(file.size()));
    _wifi_ap_server.sendHeader("Cache-Control", "max-age=2628000, public"); // cache for 30 days
    _wifi_ap_server.streamFile(file, contentType); // And send it to the client
    file.close();                                       // Then close the file again
    return true;
  }
  return false;                                         // If the file doesn't exist, return false
}

void M1128::_initSubscribe(){
  if (mqtt->connected()) {
    _mqttClient.subscribe(constructTopic("$fw/update/mode/set"),1);
    _mqttClient.subscribe(constructTopic("$fw/update/trigger"),1);
    _mqttClient.subscribe(_constructTopicModel("$fw/updates"),1);
  }
}

void M1128::_updateCollectionPublish(String payload){
  char newVersion[15];
  int oldIndex = payload.indexOf(";",0) + 1;
  int newIndex = payload.indexOf(";",oldIndex);
  strcpy(newVersion,payload.substring(oldIndex,newIndex).c_str());
  if (strcmp(newVersion,fw.version)!=0){
    if (strcmp(_mode,"manual")==0){
      _mqttClient.publish(constructTopic("$fw/update/available"), "true", true);
      bool nextCollection = true;
      String publishUpdate = "";
      _updateCollection = "";
      oldIndex = 0;
      while (nextCollection) {
        for (int indexProperties = 0;indexProperties<=3;indexProperties++){ //collection properties => (0)name;(1)version;(2)size;(3)url;md5
          newIndex = payload.indexOf(";",oldIndex);
          //payload for publish update
          if (indexProperties <= 2) {
            publishUpdate += payload.substring(oldIndex,newIndex); 
            publishUpdate += ";";
          }
          if (indexProperties == 2) {
            if (ESP.getFreeSketchSpace() >= payload.substring(oldIndex,newIndex).toInt()*1000) publishUpdate += "true";
            else if (ESP.getFreeSketchSpace() < payload.substring(oldIndex,newIndex).toInt()*1000) publishUpdate += "false";
          }
          //save version;size;url in a string
          if (indexProperties > 0) {
            _updateCollection += payload.substring(oldIndex,newIndex);
            if (indexProperties != 3) _updateCollection += ";";
          }
          oldIndex = newIndex + 1;
        }
        //delimiter of a collection
        if (payload.indexOf("|",oldIndex) > 0) {
          oldIndex = payload.indexOf("|",oldIndex) + 1;
          publishUpdate += "|";
          _updateCollection += "|";
        }
        else nextCollection = false;
      }
      _mqttClient.publish(constructTopic("$fw/updates"), publishUpdate.c_str(), true);
    }
    else if (strcmp(_mode,"auto")==0){
      oldIndex = payload.indexOf(";",0)+1;
      String newVersion = "";
      String urlUpdate = "";
      int errorStatus = 2;
      for (int indexProperties = 1; indexProperties <= 3; indexProperties++){
        newIndex = payload.indexOf(";",oldIndex);
        if (indexProperties == 1) _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("starting",payload.substring(oldIndex,newIndex).c_str(), _dateTime()),true);
        else if (indexProperties == 2) {
          if (ESP.getFreeSketchSpace() >= payload.substring(oldIndex,newIndex).toInt()*1000) errorStatus = 0;
          else if (ESP.getFreeSketchSpace() < payload.substring(oldIndex,newIndex).toInt()*1000) errorStatus = 1;
        }
        else if (indexProperties == 3) urlUpdate = payload.substring(oldIndex,newIndex);
        oldIndex = newIndex + 1;
      }
      _updateExecution(errorStatus,urlUpdate);
    }
  }
  else if (strcmp(_mode,"manual")==0) _mqttClient.publish(constructTopic("$fw/update/available"), "false", true);
}

const char* M1128::_dateTime() {
  if (WiFi.status() == WL_CONNECTED) {
    int year = 0;
    time_t now = time(nullptr);
    struct tm * breaktime = gmtime(&now);
    year = breaktime->tm_year;
    if (year>100){
      strcpy(_date,String(year+1900).c_str());
      strcat(_date,"-");
      _twoDigit(breaktime->tm_mon+1);
      strcat(_date,"-");
      _twoDigit(breaktime->tm_mday);
      strcat(_date," ");
      _twoDigit(breaktime->tm_hour);
      strcat(_date,":");
      _twoDigit(breaktime->tm_min);
      strcat(_date,":");
      _twoDigit(breaktime->tm_sec);
      return _date;
    }
  }
}

void M1128::_twoDigit(int num){
  int firstDigit = floor(num/10);
  int secondDigit = num % 10;
  strcat(_date,String(firstDigit).c_str());
  strcat(_date,String(secondDigit).c_str());
}

const char* M1128::_constructUpdateState(const char* status, const char* message, const char* date){
  #if defined(ESP8266)
    char updateState[500];
	strcpy(updateState,status);
    strcat(updateState,";");
    strcat(updateState,message);
    strcat(updateState,";");
    strcat(updateState,date);
    return updateState;
  #elif defined(ESP32)
    String updateState2 = String(status)+";"+String(message)+";"+String(date);
    return updateState2.c_str();
  #endif
  
}

void M1128::_triggerUpdate(String version){
  int errorStatus = 2;
  String urlUpdate = "";
  if (_updateCollection.length()>0) {
    int oldIndex = _updateCollection.indexOf(version,0);
	int newIndex = 0;
    if (oldIndex >= 0){
      _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("starting",version.c_str(), _dateTime()),true);
      for (int indexProperties = 0;indexProperties <= 2;indexProperties++){
        if (indexProperties < 2) newIndex = _updateCollection.indexOf(";",oldIndex+1);
	    else if (indexProperties == 2) newIndex = _updateCollection.indexOf("|",oldIndex);
        if (indexProperties == 0) {
          if (ESP.getFreeSketchSpace() >= _updateCollection.substring(oldIndex+1,newIndex).toInt()*1000) errorStatus = 0;
          else if (ESP.getFreeSketchSpace() < _updateCollection.substring(oldIndex+1,newIndex).toInt()*1000) errorStatus = 1;
        }
        else if (indexProperties == 2) {
          if (newIndex >= 0) urlUpdate = _updateCollection.substring(oldIndex+1,newIndex);
		  else urlUpdate = _updateCollection.substring(oldIndex+1);
	    }
        oldIndex = newIndex;
      }
	  _updateExecution(errorStatus,urlUpdate);
    }
  }
  else _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("failed","Unknown error update, please restart this device to try updating again", _dateTime()),true);
}

void M1128::_updateExecution(int errorStatus,String urlUpdate){
  if (errorStatus == 2) _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("failed","Unknown error update, please restart this device to try updating again", _dateTime()),true);
  else if (errorStatus == 1) _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("failed","This device doesn't have enough memory to update to the latest version", _dateTime()),true);
  else if (errorStatus == 0) {
    String errorString = "";
    _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("updating",urlUpdate.c_str(), _dateTime()),true);
    #if defined(ESP8266)
      t_httpUpdate_return ret = ESPhttpUpdate.update(urlUpdate);
    #elif defined(ESP32)
	  WiFiClient client;
      t_httpUpdate_return ret = httpUpdate.update(client,urlUpdate);
    #endif
    switch (ret) {
      case HTTP_UPDATE_FAILED:
	    #if defined(ESP8266)
          errorString += String(ESPhttpUpdate.getLastError());
          errorString += ESPhttpUpdate.getLastErrorString();
		#elif defined(ESP32)
		  errorString += String(httpUpdate.getLastError());
          errorString += httpUpdate.getLastErrorString();
		#endif
        _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("failed",errorString.c_str(), _dateTime()),true);
        break;
      
      case HTTP_UPDATE_NO_UPDATES:
        _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("failed","File not found. Please contact the device's administrator", _dateTime()),true);
        break;
      
      case HTTP_UPDATE_OK:
        _mqttClient.publish(constructTopic("$fw/update/state"),_constructUpdateState("completed","Device is up to date", _dateTime()),true);
        break;
    }
  }
}
//END OF M1128
