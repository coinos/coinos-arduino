/****************************************************************************************************************************
  RP2040W-Client_SINRIC.ino
  For RP2040W with CYW43439 WiFi.

  Based on and modified from Gil Maimon's ArduinoWebsockets library https://github.com/gilmaimon/ArduinoWebsockets
  to support STM32F/L/H/G/WB/MP1, nRF52, SAMD21/SAMD51, RP2040 boards besides ESP8266 and ESP32


  The library provides simple and easy interface for websockets (Client and Server).

  Built by Khoi Hoang https://github.com/khoih-prog/Websockets2_Generic
  Licensed under MIT license
 *****************************************************************************************************************************/
/****************************************************************************************************************************
  RP2040W Websockets SINRIC Client

  This sketch:
        1. Connects to a WiFi network
        2. Connects to a Websockets SINRIC server
        3. Control the LED_BUILTIN using SINRIC/Alexa commands

  Hardware:
        For this sketch you only need an RP2040W board.

*****************************************************************************************************************************/

#include "defines.h"

#define DEBUG_LOCAL   2

#define SINRIC_WEBSERVER          "iot.sinric.com"
#define SINRIC_WEBSERVER_PORT     80
#define SINRIC_API_KEY            "11111111-2222-3333-4444-555555555555"

#define SINRIC_Device_ID_1        "012345678901234567890123"   // Device ID, got from Sinric

const char* websockets_server_host    = SINRIC_WEBSERVER;       //Enter server address
const uint16_t websockets_server_port = SINRIC_WEBSERVER_PORT;  // Enter server port

#ifdef LED_BUILTIN
  #define LED_PIN     LED_BUILTIN
#else
  #define LED_PIN     13
#endif

int status = WL_IDLE_STATUS;

#include <WebSockets2_Generic.h>
#include <ArduinoJson.h>          // https://arduinojson.org/ or install via Arduino library manager

using namespace websockets2_generic;

WebsocketsClient client;

#define HEARTBEAT_INTERVAL      300000 // 5 Minutes 

bool isConnected                = false;
uint64_t heartbeatTimestamp     = 0;
uint64_t now                    = 0;

//To increase no of devices as needed
void turnOn(String deviceId)
{
  if (deviceId == SINRIC_Device_ID_1) // Device ID of 1st device
  {
#if (DEBUG_LOCAL > 1)
    Serial.print("Turn on device id: ");
    Serial.println(deviceId);
#endif
    digitalWrite(LED_PIN, HIGH);
  }
  else
  {
#if (DEBUG_LOCAL > 1)
    Serial.print("Turn on for unknown device id: ");
    Serial.println(deviceId);
#endif
  }
}

//To increase no of devices as needed
void turnOff(String deviceId)
{

  if (deviceId == SINRIC_Device_ID_1) // Device ID of 1st device
  {
#if (DEBUG_LOCAL > 1)
    Serial.print("Turn off Device ID: ");
    Serial.println(deviceId);
#endif
    digitalWrite(LED_PIN, LOW);
  }
  else
  {
#if (DEBUG_LOCAL > 1)
    Serial.print("Turn off for unknown device id: ");
    Serial.println(deviceId);
#endif
  }
}

void onEventsCallback(WebsocketsEvent event, String data)
{
  (void) data;

  if (event == WebsocketsEvent::ConnectionOpened)
  {
    if (!isConnected)
      isConnected = true;

    Serial.println("Connnection Opened");
  }
  else if (event == WebsocketsEvent::ConnectionClosed)
  {
    if (isConnected)
      isConnected = false;

    Serial.println("Connnection Closed");
  }
  else if (event == WebsocketsEvent::GotPing)
  {
    if (!isConnected)
      isConnected = true;

    Serial.println("Got a Ping!");
  }
  else if (event == WebsocketsEvent::GotPong)
  {
    if (!isConnected)
      isConnected = true;

    Serial.println("Got a Pong!");
  }
}

void onMessagesCallback(WebsocketsMessage message)
{
  String SINRIC_message = message.data();

  Serial.print("Got Message: ");
  Serial.println(SINRIC_message /*message.data()*/);

  // Example payloads

  // For Switch or Light device types
  // {"deviceId": xxxx, "action": "setPowerState", value: "ON"} // https://developer.amazon.com/docs/device-apis/alexa-powercontroller.html

  // For Light device type
  // Look at the light example in github

#if (ARDUINOJSON_VERSION_MAJOR >= 6)
  DynamicJsonDocument json(1024);
  //auto deserializeError = deserializeJson(json, (char*)message.data());
  auto deserializeError = deserializeJson(json, SINRIC_message);

  if ( deserializeError )
  {
    Serial.println("JSON parseObject() failed");
    return;
  }

  //serializeJson(json, Serial);
#else
  DynamicJsonBuffer jsonBuffer;
  // Parse JSON string
  JsonObject& json = jsonBuffer.parseObject(SINRIC_message);

  // Test if parsing succeeds.
  if (!json.success())
  {
    Serial.println("JSON parseObject() failed");
    return;
  }

#endif

  String deviceId = json ["deviceId"];
  String action = json ["action"];

  if (action == "setPowerState")
  {
    // Switch or Light
    String value = json ["value"];

    if (value == "ON")
    {
      turnOn(deviceId);
    }
    else
    {
      turnOff(deviceId);
    }
  }
}

void printWifiStatus()
{
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("Local IP Address: ");
  Serial.println(ip);
}

void setup()
{
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);

  Serial.begin(115200);

  while (!Serial && millis() < 5000);

  Serial.println("\nStarting RP2040W-Client_SINRIC on " + String(BOARD_NAME));
  Serial.println(WEBSOCKETS2_GENERIC_VERSION);

  ///////////////////////////////////

  // check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE)
  {
    Serial.println("Communication with WiFi module failed!");

    // don't continue
    while (true);
  }

  Serial.print(F("Connecting to SSID: "));
  Serial.println(ssid);

  status = WiFi.begin(ssid, pass);

  delay(1000);

  // attempt to connect to WiFi network
  while ( status != WL_CONNECTED)
  {
    delay(500);

    // Connect to WPA/WPA2 network
    status = WiFi.status();
  }

  printWifiStatus();

  ///////////////////////////////////

  Serial.print("Connecting to WebSockets Server @");
  Serial.println(websockets_server_host);

  // run callback when events are occuring
  client.onMessage(onMessagesCallback);

  // try to connect to Websockets server
  client.addHeader("apikey", SINRIC_API_KEY);
  client.setAuthorization("apikey", SINRIC_API_KEY);

  bool connected = client.connect(websockets_server_host, websockets_server_port, "/");

  if (connected)
  {
    Serial.println("Connected!");
  }
  else
  {
    Serial.println("Not Connected!");
  }
}

void loop()
{
  // let the websockets client check for incoming messages
  if (client.available())
  {
    client.poll();

    now = millis();

    // Send heartbeat in order to avoid disconnections during ISP resetting IPs over night. Thanks @MacSass
    if ((now - heartbeatTimestamp) > HEARTBEAT_INTERVAL)
    {
      heartbeatTimestamp = now;
      client.send("H");
    }
  }
}
