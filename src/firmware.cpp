#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include "firmware.h"
#include "WiFiUdp.h"
#include "config.h"
#include <Ticker.h>

Ticker otaHandler;

void exitFirmwareFlashMode(){
  otaHandler.detach();
  WiFi.disconnect(true);
}


void enterFirmwareFlashMode(){
    WiFi.mode(WIFI_STA);
    WiFi.begin(SSID, PASSWORD);
    while (WiFi.waitForConnectResult() != WL_CONNECTED) {
        Serial.println("Connection Failed! Rebooting...");
        delay(5000);
        ESP.restart();
    }

    if (!MDNS.begin(DEVICE_NAME)) {
      Serial.println("Error setting up MDNS responder!");
      while(1){
          delay(1000);
      }
    }

    ArduinoOTA.setHostname(DEVICE_NAME);

    ArduinoOTA
    .onStart([]() {
      String type;
      if (ArduinoOTA.getCommand() == U_FLASH)
        type = "sketch";
      else // U_SPIFFS
        type = "filesystem";

      // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
      Serial.println("Start updating " + type);
    })
    .onEnd([]() {
      Serial.println("\nEnd");
       Serial.println("Restarting device in 3 seconds.");
      delay(3000);
      ESP.restart();
    })
    .onProgress([](unsigned int progress, unsigned int total) {
      Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
    })
    .onError([](ota_error_t error) {
      Serial.printf("Error[%u]: ", error);
      if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
      else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
      else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
      else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
      else if (error == OTA_END_ERROR) Serial.println("End Failed");
    });

  ArduinoOTA.begin();

  Serial.println("Ready");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("Starting ota handler");
  otaHandler.attach(0.1,handle);

}

void handle(){
    ArduinoOTA.handle();
}