#include <cstdint>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <nvs_flash.h>
#include "nvs.h"
#include "credentials.h"
#include "embedded_wifimanager.h"
#include "main.h"
#include <Arduino.h>
#include <EEPROM.h>
#include <WiFi.h>
#include <esp_wifi.h>

static wifi_station_reconnect_flag_t wifi_reconnect_event = {0, 0};
static char device_hostname[30];
char default_auth_password[] ="12345678";
uint8_t mac[6];
extern char user_token[10];
extern char app_id[10];
uint8_t station_mode = 0, go_station = 0;
static uint32_t initial_ap_timer = 0;
wifi_settings_t device_wifi_settings = {"", ""};
extern bool connected;

enum wifi_scan_states wifi_scan_status = INIT;
int scan_task_return;

bool dhcp = false;
char ipAddress[16] = "192.168.1.155";
char netMaskAddress[16] = "255.255.255.0";
char gateWayAddress[16] = "192.168.1.1";
char DNSAddress[16] = "8.8.8.8";

#define INITIAL_AP_TIME 60000

bool check_wm_state_station() { 
  return station_mode == 1;
}



void reset_initial_ap_timer() {
  initial_ap_timer = millis();
}
void wm_activity_callback() {
  reset_initial_ap_timer();
}

void onStationConnected(WiFiEvent_t event, WiFiEventInfo_t info)
{
  wm_activity_callback();
}

void onStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("Station disconnected ");
}


void setup_ap() {
  
  WiFi.mode(WIFI_AP);
  delay(1000);
  WiFi.softAP(device_hostname, default_auth_password);
  // Start a timer.
  initial_ap_timer = millis();
  WiFi.onEvent(onStationConnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED);
  // Call "onStationDisconnected" each time a station disconnects
  WiFi.onEvent(onStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STADISCONNECTED);

}









void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
  DEBUG_PRINTLN(F("WiFi Disconnected from AP."));
  wifi_reconnect_event.flag = 1;
}

static int station_connect_wifi(const char *ssid, const char *pass) {
  int connRes;
  DEBUG_PRINTLN(F("Connecting as wifi client..."));
  IPAddress ip, gw, nm, dns;
  if (!dhcp) {
    DEBUG_PRINTLN(F("Custom STA IP/GW/Subnet"));
    ip.fromString(ipAddress);
    gw.fromString(gateWayAddress);
    nm.fromString(netMaskAddress);
    dns.fromString(DNSAddress);
    WiFi.config(ip, gw, nm, dns);
    DEBUG_PRINTLN(WiFi.localIP());
  }
  // check if we have ssid and pass and force those, if not, try with last saved
  // values
  // if (ssid != "") {
  if (strcmp(ssid, "") != 0) {
    WiFi.begin(ssid, pass);
   
  } else {
    if (WiFi.SSID()) {
      DEBUG_PRINTLN(F("Using last saved values, should be faster"));
      esp_wifi_disconnect();

      WiFi.begin();
    } else {
      Serial.println("No saved credentials");
    }
  }
  connRes = WiFi.waitForConnectResult();
  if (connRes != WL_CONNECTED) {
    connected = false;
  } else {
    connected = true;
  }
  return connRes;
}

void setup_station() {
  int connectionState;

  WiFi.mode(WIFI_STA);

  WiFi.onEvent(WiFiStationDisconnected, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  delay(1000);
  connectionState =
      station_connect_wifi((const char *)device_wifi_settings.ssid,
                           (const char *)device_wifi_settings.password);
  if (connectionState != WL_CONNECTED) {
    connected = false;
  } else {
    connected = true;
  }
}


void wm_init() {

  pinMode(AP_MODE_PIN, INPUT_PULLUP);
  sprintf(device_hostname, "APC-%s-%s", user_token, app_id);
  WiFi.setHostname(device_hostname);

  if (digitalRead(AP_MODE_PIN) == LOW)
  {
      setup_ap();
  } else if(station_mode == 0)
  {
    setup_ap();
  }
  else if(station_mode == 1)
  {
    setup_station();
  }
  else
  {
    Serial.println("wm_init: should never come here..");
    while (1)
      ;
  } 

}



void ap_loop() {
  if (digitalRead(AP_MODE_PIN) != LOW) {

    if ((millis() - initial_ap_timer > INITIAL_AP_TIME) && (station_mode == 1)) {
      ESP.restart();
    }
  }
}


void station_wifi_reconnect(void) {
  if(WiFi.status() == WL_CONNECTED) {
    wifi_reconnect_event.flag = 0;
    wifi_reconnect_event.timer = 0;
    DEBUG_PRINTLN(F("WiFi Already Connected."));
  }
  else {
    if(wifi_reconnect_event.timer == 0) {
      wifi_reconnect_event.timer = millis();
      DEBUG_PRINTLN(F("WiFi reconnect in few minutes.."));
    }
    else {
      if(millis() - wifi_reconnect_event.timer > 120000) {
        DEBUG_PRINTLN(F("WiFi reconnect event."));
        WiFi.disconnect();
        delay(1000);
        wifi_reconnect_event.flag = 0;
        wifi_reconnect_event.timer = 0;
        station_connect_wifi((const char *)device_wifi_settings.ssid,
                         (const char *)device_wifi_settings.password);
        if (connected == true) {
          DEBUG_PRINTLN("connected...yeey :)");
          DEBUG_PRINTLN("local ip");
          DEBUG_PRINTLN(WiFi.localIP());
        } 
      }
    }
  }
}

void station_loop() {
  if (connected && WiFi.status() != WL_CONNECTED) {
    connected = false;;
    wifi_reconnect_event.flag = 1;
  }
  if (!connected && WiFi.status() == WL_CONNECTED) {
    connected = true;
    DEBUG_PRINTLN("connected...yeey :)");
    DEBUG_PRINTLN("local ip");
    DEBUG_PRINTLN(WiFi.localIP());
  }
  if(wifi_reconnect_event.flag) {
    station_wifi_reconnect();
  }
}

void wm_loop() {
  if (station_mode == 1) {
    station_loop();
  } else {
    ap_loop();
  } 
}


void scan_task(void* parameter)
{
  delay(1000);
  DEBUG_PRINTLN(F("Scan start"));
  // WiFi.scanNetworks will return the number of networks found.
  scan_task_return = WiFi.scanNetworks();
  DEBUG_PRINTLN(F("Scan done"));
  wifi_scan_status = FINISHED;
  // Delete the scan result to free memory for code below.
  //WiFi.scanDelete();

  delay(10);

  vTaskDelete(NULL);
}