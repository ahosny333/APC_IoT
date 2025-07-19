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

char user_token[10];
char app_id[10];
extern uint8_t station_mode,go_station;

void readSystemVariables() {

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
      err = nvs_set_u8(my_handle, "station_mode", go_station);
      err = nvs_set_str(my_handle, "user_token", user_token);
      err = nvs_set_str(my_handle, "app_id", app_id);  

      err = nvs_commit(my_handle);
      nvs_close(my_handle);
    }

}