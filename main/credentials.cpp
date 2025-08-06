#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <nvs_flash.h>
#include "nvs.h"
#include "credentials.h"
#include "main.h"
#include <Arduino.h>
#include "embedded_wifimanager.h"
#include "mqtt.h"

char user_token[10];
char app_id[10];
extern uint8_t station_mode,go_station;
extern wifi_settings_t device_wifi_settings;
extern struct mqtt_set mqtt_setting;

void readSystemVariables() {
    //  Clear MQQT
    memset((void *)&mqtt_setting, 0, sizeof(mqtt_setting));

    nvs_handle my_handle;
    //printf("\nOpening Non-Volatile Storage (NVS) handle... for read \n");
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle! for read \n", esp_err_to_name(err));
    }
    else
    {
        size_t size;
        wifi_settings_t temp_device_wifi_settings = {0};
        uint8_t temp_station_mode;
        err= nvs_get_u8(my_handle, "station_mode", &temp_station_mode);
        if(err != ESP_OK)
            {station_mode = 0;}
        else
            {
            station_mode = temp_station_mode;
            go_station = temp_station_mode;
            }
        
        size = sizeof(user_token);
        err = nvs_get_str(my_handle, "user_token", user_token, &size);
        if(err != ESP_OK)
            {sprintf(user_token,"no_token");}

        size = sizeof(app_id);
        err = nvs_get_str(my_handle, "app_id", app_id, &size);
        if(err != ESP_OK)
            {sprintf(app_id,"ap_mode");}

        size = sizeof(temp_device_wifi_settings);
        err = nvs_get_blob(my_handle, "wifi_settings", (void *)&temp_device_wifi_settings, &size);
        if(err == ESP_OK)
        {
            memcpy((void *)&device_wifi_settings, (void *)&temp_device_wifi_settings,sizeof(device_wifi_settings));
        }

        struct mqtt_set temp_mqtt_setting;
        size = sizeof(temp_mqtt_setting);
        err = nvs_get_blob(my_handle, "mqtt_settings", (void *)&temp_mqtt_setting, &size);
        if(err == ESP_OK)
        {
            memcpy((void *)&mqtt_setting, (void *)&temp_mqtt_setting,sizeof(mqtt_setting));
        }
        // Serial.println(device_wifi_settings.ssid);
        // Serial.println(device_wifi_settings.password);
            

        // Close
        nvs_close(my_handle);

    }
   
}

void saveSystemVariables() {
    nvs_handle my_handle;
    //printf("\nOpening Non-Volatile Storage (NVS) handle... for write\n");
    esp_err_t err = nvs_open("storage", NVS_READWRITE, &my_handle);
    if (err != ESP_OK)
    {
        printf("Error (%s) opening NVS handle! for write\n", esp_err_to_name(err));
    }
    else
    {
      size_t size;
      err = nvs_set_u8(my_handle, "station_mode", go_station);
      err = nvs_set_str(my_handle, "user_token", user_token);
      err = nvs_set_str(my_handle, "app_id", app_id); 
      size = sizeof(device_wifi_settings);
      err = nvs_set_blob(my_handle, "wifi_settings", (void *)&device_wifi_settings,size); 
      size = sizeof(mqtt_setting);
      err = nvs_set_blob(my_handle, "mqtt_settings", (void *)&mqtt_setting,size);

      err = nvs_commit(my_handle);
      nvs_close(my_handle);
    }

}