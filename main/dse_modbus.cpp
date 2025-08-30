#include <ModbusRTU.h>
#include <Arduino.h>
#include "main.h"
#include "dse_modbus.h"


#define SLAVE_ID 1

ModbusRTU dse_rtu;
uint16_t registers[70];
char modbus_string[1000];
struct dse_modbus_data dse_data;

enum dse_task_status { read_general=0,read_mains, write_command };
enum dse_task_status dse_state = read_general;
uint32_t dse_scan_timer = 0;
uint16_t dse_scan_interval = 5000; // 2 seconds
uint8_t dse_request_started = 0;

bool dse_modbus_success = false;

bool dse_cb(Modbus::ResultCode event, uint16_t transactionId, void* data) { 

  if (event != Modbus::EX_SUCCESS) {
    // Serial.print("Request result: 0x");
    // Serial.print(event, HEX);
    dse_request_started = 0;
    //dse_scan_timer = millis();
    dse_modbus_success = false;
    memset(modbus_string,0,sizeof(modbus_string));
    sprintf(modbus_string,"{\"status\":0,\"oil_p\":%d,\"cool_t\":%d,\"oil_t\":%d,\"fuel_l\":%d,\"cgh_v\":%.2f,\"bat_v\":%.2f,\"rpm\":%d}", dse_data.oil_pressure,
      dse_data.cool_temp,dse_data.oil_temp,
      dse_data.fuel_level, dse_data.chg_volt, dse_data.bat_volt, dse_data.engine_speed);
  }
  else{
    // Serial.print("success Request result: 0x");
    dse_modbus_success = true;
    if (dse_state == read_general){
      dse_data.oil_pressure = registers[0];
      dse_data.cool_temp = (int16_t) (registers[1]);
      dse_data.oil_temp = (int16_t) (registers[2]);
      dse_data.fuel_level = registers[3];
      dse_data.chg_volt = registers[4] * 0.1;
      dse_data.bat_volt = registers[5] * 0.1;
      dse_data.engine_speed = registers[6];
      dse_request_started = 0;
      dse_state = read_mains;
      //dse_scan_timer = millis();
                
    }
    else if (dse_state == read_mains){
      dse_data.m_freq = registers[0] * 0.1;
      for(int i=0;i<6;i++)
      {
        dse_data.volt[i] = (registers[(i*2)+2] | (registers[(i*2)+1])<<16) * 0.1;
      }
      for(int i=0;i<4;i++)
      {
        dse_data.current[i] = (registers[(i*2)+18] | (registers[(i*2)+17])<<16) * 0.1;
      }
      for(int i=0;i<3;i++)
      {
        dse_data.watt[i] = (int32_t)(registers[(i*2)+26] | (registers[(i*2)+25])<<16);
      }

      dse_request_started = 0;
      dse_state = read_general;
      dse_scan_timer = millis();
                
    }
    

    // memset(modbus_string,0,sizeof(modbus_string));
    // //sprintf(modbus_string,"{\"status\":1,\"oil_p\":%d,\"cool_t\":%d,\"oil_t\":%d,\"fuel_l\":%d,\"cgh_v\":%.2f,\"bat_v\":%.2f,\"rpm\":%d,\"f\":%.2f,\"v\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],\"a\":[%.2f,%.2f,%.2f,%.2f],\"w\":[%" PRId32 ",%" PRId32 ",%" PRId32"]}", 
    // sprintf(modbus_string,
    //     "{\"status\":1,\"oil_p\":%d,\"cool_t\":%d,\"oil_t\":%d,\"fuel_l\":%d,\"cgh_v\":%.2f,\"bat_v\":%.2f,\"rpm\":%d,\"f\":%.2f,"
    //     "\"v\":[%.2f,%.2f,%.2f,%.2f,%.2f,%.2f],"
    //     "\"a\":[%.2f,%.2f,%.2f,%.2f],"
    //     "\"w\":[%" PRId32 ",%" PRId32 ",%" PRId32 "]}",
    //   dse_data.oil_pressure,
    //   dse_data.cool_temp,dse_data.oil_temp,
    //   dse_data.fuel_level, dse_data.chg_volt, dse_data.bat_volt, dse_data.engine_speed,
    //   dse_data.m_freq,dse_data.volt[0],dse_data.volt[1],dse_data.volt[2],dse_data.volt[3],dse_data.volt[4],dse_data.volt[5],
    //   dse_data.current[0],dse_data.current[1],dse_data.current[2],dse_data.current[3],dse_data.watt[0],dse_data.watt[1],dse_data.watt[2]);

  }
  return true;



}


void dse_task(void* parameter)
{

    dse_rtu.begin(&Serial, RTU_DE_PIN, true);
    dse_rtu.setBaudrate(115200);
    dse_rtu.master();
    while(1)
    {
      if(dse_state == write_command && dse_request_started == 0 && !dse_rtu.slave()) 
      {
        dse_request_started = 1;

      }
      else{
        if(millis() - dse_scan_timer >= dse_scan_interval )
        {
          switch (dse_state)
          {
          case read_general:
            if(dse_request_started == 0 && !dse_rtu.slave()) 
            {
              dse_request_started = 1;
              dse_rtu.readHreg(SLAVE_ID, 1024, registers,7, dse_cb);

            }
            break;
          case read_mains:
            if(dse_request_started == 0 && !dse_rtu.slave()) 
            {
              dse_request_started = 1;
              dse_rtu.readHreg(SLAVE_ID, 1059, registers,31, dse_cb);

            }
            break;
          
          default:
            break;
          }
        }
      }



      // if (!dse_rtu.slave()) {
      //   dse_rtu.readHreg(SLAVE_ID, 1024, registers,7, dse_cb);
      // }






      dse_rtu.task();
      delay(1000);

    }

}


