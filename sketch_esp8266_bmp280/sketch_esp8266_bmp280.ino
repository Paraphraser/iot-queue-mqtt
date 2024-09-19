#include "Defines.h"

/*
 *  Sketch to periodically report pressure and temperature to an MQTT service.
 *  
 *      LOLIN(WEMOS) D1 R2 & mini or WeMos D1 R3 ESP12
 *      
 *      #if (ESP8266)
 *      #if (ARDUINO_ESP8266_WEMOS_D1MINI)
 *      #if (ARDUINO_ARCH_ESP8266)
 *      
 *  Setup:
 *  
 *      Tools > Board > [ESP8266 Boards] LOLIN (WEMOS) D1 R2 & mini
 *      
 *      plus, for reliability:
 *      
 *          Tools > Upload Speed "115200"
 *          
 *      Programming the mini MAY OCCASIONALLY require:
 *      
 *      1. Hold down the reset button.
 *      2. Start compiling the code for upload.
 *      3. Release the reset button.
 * 
 *  WeMosD1R2   GPIO        Shield
 *      D0       16         jumpered to RST (timer wakeup)      
 *      D1        5         I²C SCL
 *      D2        4         I²C SDA
 *      D3        0         KEY_BUILTIN
 *      D4        2         LED_BUILTIN
 *      D8       15         .
 *      TX        1         .
 *      RX        3         .
 *      A0                  (analog INPUT)
 *      
 *      3V3                 to BOTH the VCC and SDO pins
 *      GND                 to the GND pin
 *      RST                 jumpered to D0/GPIO16 (timer wakeup)
 *  
 */

#if (!ESP8266)
   #error "Sketch is designed for ESP8266"
#endif


void setup() {

  #if (SerialDebugging)
  Serial.begin(74880); while (!Serial); Serial.println();
  #endif

  // were we just reset by the IDE?
  if (ESP.getResetInfoPtr()->reason == REASON_EXT_SYS_RST) {

    #if (SerialDebugging)
    Serial.println("Reset Info == REASON_EXT_SYS_RST so immediate reboot");
    #endif

    // perform a timed sleep/wake to force board into known state
    reboot();

  }

  // we want the builtin LED set to minimal glow
  digitalWrite(LED_BUILTIN,LOW);
  pinMode(LED_BUILTIN,INPUT);

}


void periodicRestartCheck(unsigned days) {

    // remember, the millis() clock is limited to 49.7 days
    if (millis() > (days * 86400 * 1000)) {

        // yes! probably time to clear out the cobwebs then
        reboot();
        
    }

}


void loop() {
    
  // start & maintain WiFi and OTA services
  wifi_handle();

  // is WiFi up?
  if (WiFi.status() == WL_CONNECTED) {

      // bung out a status report
      periodicStatusReport();

      // handle any sensors
      sensor_handle();

      // mqtt to handle any queued telemetry
      mqtt_handle();

  }

  // an occasional clearing of the cobwebs (days)
  periodicRestartCheck(30);

  // give WiFi some guaranteed time
  delay(1);
    
}
