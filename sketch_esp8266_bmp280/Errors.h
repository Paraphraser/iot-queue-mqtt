#pragma once

/*
 * 
 *  Error handling
 *  
 */


/*
 * Errors. These are distinct ordinals not bit positions in a mask.
 */
enum Sensor_Error {
    
  noError                     =  0,

  externalReboot              =  1,   // special case for ESP8266

  wiFiStartError              =  2,
  
  otaAuthError                =  3,   // otaAuthError
  otaBeginError               =  4,   // otaBeginError
  otaConnectError             =  5,   // otaConnectError
  otaReceiveError             =  6,   // otaReceiveError
  otaEndError                 =  7,   // otaEndError
  otaOtherError               =  8,   // otaOtherError

  connectMQTTError            =  9,   // mqttConnectDisconnectError (see 2 down)
  publishMQTTError            = 10,   // mqttTransmitError
  disconnectMQTTError         = 11,   // mqttConnectDisconnectError (see 2 up)

  queuePushError              = 12,
  queuePopError               = 13,

  sensorStartError            = 14,
  sensorMalfunctionError      = 15,

  rebootNoError               = 63    // internalError
    
};


const char * stringForError(Sensor_Error error) {
  
  switch (error) {
      
    case noError:                       return "none";

    case externalReboot:                return "external reboot - rebooting";

    case wiFiStartError:                return "wiFi start";
    
    case otaAuthError:                  return "ota auth";
    case otaBeginError:                 return "ota begin";
    case otaConnectError:               return "ota connect";
    case otaReceiveError:               return "ota receive";
    case otaEndError:                   return "ota end";
    case otaOtherError:                 return "ota other";
    
    case connectMQTTError:              return "connect MQTT";
    case publishMQTTError:              return "publish MQTT";
    case disconnectMQTTError:           return "disconnect MQTT";
    case queuePushError:                return "queue push";
    case queuePopError:                 return "queue pop";

    case sensorStartError:              return "sensor did not start";
    case sensorMalfunctionError:        return "sensor malfunction";

    case rebootNoError:                 return "normal reboot";

    default: break;

  }

  return "undocumented error";

}


void reboot () {

  // depends on D0 (GPIO16) being jumpered to RST 
  ESP.deepSleep(1E6);

}


void fatalError (
  Sensor_Error error,
  const char * caller
) {

  // turn on the serial port if no SerialDebugging
  #if (!SerialDebugging)
  Serial.begin(74880); while (!Serial); Serial.println();
  #endif

  // Serial port now guaranteed available
  Serial.printf(
    "\nfatalError(\"%s\",%s()) - rebooting\n",
    stringForError(error),
    caller
  );
  Serial.flush();
  delay(1000);

  // turn the on-board LED on
  pinMode(LED_BUILTIN,OUTPUT);

  // quick flashes to signal "something is wrong"
  for (int j = 0 ; j < 10 ; j++) {
      digitalWrite(LED_BUILTIN,HIGH);
      delay(50);
      digitalWrite(LED_BUILTIN,LOW);
      delay(50);
  }

  // and reboot
  reboot();

}
