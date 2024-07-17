#ifndef _MAIN_H_
#define _MAIN_H_
#include "statusled.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_wps.h>
#include <esp_dmx.h>

void ADCTaskFunc (void * p);
void DMXTaskFunc (void * p);
void LEDTaskFunc (void * p);
void sendHTTPResponse(WiFiClient client, String resourceRequested);
void handleResponseLED(String action);
void handleResponseDMX(String action);
String readHTTPResponse(String line);

enum class dmxsemaphore_t{free, busy, updated};

#endif