#pragma once

#include <Arduino.h>
#include <AsyncDelay.h>
#include <ESP8266WiFi.h>
#include <ArduinoOTA.h>
#include <MQTT.h>
#include <cppQueue.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BMP280.h>

/*
 * All writes to the serial port are gated through SerialDebugging.
 * If set false, the serial port will not be enabled and nothing
 * will be written to it. However, if SerialDebugging=false AND
 * the sketch calls fatalError() then the serial port is enabled
 * and a short error message is written. If you notice that the
 * ESP8266 seems to be rebooting (on-board LED blinking) you can
 * connect to the serial port to see the error condition.
 */
#define SerialDebugging true

/*
 * Connection definition for WiFi:
 * 
 * - WIFI_SSID - replace with your WiFi network name
 * - WIFI_PSK - replace with your WiFi pre-shared key (password)
 * - WIFI_DHCP_ClientID - how the ESP will announce itself to DHCP.
 *                      - this is also the default for OTA_Host_Name
 *                        and MQTTClientID (both below)
 */
const char *    WIFI_SSID                   = "YourWiFiNetwork";
const char *    WIFI_PSK                    = "YourWiFiPassword";
const char *    WIFI_DHCP_ClientID          = "sketch";

/*
 * Connection definition for Over The Air updating:
 * 
 * - OTA_Host_Name - the name this ESP uses to advertise itself to
 *   OTA clients (eg the Arduino IDE)
 * - OTA_Host_Password - set to a non-null value if you want to protect
 *   this ESP from unauthorised over-the-air updates.
 * - OTA_Host_Port - the port this ESP listens on for OTA messages.
 */
const char *    OTA_Host_Name               = WIFI_DHCP_ClientID;
const char *    OTA_Host_Password           = "";
const uint16_t  OTA_Host_Port               = 8266;

/*
 * Connection definition for MQTT:
 * 
 * - MQTTHostFQDN_or_IP the fully-qualified domain name or IP address 
 *   of the host running your MQTT broker. Examples:
 *   
 *      192.168.100.50
 *      iot-hub.local
 *      iot-hub.mydomain.com
 *      
 * - MQTTHostPort the port number the MQTT broker is listening on.
 *   Usually, this is 1883.
 *   
 * - MQTTTopicPrefix
 * - MQTTClientID
 * 
 *   All MQTT topics will commence with the strings defined by those
 *   two variables:
 *   
 *      «MQTTTopicPrefix»/«MQTTClientID»
 */
const char *    MQTTHostFQDN_or_IP          = "raspberrypi.local";
const uint16_t  MQTTHostPort                = 1883;
const char *    MQTTTopicPrefix             = "home";
const char *    MQTTClientID                = WIFI_DHCP_ClientID;

/*
* Your altitude in meters above sea level.
* You need to determine this value yourself.
*/
const float LocalHeightAboveSeaLevelInMetres = 338;

// Assumption is that this is a D1 R3 with a button on GPIO0/D3
#if (ARDUINO_ESP8266_WEMOS_D1MINI)
  #ifndef KEY_BUILTIN
    #define KEY_BUILTIN D3
  #endif
#endif


#include "Errors.h"
#include "Comms.h"
#include "Telemetry.h"
#include "Sensor.h"
#include "Status.h"

