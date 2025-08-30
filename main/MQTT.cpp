#include "MQTT.h"
#include "main.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "embedded_wifimanager.h"
#include "credentials.h"
#include "dse_modbus.h"
//extern char modbus_string[1000];
extern bool dse_modbus_success;
extern struct dse_modbus_data dse_data;

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
  char text_string[1000];

  memset(text_string, 0, sizeof(text_string));
  if(dse_modbus_success)
  {
      
      //sprintf(modbus_string,"{\"status\":1,\"oil_p\":%d,\"cool_t\":%d,\"oil_t\":%d,\"fuel_l\":%d,\"cgh_v\":%.2f,\"bat_v\":%.2f,\"rpm\":%d,\"f\":%.2f,\"v\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],\"a\":[%.2f,%.2f,%.2f,%.2f],\"w\":[%" PRId32 ",%" PRId32 ",%" PRId32"]}", 
      sprintf(text_string,
          "{\"status\":1,\"oil_p\":%d,\"cool_t\":%d,\"oil_t\":%d,\"fuel_l\":%d,\"cgh_v\":%.2f,\"bat_v\":%.2f,\"rpm\":%d,\"f\":%.2f,"
          "\"v\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],"
          "\"a\":[%.2f,%.2f,%.2f,%.2f],"
          "\"w\":[%" PRId32 ",%" PRId32 ",%" PRId32 "]}",
      dse_data.oil_pressure,
      dse_data.cool_temp,dse_data.oil_temp,
      dse_data.fuel_level, dse_data.chg_volt, dse_data.bat_volt, dse_data.engine_speed,
      dse_data.m_freq,dse_data.volt[0],dse_data.volt[1],dse_data.volt[2],dse_data.volt[3],dse_data.volt[4],dse_data.volt[5],
      dse_data.current[0],dse_data.current[1],dse_data.current[2],dse_data.current[3],dse_data.watt[0],dse_data.watt[1],dse_data.watt[2]);

  }
  else{
      sprintf(text_string,"{\"status\":0}");
  }

  //mqttclient.publish(topic,"{\"data\":\"111\"}");
  mqttclient.publish(topic,text_string);

}

