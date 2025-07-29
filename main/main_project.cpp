#include "Arduino.h"
#include "credentials.h"
#include "embedded_wifimanager.h"
#include "main.h"
#include "app_webserver_idf.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_pm.h>
#include <SPIFFS.h>
#include <PubSubClient.h>
#include "MQTT.h"



extern char user_token[10];
extern char app_id[10];
bool connected = false;
uint32_t check_wifi_timer = 0;

extern bool wifi_scan_start;

extern bool flashUpdateRequest;

/// sructure that contain MQTT settings
extern struct mqtt_set mqtt_setting;
extern WiFiClient espClient;
extern PubSubClient mqttclient;
extern unsigned int MQTTIntervalTimer;
extern bool mqtt_update_flag;



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
  MQTT_init();
}

void loop(){
    Serial.println("loop");
    delay(1000);
    if (millis() - check_wifi_timer > check_wifi_period) {
      check_wifi_timer = millis();
      wm_loop();
    }

    if(wifi_scan_start)
    {
      wifi_scan_start = false;
      xTaskCreate(scan_task, "scan_task", 5000, NULL, 1, NULL);
    }

    if(flashUpdateRequest)
    {
      flashUpdateRequest = false;
      saveSystemVariables();
      if (mqtt_update_flag) {
        mqttclient.disconnect();
        if ((mqtt_setting.interval >= 1) && (connected && check_wm_state_station())) {
          mqttclient.setServer(mqtt_setting.broker,(unsigned short)strtoul(mqtt_setting.port, NULL, 0));
          reconnect();
        }
        mqtt_update_flag = false;
      }
    }
    
    // mqtt client connect and sending messages if required
    if ((mqtt_setting.interval >= 1) && (connected && check_wm_state_station()) && !mqttclient.connected()) {
        reconnect();
    }
    if ((mqtt_setting.interval >= 1) &&
        (millis() - MQTTIntervalTimer > mqtt_setting.interval ) &&
        (connected && check_wm_state_station()) && mqttclient.connected()) {
        //Serial.println("start send mqtt");
        MQTT_Log();
        MQTTIntervalTimer = millis();
    }
    if (connected && check_wm_state_station()) {
        mqttclient.loop();
    }

    

}