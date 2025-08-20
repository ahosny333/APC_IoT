#include "MQTT.h"
#include "main.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "embedded_wifimanager.h"
#include "credentials.h"

/// sructure that contain MQTT settings
struct mqtt_set mqtt_setting;
WiFiClient espClient;
PubSubClient mqttclient(espClient);
unsigned int MQTTIntervalTimer = 0;
bool mqtt_update_flag;
static char test_topic_mac[13];
static uint32_t mqtt_reconnect_timer;
static uint32_t mqtt_reconnect_period = 60000; // 60 seconds


static uint8_t mqtt_app_publish(uint8_t topic_id, uint32_t uptime);





void MQTT_init(void) {
  MQTTIntervalTimer = millis();
  mqttclient.setServer(mqtt_setting.broker,
                       (unsigned short)strtoul(mqtt_setting.port, NULL, 0));
}


void reconnect()
{
  if(millis() - mqtt_reconnect_timer > mqtt_reconnect_period){
    mqtt_reconnect_timer = millis();
    char topic[50];
    uint8_t MAC_array[6];
    WiFi.macAddress(MAC_array);
    sprintf(test_topic_mac, "%02X%02X%02X%02X%02X%02X",MAC_array[0], MAC_array[1],
        MAC_array[2], MAC_array[3], MAC_array[4], MAC_array[5]); 
    DEBUG_PRINTLN("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32Client-";
    clientId += String(random(0xffff), HEX);
    memset(topic, 0, 50);
    sprintf(topic, "%s/online",test_topic_mac );
    if(strlen(mqtt_setting.user) && strlen(mqtt_setting.password)){
      if (mqttclient.connect(clientId.c_str(),mqtt_setting.user,mqtt_setting.password,topic,0,false,"online")) {
        DEBUG_PRINTLN("connected");
        mqttclient.publish(topic,"connected");
        DEBUG_PRINT(topic);
      } else {
        DEBUG_PRINT("failed, rc=");
        DEBUG_PRINT(mqttclient.state());
      }
    }
    else{
      if (mqttclient.connect(clientId.c_str(),topic,0,false,"online")) {
        DEBUG_PRINTLN("connected");
        mqttclient.publish(topic,"connected");
        DEBUG_PRINT(topic);
      } else {
        DEBUG_PRINT("failed, rc=");
        DEBUG_PRINT(mqttclient.state());
      }

    }
  }
}


void MQTT_Log() {

  char topic[50];
  sprintf(topic, "device/%s/%s/data",test_topic_mac,mqtt_setting.token);
  mqttclient.publish(topic,"{\"data\":\"111\"}");

}

