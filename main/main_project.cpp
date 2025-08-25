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

// === TinyGSM configuration macros ===
#define TINY_GSM_MODEM_SIM7600
#define TINY_GSM_USE_GPRS true
#define TINY_GSM_USE_WIFI false
#define TINY_GSM_USE_ETHERNET false
#define TINY_GSM_DEBUG Serial

#include <TinyGsmClient.h>

#define MODEM_PWRKEY_PIN  4
#define MODEM_FLIGHT_PIN  25
#define MODEM_DTR_PIN     32
#define MODEM_TX_PIN      27
#define MODEM_RX_PIN      26
#define MODEM_BAUDRATE    115200

// set GSM PIN, if any
#define GSM_PIN ""

bool sim_connected = false;

// APN for SIM
const char apn[] = "your_apn";
const char gprsUser[] = "";
const char gprsPass[] = "";

HardwareSerial SerialAT(1);
TinyGsm modem(SerialAT);
TinyGsmClient netClient(modem);
PubSubClient mqttclient(netClient);

void powerOnModem() {
    pinMode(MODEM_FLIGHT_PIN, OUTPUT);
    pinMode(MODEM_DTR_PIN, OUTPUT);
    digitalWrite(MODEM_FLIGHT_PIN, HIGH);
    digitalWrite(MODEM_DTR_PIN, LOW);

    pinMode(MODEM_PWRKEY_PIN, OUTPUT);
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
    delay(100);
    digitalWrite(MODEM_PWRKEY_PIN, HIGH);
    delay(1500);
    digitalWrite(MODEM_PWRKEY_PIN, LOW);
}




extern char user_token[10];
extern char app_id[10];
bool connected = false;
uint32_t check_wifi_timer = 0;

extern bool wifi_scan_start;

extern bool flashUpdateRequest;

/// sructure that contain MQTT settings
extern struct mqtt_set mqtt_setting;
// extern WiFiClient espClient;
// extern PubSubClient mqttclient;
extern unsigned int MQTTIntervalTimer;
extern bool mqtt_update_flag;



void setup(){
  Serial.begin(115200);
  memset(user_token, 0, sizeof(user_token));
  memset(app_id, 0, sizeof(app_id));
  while(!Serial){
    ; // wait for serial port to connect
  }
  powerOnModem();

  SerialAT.begin(MODEM_BAUDRATE, SERIAL_8N1, MODEM_RX_PIN, MODEM_TX_PIN);
  delay(3000);

  Serial.println("Initializing modem...");
  modem.restart();  // modem.init()
  Serial.print("Modem Info: ");
  Serial.println(modem.getModemInfo());

  #if TINY_GSM_USE_GPRS
    // Unlock your SIM card with a PIN if needed
    if (GSM_PIN && modem.getSimStatus() != 3) {
        modem.simUnlock(GSM_PIN);
    }
  #endif

  Serial.print("Waiting for network...");
  if (!modem.waitForNetwork(60000)) {
    Serial.println(" failed!");
    while (1) delay(10000);
  }
  Serial.println(" Network found");
  if (modem.isNetworkConnected()) {
        Serial.println("Network connected");
    }

#if TINY_GSM_USE_GPRS
    // GPRS connection parameters are usually set after network registration
    Serial.print("Connecting via GPRS...");
    if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
        Serial.println(" failed!");
        while (1) delay(10000);
    }
    Serial.println(" success");

    if (modem.isGprsConnected()) {
        Serial.println("GPRS connected");
        sim_connected = true;
    }
#endif

  Serial.print("Local IP: ");
  Serial.println(modem.localIP());


  readSystemVariables();
  wm_init();
  webserver_task();
  MQTT_init();
}

void loop(){
    Serial.println("loop");
    delay(1000);



    //////////////////////////
    // Make sure we're still registered on the network
    if (!modem.isNetworkConnected()) {
        Serial.println("Network disconnected");
        if (!modem.waitForNetwork(180000L, true)) {
            Serial.println(" fail to reconnect");
            delay(10000);
            sim_connected = false;
        }
        if (modem.isNetworkConnected()) {
            Serial.println("Network re-connected");
        }

    #if TINY_GSM_USE_GPRS
        // and make sure GPRS/EPS is still connected
        if (!modem.isGprsConnected()) {
            Serial.println("GPRS disconnected!");
            Serial.print("Connecting to ");
            Serial.print(apn);
            if (!modem.gprsConnect(apn, gprsUser, gprsPass)) {
                Serial.println(" fail to reconnect gprs");
                delay(10000);
                sim_connected = false;
            }
            if (modem.isGprsConnected()) {
                Serial.println("GPRS reconnected");
                sim_connected = true;
            }
        }
    #endif
    }

    //////////////////////////////////

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
        if ((mqtt_setting.interval >= 1) && ((connected && check_wm_state_station()) || (sim_connected))) {
          mqttclient.setServer(mqtt_setting.broker,(unsigned short)strtoul(mqtt_setting.port, NULL, 0));
          reconnect();
        }
        mqtt_update_flag = false;
      }
    }
    
    // mqtt client connect and sending messages if required
    if ((mqtt_setting.interval >= 1) && ((connected && check_wm_state_station()) || (sim_connected)) && !mqttclient.connected()) {
        reconnect();
    }
    if ((mqtt_setting.interval >= 1) &&
        (millis() - MQTTIntervalTimer > mqtt_setting.interval * 1000 ) &&
        ((connected && check_wm_state_station()) || (sim_connected)) && mqttclient.connected()) {
        //Serial.println("start send mqtt");
        MQTT_Log();
        MQTTIntervalTimer = millis();
    }
    if ((connected && check_wm_state_station()) || (sim_connected) ) {
        mqttclient.loop();
    }

    

}