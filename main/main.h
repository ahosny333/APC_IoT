#ifndef MAIN_H_
#define MAIN_H_

#define DEBUG  1
#ifdef DEBUG
#define DEBUG_PRINT(x) Serial.print(x); Serial.flush()
#define DEBUG_PRINTDEC(x) Serial.print(x, DEC);Serial.flush()
#define DEBUG_PRINTLN(x) Serial.println(x);Serial.flush()
#define DEBUG_PRINTLNDEC(x) Serial.println(x, DEC);Serial.flush()
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTDEC(x)
#define DEBUG_PRINTLN(x)
#define DEBUG_PRINTLNDEC(x)
#endif


#define check_wifi_period 1000
#define INITIAL_AP_TIME 60000
#define AP_MODE_PIN 21




#endif
