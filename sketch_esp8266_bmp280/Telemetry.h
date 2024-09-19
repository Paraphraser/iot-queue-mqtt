#pragma once


// see https://www.hivemq.com/blog/mqtt-essentials-part-6-mqtt-quality-of-service-levels/
const int       MQTT_QOS_AtMostOnce         = 0;
const int       MQTT_QOS_AtLeastOnce        = 1;
const int       MQTT_QOS_ExactlyOnce        = 2;


/*
 * The states MQTT processing can be in
 */
typedef enum {

  MQTTIdleState,
  MQTTCheckConnectState,
  MQTTWaitConnectState,
  MQTTTransmitState,
  MQTTStartDisconnectState,
  MQTTWaitDisconnectState
    
} MQTT_State;

/*
 * The state this program is in at the moment is...
 */
MQTT_State mqttState = MQTTIdleState;

// comms support
WiFiClient mqtt_WiFi_client;
MQTTClient mqtt_service(512);
AsyncDelay mqtt_service_timer;
const unsigned long MQTT_service_timeout_ms = 30*1000;

// MQTT topic+payload structure
typedef struct {
  char topic[255] = { 0 };
  char payload[255] = { 0 };
  bool retain = false;
} Telemetry;

// MQTT messages waiting to be sent
cppQueue mqttQueue(sizeof(Telemetry),10,FIFO);


void try_to_enqueue (
  const char * caller,
  Telemetry* telemetry
) {

  // try to append to the queue
  bool success = mqttQueue.push(telemetry);

  #if (SerialDebugging)
  // report outcome
  Serial.printf(
    "%s() %s in queuing MQTT message\n",
    caller,
    (success ? "succeeded" : "did not succeed")
  );
  #endif

  if (!success) {

    // something wrong - force restart - no return
    fatalError(queuePushError,__func__);

  }

}


void do_mqttCheckConnectState () {

  #if (SerialDebugging)
  Serial.printf("%s()\n",__func__);
  #endif

  // sense service already running
  if (mqtt_service.connected()) {
      
    // yes! move to next state
    mqttState = MQTTTransmitState;

    // all done
    return;
      
  }

  #if (SerialDebugging)
  // report connection attempt
  Serial.printf(
    "Attempting MQTT connection with %s on port %ld as station %s\n",
    MQTTHostFQDN_or_IP,
    MQTTHostPort,
    MQTTClientID
  );
  #endif

  // not connected, timer still running, define connection to MQTT server
  mqtt_service.begin(MQTTHostFQDN_or_IP,MQTTHostPort,mqtt_WiFi_client);

  // Attempt to connect (implied skip=false argument)
  bool ignore =  mqtt_service.connect(MQTTClientID);

  // initialise a wait timer
  mqtt_service_timer.start(MQTT_service_timeout_ms, AsyncDelay::MILLIS);

  // wait for MQTT to come up
  mqttState = MQTTWaitConnectState;
  
  #if (SerialDebugging)
  Serial.println("moving to MQTTWaitConnectState");
  #endif

}


void do_mqttWaitConnectState () {

  /*
    * Only three possibilities here: 
    * 
    * 1. MQTT is available - we move to transmit
    * 2. The timeout expires and triggers a fatal error
    * 3. We exit and the main loop brings us straight back
    * 
    */
  
  // has MQTT become available?
  if (mqtt_service.connected()) {

    #if (SerialDebugging)
    Serial.println("MQTT service connected");
    #endif

    // yes! proceed to next phase
    mqttState = MQTTTransmitState;

    return;
      
  }

  // not connected. Has the timeout expired?
  if (mqtt_service_timer.isExpired()) {

    #if (SerialDebugging)
    Serial.printf(
        "MQTT connection timeout expired, err=%d, rc=%d\n",
        mqtt_service.lastError(),
        mqtt_service.returnCode()
    );
    #endif
        
    // yes! nothing else we can do 
    fatalError(connectMQTTError,__func__); // forces restart - no return
          
  }

  // otherwise remain in this state until connected
    
}


void do_mqttTransmitState () {

  // sense queue empty
  if (mqttQueue.isEmpty()) {
      
    #if (SerialDebugging)
    Serial.printf("%s() - queue is now empty\n",__func__);
    #endif

    // nothing else to send - start disconnecting
    mqttState = MQTTStartDisconnectState;

    // shortstop
    return;

  }

  /*
    * queue is not empty - dequeue the first entry
    */
  Telemetry telemetry;
  
  bool success = mqttQueue.pop(&telemetry);

  if (!success) {
      
    #if (SerialDebugging)
    Serial.println("mqttQueue.pop() returned false. Only just checked for non-empty queue. Weird!");
    #endif
    
    fatalError(queuePopError,__func__); // forces restart - no return

  }

  #if (SerialDebugging)
  Serial.printf(
    "mosquitto_pub%s-h %s -t %s -m '%s'\n",
    (telemetry.retain ? " -r " : " "),
    MQTTHostFQDN_or_IP,
    telemetry.topic,
    telemetry.payload
  );
  #endif

  // try to transmit
  success =
    mqtt_service.publish(
      telemetry.topic,
      telemetry.payload,
      telemetry.retain,
      MQTT_QOS_AtMostOnce
    );

  if (!success) {

    #if (SerialDebugging)
    Serial.printf(
      "publish failed, err=%d, rc=%d\n",
      mqtt_service.lastError(),
      mqtt_service.returnCode()
    );
    #endif
    
    fatalError(publishMQTTError,__func__); // forces restart - no return

  }
      
  /*
    * At this point there will either be more items in the queue or it is empty.
    * Either way, we stay in this state.
    */
    
}


void do_mqttStartDisconnectState () {

  #if (SerialDebugging)
  Serial.printf("%s()\n",__func__);
  #endif

  // sense service no longer connected
  if (!mqtt_service.connected()) {
      
    // yes! move to idle state
    mqttState = MQTTIdleState;

    // all done
    return;
      
  }

  // MQTT service is available
  #if (SerialDebugging)
  Serial.printf(
    "Disconnecting MQTT connection with %s on port %ld as station %s\n",
    MQTTHostFQDN_or_IP,
    MQTTHostPort,
    MQTTClientID
  );
  #endif

  // disconnect
  bool ignore = mqtt_service.disconnect();

  // connected. Initialise a wait timer
  mqtt_service_timer.start(MQTT_service_timeout_ms, AsyncDelay::MILLIS);

  #if (SerialDebugging)
  Serial.println("moving to MQTTWaitDisconnectState");
  #endif

  // wait for MQTT to go away
  mqttState = MQTTWaitDisconnectState;
    
}


void do_mqttWaitDisconnectState () {

  // is MQTT still connected?
  if (mqtt_service.connected()) {
      
    // yes! Has the timeout expired?
    if (mqtt_service_timer.isExpired()) {

      #if (SerialDebugging)
      Serial.printf(
        "MQTT disconnection timeout expired, err=%d, rc=%d will retry\n",
        mqtt_service.lastError(),
        mqtt_service.returnCode()
      );
      #endif
          
      // yes! nothing else we can do 
      fatalError(disconnectMQTTError,__func__);
            
    }

    // force another go-round of the event loop
    return;
      
  }

  /* 
    *  MQTT disconnected
    */

  #if (SerialDebugging)
  Serial.println("MQTT service disconnected");
  #endif

  // move to idle state
  mqttState = MQTTIdleState;
    
}


void mqtt_handle() {

  // give WiFi some guaranteed time
  delay(1);

  // give MQTT some time if it is connected
  if (mqtt_service.connected()) { mqtt_service.loop(); }

  switch (mqttState) {

    case MQTTCheckConnectState:     do_mqttCheckConnectState();     break;
    case MQTTWaitConnectState:      do_mqttWaitConnectState();      break;
    case MQTTTransmitState:         do_mqttTransmitState();         break;
    case MQTTStartDisconnectState:  do_mqttStartDisconnectState();  break;
    case MQTTWaitDisconnectState:   do_mqttWaitDisconnectState();   break;

    default: // MQTTIdleState

      // is the queue empty?
      if (!mqttQueue.isEmpty()) {

        #if (SerialDebugging)
        Serial.printf("%s() - queue contains messages\n",__func__);
        #endif
        
        // no! time to start a transmission run
        mqttState = MQTTCheckConnectState;
          
      }

  }

}
