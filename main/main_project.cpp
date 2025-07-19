#include "Arduino.h"
#include "credentials.h"
#include "embedded_wifimanager.h"
#include "main.h"
#include "app_webserver_idf.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_pm.h>
#include <SPIFFS.h>



extern char user_token[10];
extern char app_id[10];
bool connected = false;
uint32_t check_wifi_timer = 0;

void setup(){
  Serial.begin(115200);
  memset(user_token, 0, sizeof(user_token));
  memset(app_id, 0, sizeof(app_id));
  while(!Serial){
    ; // wait for serial port to connect
  }
  readSystemVariables();
  wm_init();
  webserver_task();
}

void loop(){
    Serial.println("loop");
    delay(1000);
    if (millis() - check_wifi_timer > check_wifi_period) {
      check_wifi_timer = millis();
      wm_loop();
    }
}