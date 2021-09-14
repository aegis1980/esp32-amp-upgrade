#include <Arduino.h>
#include "BluetoothA2DPSink.h"
#include "avdweb_Switch.h"
#include <Ticker.h>
#include "Blinker.h"
#include "config.h"
#include "firmware.h"
#include <ArduinoOTA.h>
#include "esp_log.h"

#define POWER_RELAY_PIN 23
#define LED_BT_CONNECTION_PIN 19
#define LED_STANDBY_PIN 18
#define BT_SELECTOR_PIN 5 // wired into original amp selector (closed on AUX, open otherwise)
#define CONTROL_BTN_PIN 17 

#define POWER_STANDBY 0
#define POWER_ENTERING_STANDBY 1
#define POWER_ON 2

#define MEDIUM_BLINK 0.5f
#define SHORT_BLINK 0.15f
#define LONG_BLINK 1.0f

BluetoothA2DPSink a2dp_sink;

float SECOND_BTCXN_CHECK = 1.0f; 
unsigned long MILLISECONDS_TO_STANDBY = 10*1000;

int powerMode = POWER_ON;

// time shutdown will occur
unsigned long shutdown_ms = millis() + MILLISECONDS_TO_STANDBY;

bool autoStandby = true;
bool firmwareMode = false;

bool bt_datastream = false;
bool bt_connection = false;

Ticker btnPollTicker;

// LED blinkers

/**
 * 1111111111111111  : device connected
 * 1111----1111----- : no device connected. Accepting connections.
 */
Blinker btLED(LED_BT_CONNECTION_PIN);

/**
 * ---------------- :on
 * 1111----1111---- : entering standby
 * 1111111111111111  : in standby
 */
Blinker standbyLED(LED_STANDBY_PIN); 

Switch ctrlBtn(CONTROL_BTN_PIN);
Switch inputSelector(BT_SELECTOR_PIN);

// move shutdown time to future
void on_data() {
  shutdown_ms = millis() + MILLISECONDS_TO_STANDBY; 
}

void onBTConnect(){
  btLED.continuousOn();
  bt_connection = true;
}

void onBTDisconnect(){
  if (!firmwareMode) btLED.blink(MEDIUM_BLINK,MEDIUM_BLINK);
  bt_connection = false;
}

void connectA2DPSink(){
  i2s_pin_config_t my_pin_config = {
    .bck_io_num = 26,
    .ws_io_num = 25,
    .data_out_num = 27, // changed from default GPIO22
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  a2dp_sink.set_pin_config(my_pin_config);
  // startup sink
  a2dp_sink.set_on_data_received(on_data);
  a2dp_sink.set_on_connected2BT(onBTConnect);
  a2dp_sink.set_on_disconnected2BT(onBTDisconnect);
  a2dp_sink.start(DEVICE_NAME,true);
  btLED.blink(MEDIUM_BLINK,MEDIUM_BLINK);
}

void longPressAtStartUp(){
    firmwareMode = true;
    Serial.println("Entering firmware mode");
    btLED.blink(SHORT_BLINK,SHORT_BLINK);
    standbyLED.blink(SHORT_BLINK,SHORT_BLINK);
    enterFirmwareFlashMode();
}

void longPress(void* ref) {
  /*
  * Disconnect current BT device to free up for another one.
  */
  Serial.println("Long press");
  a2dp_sink.disconnect();
}

void singleClick(void* ref) {
  /*
  * bring out of standby.
  */
  Serial.println("Single click");
  if (powerMode == POWER_STANDBY || powerMode == POWER_ENTERING_STANDBY){
    on_data();
  } 
}


/** INput selector on AUX*/ 
void selectorActive(void* ref){
  autoStandby = true;
}

/** INput selector off AUX*/
void selectorInactive(void* ref){
  autoStandby = false;
}

void buttonPoll(){
  ctrlBtn.poll();
  inputSelector.poll();
}


void setup() {
  if (DEBUG){
    Serial.begin(115200); // open the serial port at 9600 bps:
    esp_log_level_set("*", ESP_LOG_VERBOSE);
  } 

  pinMode(CONTROL_BTN_PIN, INPUT);
  if (digitalRead(CONTROL_BTN_PIN) == LOW){
    longPressAtStartUp();
  } else {
    connectA2DPSink();
    btnPollTicker.attach(20.0/1000.0,buttonPoll);

    pinMode(POWER_RELAY_PIN, OUTPUT);

    ctrlBtn.longPressPeriod = 1000; // 2secs instead of the default 300ms
    ctrlBtn.setLongPressCallback(&longPress, (void*)"long press");
    ctrlBtn.setSingleClickCallback(&singleClick, (void*)"single click");

    inputSelector.setPushedCallback(&selectorActive, (void*)"pushed");
    inputSelector.setReleasedCallback(&selectorInactive, (void*)"released");
  }
}

void loop() {
  // check datastream timeout
  if (!firmwareMode){
    if (millis()>shutdown_ms){
      bt_datastream = false;
    } else {
      if (shutdown_ms - millis() < MILLISECONDS_TO_STANDBY){
        standbyLED.blink(MEDIUM_BLINK,MEDIUM_BLINK);
        powerMode = POWER_ENTERING_STANDBY;
      } else {
        standbyLED.continuousOff();
        powerMode = POWER_ON;
      }
      bt_datastream = true;
    }

    if (autoStandby){
      if (bt_connection && bt_datastream){
        digitalWrite(POWER_RELAY_PIN, HIGH); //on
      } else {
        digitalWrite(POWER_RELAY_PIN, LOW); //off
        powerMode = POWER_STANDBY;
        standbyLED.continuousOn();
      }
    } else {
      standbyLED.continuousOff();
      digitalWrite(POWER_RELAY_PIN, HIGH); //on
    }
  } else {
    ArduinoOTA.handle();
  }
  

}