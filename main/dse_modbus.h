#ifndef _DSE_H_
#define _DSE_H_

#include <Arduino.h>
struct dse_modbus_data
{
    uint16_t oil_pressure;
    int16_t cool_temp;
    int16_t oil_temp;
    uint16_t fuel_level;
    float chg_volt;
    float bat_volt;
    uint16_t engine_speed;
    float m_freq;
    float volt[6];
    float current[4];
    int32_t watt[3];

};

void dse_task(void* parameter);
#endif
