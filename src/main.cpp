#include <Arduino.h>
#include "BluetoothA2DPSink.h"

#define POWER_RELAY_PIN 23
#define LED_BT_CONNECTION_PIN 19
#define LED_STANDBY_PIN 18



BluetoothA2DPSink a2dp_sink;
esp_a2d_connection_state_t last_state;
uint16_t SECONDS_TO_STANDBY = 10;
unsigned long shutdown_ms = millis() + 1000 * SECONDS_TO_STANDBY;
bool data_stream = false;
bool bt_connection = false;


// move shutdown time to future
void on_data() {
  shutdown_ms = millis() + 1000 * SECONDS_TO_STANDBY; 
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
  a2dp_sink.start("Yamp");

  pinMode(POWER_RELAY_PIN, OUTPUT);
  pinMode(LED_BT_CONNECTION_PIN, OUTPUT);
}

void loop() {
  // check timeout
  if (millis()>shutdown_ms){
    // stop the processor
    Serial.println("Shutting down");
    data_stream = false;
  } else {
    data_stream = true;
  }
  // check state
  esp_a2d_connection_state_t state = a2dp_sink.get_connection_state();
  if (last_state != state){
    bt_connection = state == ESP_A2D_CONNECTION_STATE_CONNECTED;
    Serial.println(bt_connection ? "Connected" : "Not connected");    
    last_state = state;
  }
  delay(1000);

  if (bt_connection && data_stream){
    digitalWrite(POWER_RELAY_PIN, LOW);
  } else {
    digitalWrite(POWER_RELAY_PIN, HIGH);
  }

}