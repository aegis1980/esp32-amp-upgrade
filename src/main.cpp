#include <Arduino.h>
#include "BluetoothA2DPSink.h"
#include "TimedBlink.h"
#include "avdweb_Switch.h"
#include <Ticker.h>
#include "firmware.h"

#define POWER_RELAY_PIN 23
#define LED_BT_CONNECTION_PIN 19
#define LED_STANDBY_PIN 18
#define BT_SELECTOR_PIN 5 // wired into original amp selector (closed on AUX, open otherwise)
#define CONTROL_BTN_PIN 17 

#define POWER_STANDBY 0
#define POWER_ENTERING_STANDBY 1
#define POWER_ON 2


BluetoothA2DPSink a2dp_sink;
esp_a2d_connection_state_t last_state;


unsigned long SECOND_BTCXN_CHECK =0.5; 
unsigned long MILLISECONDS_TO_STANDBY = 10*1000;

int powerMode = POWER_ON;

// time shutdown will occur
unsigned long shutdown_ms = millis() + MILLISECONDS_TO_STANDBY;

bool autoStandby = true;
bool firmwareMode = false;

bool bt_datastream = false;
bool bt_connection = false;

Ticker btConnectionTicker;
Ticker btnPollTicker;

Ticker ledBT;
Ticker ledPower;

// LED blinkers

/**
 * 111111111111111111 :device connected
 * --11--11--11--11-- : no device connected. Accepting connections.
 */
TimedBlink btLED(LED_BT_CONNECTION_PIN);

/**
 * 111111111111111111 :on
 * --11--11--11--11-- : going to enter standby since not datastream
 * ----11111----11111 :standby
 */
TimedBlink standbyLED(LED_STANDBY_PIN); 


Switch ctrlBtn(CONTROL_BTN_PIN);
Switch inputSelector(BT_SELECTOR_PIN);

// move shutdown time to future
void on_data() {
  shutdown_ms = millis() + MILLISECONDS_TO_STANDBY; 
}

void longPress(void* ref) {
  /*
  * disable/ enable wifi firmware updating
  */
  if (firmwareMode){
    exitFirmwareFlashMode();
    firmwareMode = false;
    btLED.resume();
    standbyLED.resume();
  } else {
    enterFirmwareFlashMode();
    firmwareMode = true;
    btLED.blink(100,100);
    standbyLED.blink(100,100);

  }
}

void singleClick(void* ref) {
  /*
  * bring out of standby.
  */
  if (powerMode == POWER_STANDBY || powerMode == POWER_ENTERING_STANDBY){
    on_data();
  } 
}

void doubleClick(void* ref) {
  /*
  *  disable/ enable toggle autostandby mode
  */
  autoStandby = !autoStandby;
}

/** INput selector on AUX*/ 
void selectorActive(void* ref){
  autoStandby = true;
}

/** INput selector on AUX*/
void selectorInactive(void* ref){
  autoStandby = false;
}

void buttonPoll(){
  ctrlBtn.poll();
  inputSelector.poll();
}

void btConnectionCheck(){
  esp_a2d_connection_state_t state = a2dp_sink.get_connection_state();

  if (last_state != state){
    bt_connection = state == ESP_A2D_CONNECTION_STATE_CONNECTED;
    Serial.println(bt_connection ? "Connected" : "Not connected");  
    if (bt_connection) {
      btLED.continuousOn();
    } else {
      btLED.blink(200,200);
    }
    last_state = state;
  }
}


void setup() {
  i2s_pin_config_t my_pin_config = {
    .bck_io_num = 26,
    .ws_io_num = 25,
    .data_out_num = 27, // changed from default GPIO22
    .data_in_num = I2S_PIN_NO_CHANGE
  };
  a2dp_sink.set_pin_config(my_pin_config);
  // startup sink
  a2dp_sink.set_on_data_received(on_data);
  a2dp_sink.start("Yamaha amp");


  btConnectionTicker.attach(SECOND_BTCXN_CHECK,btConnectionCheck);
  btnPollTicker.attach(20/1000,buttonPoll);

  pinMode(POWER_RELAY_PIN, OUTPUT);
  pinMode(LED_BT_CONNECTION_PIN, OUTPUT);
  pinMode(LED_STANDBY_PIN,OUTPUT); 

  ctrlBtn.longPressPeriod = 2 * 1000; // 2secs instead of the default 300ms
  ctrlBtn.setLongPressCallback(&longPress, (void*)"long press");
  ctrlBtn.setDoubleClickCallback(&doubleClick, (void*)"double click");
  ctrlBtn.setSingleClickCallback(&singleClick, (void*)"single click");

  //inputSelector.setPushedCallback(&selectorActive, (void*)"pushed");
  //inputSelector.setReleasedCallback(&selectorInactive, (void*)"released");

}

void loop() {
  // check datastream timeout
  if (millis()>shutdown_ms){
    bt_datastream = false;
  } else {
    if (shutdown_ms - millis() < MILLISECONDS_TO_STANDBY){
      standbyLED.blink(200,200);
      powerMode = POWER_ENTERING_STANDBY;
    } else {
      standbyLED.continuousOn();
      powerMode = POWER_ON;
    }
    bt_datastream = true;
  }

  if (autoStandby){
    if (bt_connection && bt_datastream){
      digitalWrite(POWER_RELAY_PIN, LOW);    
    } else {
      digitalWrite(POWER_RELAY_PIN, HIGH);
      powerMode = POWER_STANDBY;
      standbyLED.blink(1000,1000);
    }
  } else {
    standbyLED.blinkOff();
  }

  btLED.blink();
  standbyLED.blink();
}