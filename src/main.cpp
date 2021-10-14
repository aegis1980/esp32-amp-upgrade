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
#define LED_BT_CONNECTION_PIN 19 //blue LED
#define LED_STANDBY_PIN 21  //green LED
#define LED_MAIN_POWER_PIN 18 //red LED
#define BT_SELECTOR_PIN 5 // microswitch onto existing selector 
#define POWER_BTN_PIN 16 // fly-by-wire power button
#define MODE_BTN_PIN 17 

#define POWER_OFF 0
#define POWER_STANDBY 1
#define POWER_ENTERING_STANDBY 2
#define POWER_ON 3

#define MEDIUM_BLINK 0.5f
#define SHORT_BLINK 0.15f
#define LONG_BLINK 1.0f

BluetoothA2DPSink a2dp_sink;

float SECOND_BTCXN_CHECK = 1.0f; 
unsigned long MILLISECONDS_TO_STANDBY = 10*1000;

int powerMode = POWER_OFF;

// time shutdown will occur
unsigned long shutdown_ms = millis() + MILLISECONDS_TO_STANDBY;

bool btInputSelected = false;
bool firmwareMode = false;

bool bt_datastream = false;
bool bt_connection = false;

Ticker btnPollTicker;

// LED blinkers

/**
 * Blue led
 * 1111111111111111  : device connected
 * 1111----1111----- : no device connected. Accepting connections.
 */
Blinker btLED(LED_BT_CONNECTION_PIN);

/**
 * Green LED - bt datastream state
 * ---------------- : no datastream
 * 1111----1111---- : entering standby (no stream detected)
 * ----------------1111----------------1111 : Not in correct input
 * 1111111111111111  : datastream.
 * 
 */
Blinker btDataStreamLED(LED_STANDBY_PIN); 

/**
 * Replaces main red power LED
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * continuous on - power on (relay)
 * continuous off - power off (relay)
 * blinking - in standby 
 */
Blinker powerLED(LED_MAIN_POWER_PIN);

Switch modeBtn(MODE_BTN_PIN);
Switch btInputKnob(BT_SELECTOR_PIN);
Switch powerBtn(POWER_BTN_PIN);

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
    powerLED.blink(SHORT_BLINK,SHORT_BLINK);
    btLED.blink(SHORT_BLINK,SHORT_BLINK);
    btDataStreamLED.blink(SHORT_BLINK,SHORT_BLINK);
    enterFirmwareFlashMode();
}

void modeBtnLongPress(void* ref) {
  /*
  * Disconnect current BT device to free up for another one.
  */
  Serial.println("Mode button, long press");
  a2dp_sink.disconnect();
}

void modeBtnSingleClick(void* ref) {
  /*
  * bring out of standby.
  */
  Serial.println("Mode button, single press");

  if (!powerBtn.on() && (powerMode == POWER_STANDBY || powerMode == POWER_ENTERING_STANDBY)){
    on_data();
  } 
}

void allOff(){
  powerMode = POWER_OFF;
  digitalWrite(POWER_RELAY_PIN, LOW); //off
  powerLED.continuousOff();
  btLED.continuousOff();
  btDataStreamLED.continuousOff();
  if (bt_connection){
    a2dp_sink.end(false);
  }
  
}

/** Input selector on TAPE/DAT 2 (ie bluetooth input)*/ 
void selectorActive(void* ref){
  Serial.println("TAPE/DAT 2 selected");
  btInputSelected = true;
}


/** INput selector off TAPE/DAT 2 (ie bluetooth input)*/
void selectorInactive(void* ref){
  Serial.println("TAPE/DAT 2 deselected");
  btInputSelected = false;
}

/** Main power button ON */ 
void powerBtnActive(void* ref){
  Serial.println("Power knob to on");
  powerMode = POWER_ON;
  connectA2DPSink();
}

/** Main power button OFF */
void powerBtnInactive(void* ref){
  Serial.println("Power knob to off");
  allOff();

  //TODO 
}






void buttonPoll(){
  powerBtn.poll();
  modeBtn.poll();
  btInputKnob.poll();
 
}


void setup() {
  if (DEBUG){
    Serial.begin(115200); // open the serial port at 9600 bps:
    esp_log_level_set("*", ESP_LOG_VERBOSE);
  } 

  modeBtn.poll();
  if (modeBtn.on()){// holding mode button at startup to go into OTA mode.
    longPressAtStartUp(); //OTA firmware update mode
  } else {
    connectA2DPSink();
    btnPollTicker.attach(20.0/1000.0,buttonPoll);

    pinMode(POWER_RELAY_PIN, OUTPUT);
    
    modeBtn.longPressPeriod = 2000; // 2secs instead of the default 300ms
    modeBtn.setLongPressCallback(&modeBtnLongPress, (void*)"Mode button long press");
    modeBtn.setSingleClickCallback(&modeBtnSingleClick, (void*)"Mode button single click");

    btInputKnob.setPushedCallback(&selectorActive, (void*)"pushed");
    btInputKnob.setReleasedCallback(&selectorInactive, (void*)"released");

    powerBtn.setPushedCallback(&powerBtnActive,(void*)"pushed");
    powerBtn.setReleasedCallback(&powerBtnInactive,(void*)"released");
  }
}

void loop() {
  // check datastream timeout
  if (!firmwareMode){
    if (!powerBtn.on()){
      allOff();
    } else {
      if (millis()>shutdown_ms){
        bt_datastream = false;
      } else {
        if (shutdown_ms - millis() < MILLISECONDS_TO_STANDBY){
          powerMode = POWER_ENTERING_STANDBY;
          btDataStreamLED.blink(MEDIUM_BLINK, MEDIUM_BLINK);
        } else {
          btDataStreamLED.continuousOn();
          powerMode = POWER_ON;
        }
        bt_datastream = true;
      }

      if (btInputKnob.on()){
        if (bt_connection && bt_datastream){
          digitalWrite(POWER_RELAY_PIN, HIGH); //on
          powerLED.continuousOn();
        } else { //
          digitalWrite(POWER_RELAY_PIN, LOW); //off
          powerMode = POWER_STANDBY;
          powerLED.blink(MEDIUM_BLINK, MEDIUM_BLINK);
          if (!bt_datastream) btDataStreamLED.blink(SHORT_BLINK,LONG_BLINK);
        }
      } else {
        if (bt_connection){
          a2dp_sink.pause();
        }
        digitalWrite(POWER_RELAY_PIN, HIGH); //on
        powerLED.continuousOn();
        btDataStreamLED.continuousOff();
      }
    } // endoff if powerMode != POWER_OFF''
  } 
}