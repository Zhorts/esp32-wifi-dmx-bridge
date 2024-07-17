/*
ESP32 Wifi DMX Bridge - based on sample projects cobbled together and expanded...
 */
#include "main.h"

#define ESP_WPS_MODE      WPS_TYPE_PBC
#define ESP_MANUFACTURER  "ESPRESSIF"
#define ESP_MODEL_NUMBER  "ESP32"
#define ESP_MODEL_NAME    "Olimex board"
#define ESP_DEVICE_NAME   "DMX Remote"

static esp_wps_config_t config;

// Server stuff
WiFiServer server(80);
const char* ssid     = "";
const char* password = "";

// OS stuff
TaskHandle_t ADCTask, LEDTask, DMXTask;

// Pin definitions
#define LED 2
#define LED_G 18
#define LED_B 19
#define LED_R 21
#define VBAT 35
#define VEXT 39
#define DMX_RX 16
#define DMX_TX 17
#define DMX_EN 22

//LEDs
StatusLed leds(LED_G, LED_B, LED_R);

// ADC variables
int ADCArray[60];
int ADCArraySize = sizeof(ADCArray) / sizeof(ADCArray[0]);
int ADCAverage = 0;
int vbatval = 0;
int vextval = 0;
int temp = 0;

// DMX stuff
#define DMXArraySize 512
byte DMXArray[DMXArraySize];
volatile dmxsemaphore_t DMXupdate = dmxsemaphore_t::free; // Semaphore to allow usage of DMXArray

int transmitPin = DMX_TX;
int receivePin = DMX_RX;
int enablePin = DMX_EN;
dmx_port_t dmxPort = 1;
byte DMXdata[DMX_PACKET_SIZE];


void wpsInitConfig(){
  config.wps_type = ESP_WPS_MODE;
  strcpy(config.factory_info.manufacturer, ESP_MANUFACTURER);
  strcpy(config.factory_info.model_number, ESP_MODEL_NUMBER);
  strcpy(config.factory_info.model_name, ESP_MODEL_NAME);
  strcpy(config.factory_info.device_name, ESP_DEVICE_NAME);
}

void wpsStart(){
    if(esp_wifi_wps_enable(&config)){
    	Serial.println("WPS Enable Failed");
    } else if(esp_wifi_wps_start(0)){
    	Serial.println("WPS Start Failed");
    }
}

void wpsStop(){
    if(esp_wifi_wps_disable()){
    	Serial.println("WPS Disable Failed");
    }
}

String wpspin2string(uint8_t a[]){
  char wps_pin[9];
  for(int i=0;i<8;i++){
    wps_pin[i] = a[i];
  }
  wps_pin[8] = '\0';
  return (String)wps_pin;
}

void WiFiEvent(WiFiEvent_t event, arduino_event_info_t info){
  switch(event){
    case ARDUINO_EVENT_WIFI_STA_START:
      //leds.off();
      //leds.flash(LED_G, 500, 500);
      Serial.println("Station Mode Started");
      break;
    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      leds.off();
      leds.on(LED_G);
      Serial.println("Connected to :" + String(WiFi.SSID()));
      Serial.print("Got IP: ");
      Serial.println(WiFi.localIP());
      break;
    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      leds.off();
      leds.flash(LED_R, 250, 250);
      Serial.println("Disconnected from station, attempting reconnection");
      WiFi.reconnect();
      break;
    case ARDUINO_EVENT_WPS_ER_SUCCESS:
      leds.off();
      leds.on(LED_B);
      Serial.println("WPS Successfull, stopping WPS and connecting to: " + String(WiFi.SSID()));
      wpsStop();
      delay(10);
      WiFi.begin();
      break;
    case ARDUINO_EVENT_WPS_ER_FAILED:
      Serial.println("WPS Failed, retrying");
      wpsStop();
      wpsStart();
      break;
    case ARDUINO_EVENT_WPS_ER_TIMEOUT:
      leds.off();
      leds.flash(LED_B, 250, 250);
      Serial.println("WPS Timedout, retrying");
      wpsStop();
      wpsStart();
      break;
    case ARDUINO_EVENT_WPS_ER_PIN:
      Serial.println("WPS_PIN = " + wpspin2string(info.wps_er_pin.pin_code));
      break;
    default:
      break;
  }
}

void setup()
{
  Serial.begin(115200);
  pinMode(LED, OUTPUT);      // set the LED pin mode
  pinMode(LED_R, OUTPUT);
  pinMode(LED_G, OUTPUT);
  pinMode(LED_B, OUTPUT);
  pinMode(VBAT, INPUT);
  pinMode(VEXT, INPUT);
  
  // DMX setup
  memset(DMXArray, 0, DMXArraySize);
  memset(DMXdata, 0, DMX_PACKET_SIZE);

  dmx_config_t DMXconfig = DMX_CONFIG_DEFAULT;
  dmx_personality_t personalities[] = {};
  int personality_count = 0;
  dmx_driver_install(dmxPort, &DMXconfig, personalities, personality_count);

  // Set DMX hardware pins
  dmx_set_pin(dmxPort, transmitPin, receivePin, enablePin);

  // All done, wait a bit to let things settle for no particular reason.
  delay(1000);

  //xTaskCreatePinnedToCore(ADCTaskFunc, "ADC Task", 1000, NULL, 1, &ADCTask, 0);
  xTaskCreatePinnedToCore(DMXTaskFunc, "DMX Task", 1000, NULL, 1, &DMXTask, 0);
  xTaskCreatePinnedToCore(LEDTaskFunc, "LED Task", 1000, NULL, 2, &LEDTask, 0);
  delay(100);

  /*leds.flash(LED_B, 250, 250);
  delay(4000);
  leds.on(LED_R);
  delay(2000);
  leds.off(LED_B);
  delay(2000);
  leds.off(LED_R);
  leds.on(LED_G);
  delay(2000);
  leds.off(LED_G);
  delay(2000);*/

  // We start by connecting to a WiFi network

  Serial.println();
  Serial.println();
  Serial.print("Connecting to wifi...");
  //Serial.println(ssid);

  WiFi.onEvent(WiFiEvent);
  WiFi.mode(WIFI_MODE_STA);
  Serial.println("Starting WPS");
  leds.flash(LED_B, 500, 500);
  wpsInitConfig();
  wpsStart();

  //WiFi.begin(ssid, password);

  Serial.println();
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  
  Serial.println("");
  Serial.println("WiFi connected.");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  leds.off();
  leds.on(LED_G);
//    digitalWrite(LED_G, HIGH);
  
  server.begin();

}

int value = 0;

String voltageMonitorToString() {
  vbatval = analogReadMilliVolts(VBAT) << 1;

  temp = analogReadMilliVolts(VEXT);
  vextval = temp + (temp >> 1);

  String s = String("VBat: ");
  s += vbatval;
  s += String(" mV   VExt: ");
  s += vextval;
  s += String(" mV.");

  return s;
}
String voltageMonitorTableToString() {
  String s = "<table>";
  for (int i = 0; i < ADCArraySize; i++) {
    s += "<tr><td>";
    s += ADCArray[i];
    s += "</td></tr>";
  }
  s += "</table>";
  return s;
}
String dmxDataToTableString() {
  String s = "<table>";
  for (int i = 0; i < DMXArraySize; i++) {
    s += "<tr><td>";
    s += i;
    s += "</td><td>";
    s += String((DMXArray[i]), DEC);
    s += "</td><td>";
    s += "<a href=\"DMX?set=";
    s += i;
    s += ",0\">0</a>";
    s += "</td><td>";
    s += "<a href=\"DMX?set=";
    s += i;
    s += ",255\">255</a>";
    s += "</td></tr>";
  }
  s += "</table>";
  return s;
}
bool validateInputPureInt(String s) {
  const char * a = s.c_str();
  bool ok = true;
  while (*a != '\0') {
    if(!isDigit(*a)) {
      ok = false;
      //Serial.println("Address not an int");
      break;
    }
    a++;
  }
  return ok;
}

void loop(){
WiFiClient client = server.available();   // listen for incoming clients

  if (client) {                             // if you get a client,
    Serial.println("New Client.");          // print a message out the serial port
    String currentLine = "";                // make a String to hold incoming data from the client
    String resourceRequested = "";          // The Resource requested by the client (if any)
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        Serial.write(c);                    // print it out the serial monitor
        if (c == '\n') {                    // on newline, we have a full line of information to process from the client
          // if the current line is also blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) { // No other characters were added to currentLine, so client has gone \n\n - end of request
            sendHTTPResponse(client, resourceRequested); // Send HTTP response to client

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, check if there's anything to handle and then then clear currentLine:
            String s = readHTTPResponse(currentLine);
            if (s != "") resourceRequested = s;
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

      }
    }
    // close the connection:
    client.stop();
    Serial.println("Client Disconnected.");
  }
}
String readHTTPResponse(String line) {
  String resource = "";
  
  //Serial.println("Checking line: " + line);
  if (line.startsWith("GET ")) { // Lines starting with GET are requests from the client, we should do something
    //Serial.println("Found GET ");
    String s = line.substring(4); // Trim off the "GET " off the start - put this in a new String in case someone else wants to check the same line for something else... maybe?
    //Serial.println("Trimmed string: " + s);
    int firstspace = s.indexOf(" "); // Find the first space - that will be the delimeter to any other junk the client is sending (like HTTP version)
    if (firstspace > 0) {
      s = s.substring(0, firstspace); // s will now contain only the interesting request part
      Serial.println("Chopped string: " + s);
      int questionmark = s.indexOf("?"); // ? is used to separate resource from action - IP/resource?index=value&index2=value2
      if (questionmark < 1) { // If there is no action, the whole string is interesting for resource check. Start looking at index 1, because index 0 will be the first / after the IP address
        questionmark = s.length();
      }
      resource = s.substring(1, questionmark); // Get only the Resource part of the URL, offset to 1 because of the first mandatory / after the IP address
      Serial.println("Resource: " + resource);
      String action = s.substring(questionmark + 1, s.length());
      if (resource.equalsIgnoreCase("LED")) { // Request for LED stuff
        handleResponseLED(action);        
      } else if (resource.equalsIgnoreCase("DMX")) { // Request for DMX stuff
        handleResponseDMX(action);
      } else { // Unknown request.. probably do nothing here? But will still tell request handler the resource requested
        NOP();
      }

    }
  }
  return (resource);
}

void handleResponseLED(String action) {
  int equalsign = action.indexOf("=");
  while (equalsign >= 0)
  {
    int ampersand = action.indexOf("&"); // Ampersand will delimit the end of this index value pair
    // If there is no ampersand, we want to consider the whole remaining string
    int endpoint = action.length(); // Default to use the whole string
    if (ampersand >= 0)
    { // Ampersand found, stop at it instead
      endpoint = ampersand;
    }
    String idx = action.substring(0, equalsign);
    String val = action.substring(equalsign + 1, endpoint);
    //Serial.println("Index: " + idx);
    //Serial.println("Value: " + val);

    if (idx.equalsIgnoreCase("set"))
    {
      if (val.equalsIgnoreCase("on"))
      {
        digitalWrite(LED, HIGH);
      }
      if (val.equalsIgnoreCase("off"))
      {
        digitalWrite(LED, LOW);
      }
    }

    // Step up to the next index=value pair
    equalsign = -1; // Assume no equal sign found for it yet... (also breaks while loop if no ampersand found)
    if (ampersand >= 0)
    {
      action = action.substring(ampersand + 1, action.length()); // Trim down action string by one pair
      equalsign = action.indexOf("=");
      //Serial.println("Next pair to be shaved!");
    }
  }
}
void handleResponseDMX(String action) {
  int timeout = 1000;
  Serial.println("Checking DMXArray Semaphore");
  while (DMXupdate != dmxsemaphore_t::free && timeout > 0) {
    // Someone else is modifying the DMXArray, so we shouldn't interfere. Wait until it's free, or until we get a timeout
    vTaskDelay(1);
    timeout--;
    Serial.print(".");
  }
  if (timeout <= 0) {
    Serial.println("!");
    Serial.println("DMX response handle timed out!");
    Serial.println("!");
    return;
  }
  // Acquire semaphore - we're might be changing stuff, so don't let others in...
  DMXupdate = dmxsemaphore_t::busy;

  bool updated = false;
  int equalsign = action.indexOf("=");
  while (equalsign >= 0)
  {
    int ampersand = action.indexOf("&"); // Ampersand will delimit the end of this index value pair
    // If there is no ampersand, we want to consider the whole remaining string
    int endpoint = action.length(); // Default to use the whole string
    if (ampersand >= 0)
    { // Ampersand found, stop at it instead
      endpoint = ampersand;
    }
    String idx = action.substring(0, equalsign);
    String val = action.substring(equalsign + 1, endpoint);
    //Serial.println("Index: " + idx);
    //Serial.println("Value: " + val);

    if (idx.equalsIgnoreCase("set"))
    {
      // For DMX set, we expect an address value pair, each of which needs to be a byte (0-255), comma-separated
      int comma = val.indexOf(",");
      if (comma > 0) { // Must have at least one character as address anyway, so it's OK to check for strict greater than zero rather than inclusive
        String address = val.substring(0, comma);
        String data = val.substring(comma + 1, val.length());
        if(validateInputPureInt(address) && validateInputPureInt(data)) { // Check that address and data are clean integers (that we can do toInt() on them without silly 0 side-effects)
          //Serial.println("Address and data are both OK");
          int address_int = address.toInt();
          int data_int = data.toInt();
          if (address_int >= 0 && address_int < 512 && data_int >= 0 && data_int < 256) { // Only act on byte values, silently discard any other silly attempts
            //Serial.println("DMXArray Update:");
            //Serial.print(" Address: ");
            //Serial.print(address_int);
            //Serial.print(" Data: ");
            //Serial.print(data_int);
            //Serial.print("\n");
            DMXArray[address_int] = data_int;
            updated = true; // We've updated at least one data point in the array, so this will allow us to let the semaphore know once we're done with all updates
          }
        }
      }
    }

    // Step up to the next index=value pair
    equalsign = -1; // Assume no equal sign found for it yet... (also breaks while loop if no ampersand found)
    if (ampersand >= 0)
    {
      action = action.substring(ampersand + 1, action.length()); // Trim down action string by one pair
      equalsign = action.indexOf("=");
      //Serial.println("Next pair to be shaved!");
    }
  }
  // Handle semaphore - did we modify anything?
  if (updated) {
    DMXupdate = dmxsemaphore_t::updated;
    //Serial.println("Leaving DMX Handle. Semaphore set to updated.");
  } else {
    DMXupdate = dmxsemaphore_t::free;
    //Serial.println("Leaving DMX Handle. Semaphore set to free.");
  }
}
void sendHTTPResponse(WiFiClient client, String resource) {
  // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
  // and a content-type so the client knows what's coming, then a blank line:
  client.println("HTTP/1.1 200 OK");
  client.println("Content-type:text/html");
  client.println();

  // the content of the HTTP response follows the header:
  client.print("Click <a href=\"/LED?set=on\">here</a> to turn the LED on pin 2 on.<br>");
  client.print("Click <a href=\"/LED?set=off\">here</a> to turn the LED on pin 2 off.<br>");
  client.print("Click <a href=\"/DMX\">here</a> to see DMX data.<br>");
  //client.print("Click <a href=\"/ADC\">here</a> to check ADC voltage readings");
  if (resource.equalsIgnoreCase("ADC")) { // Analog voltage monitor page
    client.print(voltageMonitorToString());
    client.print("<br>");
    client.print("Average VBat voltage: <i>");
    client.print(ADCAverage);
    client.print("</i> mV.<br><br>");
    client.print("Last 60 readings: <br>");
    client.print(voltageMonitorTableToString());
  } else if (resource.equalsIgnoreCase("LED")) { // LED page

  } else if (resource.equalsIgnoreCase("DMX")) { // DMX page
    client.print(dmxDataToTableString());

  } else { // Default page

  }

}
void ADCTaskFunc (void * p) {
  Serial.println("ADC Task is running");
  //Serial.print("Running on core: ");
  //Serial.println(xPortGetCoreID());

  unsigned long currentMillis = millis();
  unsigned long prevMillis = currentMillis;
  const unsigned long interval = 1000; 
  int vbatval = 0;
  int ADCArrayNext = 0;

  for(int i = 0; i < ADCArraySize; i++) {
    ADCArray[i] = 0;
  }

  while(true) {
    currentMillis = millis();
    if (currentMillis - prevMillis >= interval) {
      //Serial.println("Doing a new ADC measurement");
      // Read battery voltage and convert to actual mV (50/50 voltage divider)
      vbatval = analogReadMilliVolts(VBAT) << 1;

      // Store it in the next location of the storage array
      ADCArray[ADCArrayNext] = vbatval;
      ADCArrayNext++;
      if (ADCArrayNext > ADCArraySize) {
        ADCArrayNext = 0;
      }

      // Calculate average
      int sum = 0;
      for (int i = 0; i < ADCArraySize; i++) {
        sum += ADCArray[i];
      }
      ADCAverage = sum / ADCArraySize;

      prevMillis = currentMillis;
    }

    vTaskDelay(1);
  }
  
}
void LEDTaskFunc (void * p) {
  Serial.println("LED Task is running");
  while(true) {
    leds.tick();
    vTaskDelay(1);
  }
  
}
void DMXTaskFunc (void * p ) {
  //unsigned long lastrun = millis();
  Serial.println("DMX Task is running");
  while(true) {
    if(DMXupdate == dmxsemaphore_t::updated) {
      //Serial.println("DMXTask found semaphore set to Updated. Will now copy memory.");
      memcpy(&DMXdata[1], DMXArray, DMXArraySize); // Copy to DMXdata + 1, because DMXdata[0] contains magic
      dmx_write(dmxPort, DMXdata, DMX_PACKET_SIZE);
      DMXupdate = dmxsemaphore_t::free; // Unset semaphore
      //Serial.println("DMXTask set semaphore back to Free.");
    }
    //Serial.println("DMX Task: Sending packet.");
    dmx_send_num(dmxPort, DMX_PACKET_SIZE);
    //Serial.println("DMX Task: Packet sent to driver.");
    dmx_wait_sent(dmxPort, DMX_TIMEOUT_TICK);
    //Serial.println("DMX Task: Driver reports packet sent.");
    vTaskDelay(1);
    //while (millis() - lastrun < 1000) {
    //  NOP();
    //}
    //lastrun = millis();
    //Serial.print("-");
  }
}