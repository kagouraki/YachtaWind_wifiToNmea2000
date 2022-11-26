#ifdef __IN_ECLIPSE__
//This is a automatic generated file
//Please do not modify this file
//If you touch this file your change will be overwritten during the next build
//This file has been generated on 2022-11-26 14:26:39

#include "Arduino.h"
#define ESP32_CAN_TX_PIN GPIO_NUM_21
#define ESP32_CAN_RX_PIN GPIO_NUM_22
#include <Arduino.h>
#include <NMEA2000_CAN.h>
#include <N2kMessages.h>
extern const unsigned long TransmitMessages[];
#include <WiFiManager.h>
#include <EEPROM.h>

void setup() ;
double ReadAirPressure() ;
double ReadAirTemp() ;
double ReadHumidity() ;
double ReadDewPoint() ;
double ReadWindAngle() ;
double ReadWindSpeed() ;
void SendN2kWind() ;
void checkButton() ;
String getParam(String name) ;
void switch_server() ;
void saveParamCallback() ;
String getValue(String data, char separator, int index) ;
void loop() ;

#include "WifiToNmea2000.ino"


#endif
