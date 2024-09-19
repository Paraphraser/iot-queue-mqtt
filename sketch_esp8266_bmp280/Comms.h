#pragma once

/*
 * The states WiFi processing can be in
 */
typedef enum {

  WiFiSetupState,
  WiFiIdleState,
  WifiStartState,
  WiFiWaitConnectState,
  WiFiStartOTAState
    
} WiFi_State;

/*
 * The state this program is in at the moment is...
 */
WiFi_State wifiState = WiFiSetupState;

/*
 * whether OTA is firing on all thrusters
 */
bool isOTAServiceAvailable = false;

// timer
AsyncDelay wifi_startup_timer;
const unsigned long WiFi_startup_timeout_ms = 30*1000;


void setHostIDforDHCP (const char * hostid, bool force = false) {

  // presume the identifier is not yet set (this is a global)
  static bool isHostIdentifierSet = false;

  // sense no need for the hostID be set
  if ((isHostIdentifierSet) && (!force)) { return; }

  // sense no hostID available
  if (!hostid) { return; }

  // sense hostID is empty string
  if (strlen(hostid) == 0) { return; }

  // OK to set the DHCP HostID
  isHostIdentifierSet = WiFi.setHostname(hostid);

  #if (SerialDebugging)
  Serial.printf("DHCP HostID %sset to %s\n",(isHostIdentifierSet ? "" : "COULD NOT BE "),hostid);
  #endif
            
}


void do_wifiSetupState () {

  #if (SerialDebugging)
  Serial.printf("%s()\n",__func__);
  #endif

  // force known state
  WiFi.disconnect();
  WiFi.persistent(false);

  // go idle
  wifiState = WiFiIdleState;
    
}


void do_wifiStartState() {

  #if (SerialDebugging)
  Serial.printf("%s()\n",__func__);
  #endif

  // is WiFi already available?
  if (WiFi.status() == WL_CONNECTED) {

    // yes! set host ID
    setHostIDforDHCP(WIFI_DHCP_ClientID,false);

    // move straight to OTA
    wifiState = WiFiStartOTAState;

    return;
      
  }

  // WiFi not up - best presume OTA not available either
  isOTAServiceAvailable = false;

  #if (SerialDebugging)
  Serial.print("WiFi MAC Address = ");
  Serial.println(WiFi.macAddress());
  Serial.print("Connecting to ");
  Serial.println(WIFI_SSID);
  #endif

  /*
    * see https://github.com/esp8266/Arduino/issues/2186
    */
    
  // do not save any WiFi information
  WiFi.persistent(false);

  // off mode (in theory this is no longer needed)
  WiFi.mode(WIFI_OFF);

  // set connection mode (as a station)
  WiFi.mode(WIFI_STA);

  // start the connection process
  WiFi.begin(WIFI_SSID,WIFI_PSK);

  // initialise a wait timer
  wifi_startup_timer.start(WiFi_startup_timeout_ms, AsyncDelay::MILLIS);

  // wait for WiFi to come up
  wifiState = WiFiWaitConnectState;
  
  #if (SerialDebugging)
  Serial.println("moving to do_wifiWaitConnectState");
  #endif

}
    

void do_wifiWaitConnectState() {

  // has WiFi become available?
  if (WiFi.status() == WL_CONNECTED) {

    #if (SerialDebugging)
    Serial.print("WiFi connected using ");
    Serial.println(WiFi.localIP());
    #endif

    setHostIDforDHCP(WIFI_DHCP_ClientID,true);

    // yes! proceed to next phase
    wifiState = WiFiStartOTAState;

    return;
      
  }

  // not connected. Has the timeout expired?
  if (wifi_startup_timer.isExpired()) {

    #if (SerialDebugging)
    Serial.println("WiFi connection timeout expired");
    #endif
        
    // yes! nothing else we can do
    fatalError(wiFiStartError,__func__);
          
  }
    
}


void do_wifiStartOTAState() {
    
  #if (SerialDebugging)
  Serial.printf(
    "%s() as host %s on port %d\n",
    __func__,
    OTA_Host_Name,
    OTA_Host_Port
  );
  #endif

  // is the service already running?
  if (!isOTAServiceAvailable) {

    bool hasPassword = (strlen(OTA_Host_Password) > 0);

    #if (SerialDebugging)
    Serial.printf(
      "starting OTA on port %ld with service name %s%s%s\n",
      OTA_Host_Port,
      OTA_Host_Name,
      (hasPassword ? " and password " : ""),
      (hasPassword ? OTA_Host_Password : "")
    );
    #endif

    ArduinoOTA.setPort(OTA_Host_Port);
    ArduinoOTA.setHostname(OTA_Host_Name);
    if (hasPassword) {
      ArduinoOTA.setPassword(OTA_Host_Password);
    }
    
    // intercept callbacks
    ArduinoOTA.onError([](ota_error_t error) {
      if      (error == OTA_AUTH_ERROR)       fatalError(otaAuthError,__func__);
      else if (error == OTA_BEGIN_ERROR)      fatalError(otaBeginError,__func__);
      else if (error == OTA_CONNECT_ERROR)    fatalError(otaConnectError,__func__);
      else if (error == OTA_RECEIVE_ERROR)    fatalError(otaReceiveError,__func__);
      else if (error == OTA_END_ERROR)        fatalError(otaEndError,__func__);
      else                                    fatalError(otaOtherError,__func__);
    });

    // start OTA services - behaves as a no-op if called twice
    ArduinoOTA.begin();

    // declare ORA services now available
    isOTAServiceAvailable = true;

  }

  // move to next state
  wifiState = WiFiIdleState;

  #if (SerialDebugging)
  Serial.println("comms established, moving to WiFiIdleState");
  #endif

}


void wifi_handle() {

  // give WiFi some guaranteed time
  delay(1);

  // service OTA if it is running
  if (isOTAServiceAvailable) { ArduinoOTA.handle(); }

  // main event loop for comms
  switch (wifiState) {

    case WiFiSetupState:                do_wifiSetupState();                break;
    case WifiStartState:                do_wifiStartState();                break;
    case WiFiWaitConnectState:          do_wifiWaitConnectState();          break;
    case WiFiStartOTAState:             do_wifiStartOTAState();             break; 

    default: // WiFiIdleState

      // try to start WiFi if it is not up
      if (WiFi.status() != WL_CONNECTED) { wifiState = WifiStartState; }

  }

}
