#ifndef _MQTT_DEMO_H_
#define _MQTT_DEMO_H_

#include <Arduino.h>

// structure that contain mqtt setting parameters
// struct mqtt_set {
//   char broker[32] = "broker.mqtt.cool";
//   char port[33] = "1883";
//   char user[16];
//   char password[16];
//   char token[16];
//   unsigned short interval = 1000;
// };
struct mqtt_set {
  char broker[32];
  char port[33];
  char user[16];
  char password[16];
  char token[16];
  unsigned short interval;
};
#define MQTT_STRUCT_SIZE_WITHOUT_RESPONSE 99

/**
 * Initialize MQTT 
 *
 */
void MQTT_init(void);

/**
 * setting and send the parameters required by publishing
 * mqtt messages . called in the main loop each specific interval
 */
void MQTT_Log(void);

/**
 * connect and also reconnect the mqtt client to the broker. called
 * in the set up and also when require to reconnect to the server
 */
void reconnect();
#endif
