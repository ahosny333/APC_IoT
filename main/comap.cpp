#include <ModbusRTU.h>
#include <Arduino.h>
#include "main.h"
#include "dse_modbus.h"

#ifdef COMAP
extern ModbusRTU dse_rtu;
extern uint16_t registers[70];
extern uint16_t write_registers [10];
extern char modbus_string[1000];
extern struct dse_modbus_data dse_data;
extern bool dse_modbus_success;
extern uint16_t dse_scan_interval;
extern uint32_t dse_scan_timer;
extern uint8_t dse_request_started;
extern bool start_write;
extern char command_name[30];
extern uint8_t command_value;

enum comap_task_status { read_general=0,read_mode, write_command };
enum comap_task_status comap_state = read_general;
enum comap_task_status last_comap_state = read_general;


void comap_get_values()
{
  dse_data.engine_speed = (uint16_t)(registers[0]);
  dse_data.watt[0] = (int16_t)(registers[2]);
  dse_data.watt[1] = (int16_t)(registers[3]);
  dse_data.watt[2] = (int16_t)(registers[4]);
  dse_data.m_freq = ((uint16_t)(registers[17]))/10;
  dse_data.volt[0] = (uint16_t)(registers[18]);
  dse_data.volt[1] = (uint16_t)(registers[19]);
  dse_data.volt[2] = (uint16_t)(registers[20]);
  dse_data.volt[3] = (uint16_t)(registers[21]);
  dse_data.volt[4] = (uint16_t)(registers[22]);
  dse_data.volt[5] = (uint16_t)(registers[23]);
  dse_data.current[0] = (uint16_t)(registers[24]);
  dse_data.current[1] = (uint16_t)(registers[25]);
  dse_data.current[2] = (uint16_t)(registers[26]);
  dse_data.bat_volt = ((uint16_t)(registers[36]))/10;
  dse_data.oil_pressure = (uint16_t)(registers[38]);
  dse_data.cool_temp = (int16_t)(registers[39]);
  dse_data.fuel_level = (uint16_t)(registers[40]);




}

bool comap_cb(Modbus::ResultCode event, uint16_t transactionId, void* data) { 

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
    if (comap_state == read_general){
      comap_get_values();
      dse_request_started = 0;
      comap_state = read_mode;
    }
     else if (comap_state == read_mode){
      if(registers[0] ==0x02FE )
      {dse_data.control_mode = 0;}
      else if(registers[0] == 0x01FF)
      {dse_data.control_mode = 1;}
      dse_request_started = 0;
      comap_state = read_general;
      dse_scan_timer = millis();
    }

  }
  if(comap_state == write_command)
  {
    dse_request_started = 0;
    comap_state = last_comap_state;
  }
  return true;

}


void comap_read_modbus()
{
  if(start_write && dse_request_started == 0 && !dse_rtu.slave()) 
  {
      start_write = false;
      if((strcmp(command_name,"Stop mode") == 0) && command_value == 1) {
          write_registers[0] =  0x02FD;
          write_registers[1] = 0x00;
          write_registers[2] = 0x01;
          last_comap_state = comap_state;
          comap_state = write_command;
        }
        else if((strcmp(command_name,"Auto mode") == 0) && command_value == 1) {
          write_registers[0] =  0x01FE;
          write_registers[1] = 0x00;
          write_registers[2] = 0x01;
          last_comap_state = comap_state;
          comap_state = write_command;
        }
        if(comap_state == write_command)
        {
          dse_request_started = 1;
          // DEBUG_PRINTLN("Write state");
          // DEBUG_PRINTLN(command_name);
          // DEBUG_PRINTLN(command_value);
          dse_rtu.writeHreg(SLAVE_ID,4207,write_registers,3,comap_cb);
        }


  }
  else{
    
    if(millis() - dse_scan_timer >= dse_scan_interval )
    {
      switch (comap_state)
          {
            case read_general:
              if(dse_request_started == 0 && !dse_rtu.slave()) 
              {
                dse_request_started = 1;
                dse_rtu.readHreg(SLAVE_ID, 1000, registers,41, comap_cb);

              }
              break;

            case read_mode:
              if(dse_request_started == 0 && !dse_rtu.slave()) 
              {
                dse_request_started = 1;
                dse_rtu.readHreg(SLAVE_ID, 4208, registers,1, comap_cb);

              }
              break;
                    
            default:
              break;
          }
    }
  }
}







//     if(dse_request_started == 0 && !dse_rtu.slave()){ 
//       dse_request_started =1;
//       dse_rtu.readHreg(SLAVE_ID, 1000, registers,41, comap_cb);
//       //dse_scan_timer = millis();
//     }
//   }


// }
#endif

