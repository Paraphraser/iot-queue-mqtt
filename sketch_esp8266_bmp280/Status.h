#pragma once

/*
 * 
 *  Status reporting (board-level functions)
 *  
 */


// topic components
const char *    TopicStatusKey              = "status";

// payload components
const char *    PayloadStatusSSIDKey        = "\"ssid\"";
const char *    PayloadStatusMACKey         = "\"mac\"";
const char *    PayloadStatusIPKey          = "\"ip\"";
const char *    PayloadStatusHeapKey        = "\"heap\"";
const char *    PayloadStatusUpTimeKey      = "\"upTime\"";


AsyncDelay statusReportTimer;
const unsigned long statusReportTime_ms = 5*60*1000;


void publish_status_update(
  const char * wifi_ssid,
  const char * wifi_mac,
  const char * wifi_ip,
  uint32_t freeHeap,
  uint32_t upTime
) {
    
  // new queue entry
  Telemetry telemetry;

  // construct the topic
  sprintf(
    telemetry.topic,
    "%s/%s/%s",
    MQTTTopicPrefix,
    MQTTClientID,
    TopicStatusKey
  );

  // construct the payload
  sprintf(
    telemetry.payload,
    "{%s:\"%s\",%s:\"%s\",%s:\"%s\",%s:%lu,%s:%lu}",
    PayloadStatusSSIDKey,
    wifi_ssid,
    PayloadStatusMACKey,
    wifi_mac,
    PayloadStatusIPKey,
    wifi_ip,
    PayloadStatusHeapKey,
    freeHeap,
    PayloadStatusUpTimeKey,
    upTime
  );

  // push onto the queue and check the result
  try_to_enqueue(__func__,&telemetry);

}


void periodicStatusReport() {

  /*
    * on first entry: 
    * 1. comms will be up
    * 2. statusReportTimer.isExpired() will be true 
    */

  if (statusReportTimer.isExpired()) {

    // (re)start the timer
    statusReportTimer.start(statusReportTime_ms, AsyncDelay::MILLIS);

    // pre-calculations
    uint32_t freeHeap = system_get_free_heap_size();
    uint32_t upTime = millis() / 1000;

    // general status report
    publish_status_update(
      WiFi.SSID().c_str(),
      WiFi.macAddress().c_str(),
      WiFi.localIP().toString().c_str(),
      freeHeap,
      upTime
    );

  }
        
}
