#pragma once

/*
 * 
 *  Sensor handling
 *  
 */


const char *    TopicDeviceKey              = "bmp280";
const char *    TopicTemperatureKey         = "temperature";
const char *    TopicPressureKey            = "pressure";

const char *    PayloadCelsiusKey           = "\"temp_C\"";
const char *    PayloadFahrenheitKey        = "\"temp_F\"";

const char*     PayloadLocalPressureKey     = "\"local_hPa\"";
const char*     PayloadSeaLevelPressureKey  = "\"sea_hPa\"";
const char*     PayloadTrendKey             = "\"trend\"";
const char*     PayloadTrendTrainingValue   = "\"training\"";
const char*     PayloadTrendFallingValue    = "\"falling\"";
const char*     PayloadTrendSteadyValue     = "\"steady\"";
const char*     PayloadTrendRisingValue     = "\"rising\"";

/*
 * The states the sensor can be in
 */
typedef enum {
  SensorInitialise,
  SensorStabilising,
  SensorRead,
  SensorIdle
} SensorState;

/*
 * Holds the current sensor state
 */
SensorState sensorState = SensorInitialise;

/* 
 *  A timer for sensor operations
 */
AsyncDelay sensorTimer;

/*
 * Some sensors benefit from being given a bit of time to
 * warm up when first initialised. This time can be adjusted
 * via sensorStabilisationTime_ms
 */
const unsigned long sensorStabilisationTime_ms = 500;

/*
 * The frequency with which the sensor is read. This can be reduced for
 * testing but should be set to 10 minutes for production. That's because
 * the reliability of the "trend" estimate really needs 6 observations
 * equally-spaced in time over at least one hour. If the time between
 * observations is significantly shorter, you'll get an answer but it
 * might not be a sensible answer.
 */
const unsigned long sensorScanTime_ms = 10*60*1000;

/*
 * The sensor API
*/
Adafruit_BMP280 bmp280; // use I2C interface


float equivalentPressureAtSeaLevel  (
  float ph,    // pressure at this altitude (hPa)
  float t      // temperature at this altitude (celsius)
) {

  // see https://keisan.casio.com/exec/system/1224575267

  // precalculate altitude, corrected for the temperature lapse rate
  float hl = LocalHeightAboveSeaLevelInMetres * 0.0065;

  return ph / pow(1.0-hl/(t+hl+273.15),5.257);
    
}


const char* pressureAnalysisIncluding(double newPressure) {

  /*
  *  Note: the size of the pressure history array and the
  *  Critical value of t are related. If you change the size
  *  of the array, you must recalculate Critical_t_value.
  *  
  *  Given:
  *  
  *      Let α = 0.05    (5%)
  *      Let ν = (PressureHistorySize - 2) = (6 - 2) = 4
  *      
  *  Then use one of the following:
  *  
  *      Excel:      =T.INV(α/2,ν)  
  *      
  *      TINspire:   invt(α/2,ν)
  *      
  *  Given those inputs, the answer is:
  *  
  *      -2.776445105
  *      
  *  Critical_t_value is the absolute value of that.
  *
  *  PressureHistorySize is 6 and assumes 10-minute
  *  intervals between observations, meaning it should
  *  take an hour until the history array is full.
  *  
  */
  static const size_t PressureHistorySize = 6;
  static const double Critical_t_value = 2.776445105;

  // the array of pressures
  static double pressures[PressureHistorySize] = { };

  // the number of elements currently in the array array
  static size_t pressureHistoryCount = 0;

  // have we filled the array?
  if (pressureHistoryCount < PressureHistorySize) {

    // no! add this observation to the array
    pressures[pressureHistoryCount] = newPressure;

    // bump n
    pressureHistoryCount++;
      
  } else {

    // yes! the array is full so we have to make space
    for (size_t i = 1; i < PressureHistorySize; i++) {

      pressures[i-1] = pressures[i];

    }

    // now we can fill in the last slot
    pressures[PressureHistorySize-1] = newPressure;        
      
  }

  // is the array full yet?
  if (pressureHistoryCount < PressureHistorySize) {

    // no! we are still training
    return PayloadTrendTrainingValue;
      
  }

  /*
    * Step 1 : calculate the straight line of best fit (least-squares
    *          (linear regression). In effect we are assuming we can put
    *          time on the X axis and pressure on the Y axis, and then
    *          estimate the likely pressure at a point in time, depending
    *          on a sliding window of equally-spaced observations taken
    *          at 10-minute intervals over the last hour.
    */

  double sum_x = 0.0;     // ∑(x)
  double sum_xx = 0.0;    // ∑(x²)
  double sum_y = 0.0;     // ∑(y)
  double sum_xy = 0.0;    // ∑(xy)
  double n = 1.0 * PressureHistorySize;

  // iterate to calculate the above values
  for (size_t i = 0; i < PressureHistorySize; i++) {

    double x = 1.0 * i;
    double y = pressures[i];

    sum_x = sum_x + x;
    sum_xx = sum_xx + x * x;
    sum_y = sum_y + y;
    sum_xy = sum_xy + x * y;
      
  }

  // calculate the slope and intercept
  double slope = (sum_x*sum_y - n*sum_xy) / (sum_x*sum_x - n*sum_xx);
  double intercept = (sum_y -slope*sum_x) / n;

  /*
    * Step 2 : Perform an hypothesis test on the equation of the linear model
    *          to see whether, statistically, the available data suggests
    *          the slope is non-zero.
    *          
    *          Let beta1 = the slope of the regression line between fixed time
    *          intervals and pressure observations.
    *          
    *          H0: β₁ = 0    (the slope is zero)
    *          H1: β₁ ≠ 0    (the slope is not zero)
    *          
    *          The level of significance: α is 5% (0.05)
    *          
    *          The test statistic is:
    *          
    *              tObserved = (b₁ - β₁) / s_b₁
    *              
    *          In this context, b₁ is the estimated slope of the linear model
    *          and β₁ the reference value from the hypothesis being tested.
    *          s_b₁ is the standard error of b₁.
    *
    *          From H0, β₁ = 0 so the test statistic simplifies to:
    * 
    *              tObserved = b₁ / s_b₁
    *      
    *          This is a two-tailed test so half of α goes on each side of
    *          the T distribution.
    *          
    *          The degrees-of-freedom, ν, for the test is:
    *          
    *              ν = n-2 = 6 - 2 = 4
    *              
    *          The critical value (calculated externally using Excel or a
    *          graphics calculator) is:
    * 
    *              -tCritical = invt(0.05/2,4) = -2.776445105
    *      
    *          By symmetry:
    * 
    *              +tCritical = abs(-tCritical)
    *              
    *          The decision rule is:
    * 
    *              reject H0 if tObserved < -tCritical or tObserved > +tCritical
    *      
    *          which can be simplified to:
    * 
    *              reject H0 if abs(tObserved) > +tCritical
    *              
    *          The next step is to calculate the test statistic but one of
    *          the inputs to that calculation is SSE, so we need that first.
    *      
    */

  double SSE = 0.0;        // ∑((y-ŷ)²)

  // iterate
  for (size_t i = 0; i < PressureHistorySize; i++) {

    double y = pressures[i];
    double residual = y - (intercept + slope * i);
    SSE = SSE + residual * residual;

  }

  /*    
    *          Now we can calculate the test statistic. Note the use
    *          of the fabs() function below to force the result into
    *          the positive domain for comparison with Critical_t_value
    */

  double tObserved = fabs(slope/(sqrt(SSE / (n-2.0)) / sqrt(sum_xx - sum_x*sum_x/n)));

  /*    
    *          Finally, make the decision and return a string summarising
    *          the conclusion.
    */

  // is tObserved further to the left or right than tCritical?
  if (tObserved > Critical_t_value) {

    // yes! what is the sign of the slope?
    if (slope < 0.0) {

      return PayloadTrendFallingValue;
        
    } else {

      return PayloadTrendRisingValue;
        
    }

  }

  // otherwise, the slope may be zero
  return PayloadTrendSteadyValue;

}


void publish_bmp280_temperature(
  float celsius
) {
    
  // new queue entry
  Telemetry telemetry;

  // construct the topic
  sprintf(
    telemetry.topic,
    "%s/%s/%s/%s",
    MQTTTopicPrefix,
    MQTTClientID,
    TopicDeviceKey,
    TopicTemperatureKey
  );

  // conversion
  float fahrenheit = 32.0 + 9.0 * celsius / 5.0;
    
  // construct payload
  sprintf(
    telemetry.payload,
    "{%s:%0.1f,%s:%0.1f}",
    PayloadCelsiusKey,
    celsius,
    PayloadFahrenheitKey,
    fahrenheit
  );

  // publish the temperature payload
  try_to_enqueue(__func__,&telemetry);

}


void publish_bmp280_pressure(
  float localHPa,
  float seaLevelHPa,
  const char * trend
) {

  // new queue entry
  Telemetry telemetry;

  // construct topic
  sprintf(
    telemetry.topic,
    "%s/%s/%s/%s",
    MQTTTopicPrefix,
    MQTTClientID,
    TopicDeviceKey,
    TopicPressureKey
  );

  // construct payload
  sprintf(
    telemetry.payload,
    "{%s:%0.2f,%s:%0.2f,%s:%s}",
    PayloadLocalPressureKey,
    localHPa,
    PayloadSeaLevelPressureKey,
    seaLevelHPa,
    PayloadTrendKey,
    trend
  );

  // publish the pressure payload
  try_to_enqueue(__func__,&telemetry);

}


void enterSensorIdleLoop() {

  // start a timer
  sensorTimer.start(sensorScanTime_ms, AsyncDelay::MILLIS);

  // move to idle state
  sensorState = SensorIdle;
    
}


void do_SensorInitialise() {
    
  #if (SerialDebugging)
  Serial.printf("%s()\n",__func__);
  #endif

  /*
    *  sensor initialisation
    */

  // can we start the sensor?
  if (!bmp280.begin()) { fatalError(sensorStartError,__func__); }

  // sensor found - configure    
  bmp280.setSampling(
    Adafruit_BMP280::MODE_NORMAL,     /* Operating Mode. */
    Adafruit_BMP280::SAMPLING_X2,     /* Temp. oversampling */
    Adafruit_BMP280::SAMPLING_X16,    /* Pressure oversampling */
    Adafruit_BMP280::FILTER_X16,      /* Filtering. */
    Adafruit_BMP280::STANDBY_MS_500   /* Standby time. */
  );

  #if (SerialDebugging)
  // report
  bmp280.getTemperatureSensor()->printSensorDetails();
  #endif

  // start a timer
  sensorTimer.start(sensorStabilisationTime_ms, AsyncDelay::MILLIS);

  // move to stabilising mode (wait for it to react to power being applied)
  sensorState = SensorStabilising;
    
}


void do_SensorStabilising() {
    
  // has the timeout expired?
  if (! sensorTimer.isExpired()) {

    // no! shortstop
    return;

  }

  // go idle
  enterSensorIdleLoop();
    
}


void do_SensorRead() {

  // structs for replies
  sensors_event_t temp_event, pressure_event;

  // read temperature value
  bmp280.getTemperatureSensor()->getEvent(&temp_event);

  // abort on bad reading
  if (!temp_event.temperature)  { fatalError(sensorMalfunctionError,__func__); }

  #if (SerialDebugging)
  // diagnostic
  Serial.printf("BMP280 temp: %0.2f C\n",temp_event.temperature);
  #endif

  // transmit temperature
  publish_bmp280_temperature(temp_event.temperature);


  // read pressure value
  bmp280.getPressureSensor()->getEvent(&pressure_event);

  // abort on bad reading
  if (!pressure_event.pressure) { fatalError(sensorMalfunctionError,__func__); }

  #if (SerialDebugging)
  // diagnostic
  Serial.printf("BMP280 local pressure: %0.2f HPa\n",pressure_event.pressure);
  #endif

  // calculate equivalent barometric pressure at sea level
  float seaLevelPressure = equivalentPressureAtSeaLevel(pressure_event.pressure,temp_event.temperature);

  // transmit pressure
  publish_bmp280_pressure(
    pressure_event.pressure,
    seaLevelPressure,
    pressureAnalysisIncluding(seaLevelPressure)
  );

  
  // go idle
  enterSensorIdleLoop();
    
}


void do_SensorIdle() {
    
  // has the idle timer expired?
  if (sensorTimer.isExpired()) {

    // yes! go and read the sensor
    sensorState = SensorRead;

    // short stop
    return;

  }

}


void sensor_handle() {

  // give WiFi some guaranteed time
  delay(1);

  switch (sensorState) {

    case SensorInitialise:          do_SensorInitialise();           break;
    case SensorStabilising:         do_SensorStabilising();          break;
    case SensorRead:                do_SensorRead();                 break;
    case SensorIdle:                do_SensorIdle();                 break;
    default:                        enterSensorIdleLoop();

  }

}
