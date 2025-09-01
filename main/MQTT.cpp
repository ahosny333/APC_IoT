#include "MQTT.h"
#include "main.h"
#include <Arduino.h>
#include <WiFi.h>
#include <PubSubClient.h>
#include "embedded_wifimanager.h"
#include "credentials.h"
#include "dse_modbus.h"
#include "cJSON.h"
//extern char modbus_string[1000];
extern bool dse_modbus_success;
extern struct dse_modbus_data dse_data;

/// sructure that contain MQTT settings
struct mqtt_set mqtt_setting;
WiFiClient espClient;
PubSubClient mqttclient(espClient);
unsigned int MQTTIntervalTimer = 0;
bool mqtt_update_flag;
static char test_topic_mac[13]="1";
static uint32_t mqtt_reconnect_timer;
static uint32_t mqtt_reconnect_period = 60000; // 60 seconds
char command_topic[100];

// {'command_name': 'Auto mode', 'command_value': 0}
char command_name[30];
uint8_t command_value;
bool start_write = false;

// uint8_t parse_payload(char *payload) {
//     if (payload == NULL) {
//         Serial.println("1");
//         return 0;
//     }

//     char *name_start = strstr(payload, "\"command_name\"");
//     char *value_start = strstr(payload, "\"command_value\"");

//     if (!name_start || !value_start) {
//         Serial.println("2");
//         return 0; // Missing key
//     }

//     // Extract command_name
//     name_start = strchr(name_start, ':'); 
//     if (!name_start) {
//       Serial.println("3");
//       return 0;
//     }
//     name_start++; // move past :

//     // Skip possible spaces and quotes
//     while (*name_start == ' ' || *name_start == '\"') {
//         name_start++;
//     }

//     char *name_end = strchr(name_start, '\"');
//     if (!name_end) {
//       Serial.println("4");
//       return 0;
//     }
//     size_t len = name_end - name_start;
//     if (len >= sizeof(command_name)) len = sizeof(command_name) - 1;

//     strncpy(command_name, name_start, len);
//     command_name[len] = '\0';

//     // Extract command_value
//     value_start = strchr(value_start, ':');
//     if (!value_start) {
//       Serial.println("5");
//       return 0;
//     }

//     value_start++; // move past :

//     while (*value_start == ' ' || *value_start == '\"') {
//         value_start++;
//     }

//     int val;
//     if (sscanf(value_start, "%d", &val) != 1) {
//         Serial.println("6");
//         return 0; // Failed to parse int
//     }
//     command_value = (uint8_t)val;

//     return 1; // Success
// }


uint8_t parse_payload(char *payload) {
    if (payload == NULL) {
        return 0;
    }

    // Parse the JSON string
    cJSON *root = cJSON_Parse(payload);
    if (root == NULL) {
        return 0; // Invalid JSON
    }

    // Extract command_name
    cJSON *name_item = cJSON_GetObjectItemCaseSensitive(root, "command_name");
    if (!cJSON_IsString(name_item) || (name_item->valuestring == NULL)) {
        cJSON_Delete(root);
        return 0; // Missing or invalid command_name
    }

    // Copy safely into global buffer
    strncpy(command_name, name_item->valuestring, sizeof(command_name) - 1);
    command_name[sizeof(command_name) - 1] = '\0';

    // Extract command_value
    cJSON *value_item = cJSON_GetObjectItemCaseSensitive(root, "command_value");
    if (!cJSON_IsNumber(value_item)) {
        cJSON_Delete(root);
        return 0; // Missing or invalid command_value
    }
    command_value = (uint8_t)value_item->valueint;

    // Clean up memory
    cJSON_Delete(root);

    return 1; // Success
}




static void MQTT_callback(char *topic, byte *payload, unsigned int length)
{
  // DEBUG_PRINTLN("mqtt Message arrived ");
  DEBUG_PRINT(topic);
  

  if(strcmp(topic,command_topic) == 0) {
    char buf[70];                      
    memset(buf, 0, sizeof(buf));
    if (length >= sizeof(buf)) {        // safety check
        // message too long – handle error here
        return;
    }
    memcpy(buf, payload, length);       // copy the bytes
    buf[length] = '\0';   
    if(parse_payload(buf))
    {
      // DEBUG_PRINTLN("success receieve command");
      // DEBUG_PRINTLN(command_name);
      // DEBUG_PRINTLN(command_value);
      start_write = true;

    }
    else
    {
      DEBUG_PRINTLN("failed receieve command");
    }







  }
  
  

}



void MQTT_init(void) {
  MQTTIntervalTimer = millis();
  mqttclient.setServer(mqtt_setting.broker,
                       (unsigned short)strtoul(mqtt_setting.port, NULL, 0));
  mqttclient.setCallback(MQTT_callback);
}


void reconnect()
{
  if(millis() - mqtt_reconnect_timer > mqtt_reconnect_period){
    mqtt_reconnect_timer = millis();
    char topic[50];
    uint8_t MAC_array[6];
    WiFi.macAddress(MAC_array);
    // sprintf(test_topic_mac, "%02X%02X%02X%02X%02X%02X",MAC_array[0], MAC_array[1],
    //     MAC_array[2], MAC_array[3], MAC_array[4], MAC_array[5]); 
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
        
        memset(command_topic, 0, 100);
        sprintf(command_topic, "device/%s/%s/command",mqtt_setting.token,test_topic_mac);
        Serial.println(command_topic);
        mqttclient.subscribe(command_topic);
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
  sprintf(topic, "device/%s/%s/data",mqtt_setting.token,test_topic_mac);
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

