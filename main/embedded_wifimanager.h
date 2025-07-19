#ifndef WIFIMANAGER_H_
#define WIFIMANAGER_H_

#include <Arduino.h>
#include <stdint.h>
/**
 * Structure to save the Wi-Fi vredentials.
 *
 * ssid: 32 character string for the SSID name.
 * password: 63 character string for the SSPSK.
 * wifi_station_flag: flag to indicate weither the device will boot in the
 * Access point or station mode.
 */
typedef struct wifi_settings {
  char ssid[33];
  char password[64];
} wifi_settings_t;

typedef struct wifi_station_reconnect_flag {
  uint8_t flag;
  uint32_t timer;
} wifi_station_reconnect_flag_t;

enum wifi_scan_states { INIT = 'I', STARTED = 'S', FINISHED = 'F' };
extern wifi_settings_t device_wifi_settings;


void wm_init();
void wm_loop();
void wm_activity_callback();
void scan_task(void* parameter);

#endif
