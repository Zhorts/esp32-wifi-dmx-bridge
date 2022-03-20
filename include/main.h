#ifndef _MAIN_H_
#define _MAIN_H_
#include "statusled.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wps.h>

void ADCTaskFunc (void * p);
void LEDTaskFunc (void * p);
void sendHTTPResponse(WiFiClient client, String resourceRequested);
void handleResponseLED(String action);
void handleResponseDMX(String action);
String readHTTPResponse(String line);

#endif