#include <ESP8266WiFi.h>
#include "src/iotc/common/string_buffer.h"
#include "src/iotc/iotc.h"
#include <ArduinoJson.h>
#include <SoftwareSerial.h>
#include "SafeString.h"


#define WIFI_SSID "<ENTER WIFI SSID HERE>"
#define WIFI_PASSWORD "<ENTER WIFI PASSWORD HERE>"

const char *SCOPE_ID = "<ENTER SCOPE ID HERE>";
const char *DEVICE_ID = "<ENTER DEVICE ID HERE>";
const char *DEVICE_KEY = "<ENTER DEVICE primary/secondary KEY HERE>";

SoftwareSerial NodeMCU(D7, D8);
DynamicJsonDocument telemetryDoc(384);
DynamicJsonDocument propertyDoc(384);
String telemetryJsonMessage = "";
String propertyJsonMessage = "";
char messageBuffer[256];
int bufferIndex = 0;

void on_event(IOTContext ctx, IOTCallbackInfo *callbackInfo);
#include "src/connection.h"

void on_event(IOTContext ctx, IOTCallbackInfo *callbackInfo)
{
  // ConnectionStatus
  if (strcmp(callbackInfo->eventName, "ConnectionStatus") == 0)
  {
    LOG_VERBOSE("Is connected ? %s (%d)",
                callbackInfo->statusCode == IOTC_CONNECTION_OK ? "YES" : "NO",
                callbackInfo->statusCode);
    isConnected = callbackInfo->statusCode == IOTC_CONNECTION_OK;
    return;
  }

  // payload buffer doesn't have a null ending.
  // add null ending in another buffer before print
  AzureIOT::StringBuffer buffer;
  if (callbackInfo->payloadLength > 0)
  {
    buffer.initialize(callbackInfo->payload, callbackInfo->payloadLength);
  }

  LOG_VERBOSE("- [%s] event was received. Payload => %s\n",
              callbackInfo->eventName, buffer.getLength() ? *buffer : "EMPTY");

  if (strcmp(callbackInfo->eventName, "Command") == 0)
  {
    LOG_VERBOSE("- Command name was => %s\r\n", callbackInfo->tag);
  }
}

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
    
  Serial.begin(9600);

  NodeMCU.begin(115200);

  pinMode(D7, INPUT);
  pinMode(D8, OUTPUT);

  connect_wifi(WIFI_SSID, WIFI_PASSWORD);
  connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);

  if (context != NULL)
  {
    lastTick = 0; // set timer in the past to enable first telemetry a.s.a.p
  }

  memset(messageBuffer, 0, sizeof(messageBuffer));
  messageBuffer[0] = 0;
}

void loop()
{
digitalWrite(LED_BUILTIN, HIGH);   // turn the LED on (HIGH is the voltage level)
  
  /*
    if (isConnected)
    {
    //  pulse = 0;
      interrupts();
      delay(2000);
      noInterrupts();

      char msg[64] = {0};
      int pos = 0, errorCode = 0;

      pos = snprintf(msg, sizeof(msg) - 1, "{\"energy\": %f}",
                     10);
      errorCode = iotc_send_telemetry(context, msg, pos);
      msg[pos] = 0;

      if (errorCode != 0)
      {
          LOG_ERROR("Sending message has failed with error code %d", errorCode);
      }
      iotc_do_work(context);
    }
    else
    {
      iotc_free_context(context);
      context = NULL;
      connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
    }
  */
  String content = "";

  createSafeString(sfContent, 1024, "");
  char character;

  if (isConnected)
  {

    while (NodeMCU.available())
    {

      character = NodeMCU.read();
      // content.concat(character);

      if (bufferIndex < sizeof(messageBuffer) - 1)
      {
        if (character != '\t')
          messageBuffer[bufferIndex++] = character ;
        else
        {
          Serial.println("Message Received");
          sfContent = messageBuffer;
          //    cSFP(sfContent,messageBuffer);
          //   content = String(messageBuffer);
          messageBuffer[0] = 0;
          memset(messageBuffer, 0, sizeof(messageBuffer));
          bufferIndex = 0;
        }
      }
      else
      {
        bufferIndex = 0;
        memset(messageBuffer, 0, sizeof(messageBuffer));
        messageBuffer[0] = 0;
      }

    }

    //  Serial.println(messageBuffer);

    //  Serial.println("Content " + content);

    if (sfContent != "")
    {
      // Serial.println(content);
      ParseSafeMessage(sfContent);
      telemetryJsonMessage = "";
      propertyJsonMessage = "";

      if (telemetryDoc.size() > 0)
        serializeJson(telemetryDoc, telemetryJsonMessage);

      if (propertyDoc.size() > 0)
        serializeJson(propertyDoc, propertyJsonMessage);

      //   Serial.println(jsonMessage);
    }

    unsigned long ms = millis();
    if (ms - lastTick > 30000)
    { // send telemetry every 10 seconds
      int errorCode = 0;
      lastTick = ms;

      if (telemetryJsonMessage.length() > 0)
      {

        errorCode = iotc_send_telemetry(context, telemetryJsonMessage.c_str(), telemetryJsonMessage.length());

        if (errorCode != 0)
        {
          LOG_ERROR("Sending message has failed with error code %d", errorCode);
        }
      }

      if (propertyJsonMessage.length() > 0)
      {

        errorCode = iotc_send_property(context, propertyJsonMessage.c_str(), propertyJsonMessage.length());

        if (errorCode != 0)
        {
          LOG_ERROR("Sending message has failed with error code %d", errorCode);
        }
      }
    }

    iotc_do_work(context); // do background work for iotc
  }
  else
  {
    iotc_free_context(context);
    context = NULL;
    connect_client(SCOPE_ID, DEVICE_ID, DEVICE_KEY);
  }

   digitalWrite(LED_BUILTIN, LOW);
}

void ParseSafeMessage(SafeString& message)
{
  telemetryDoc.clear();
  propertyDoc.clear();

  Serial.println("PARSE SAFE");
  Serial.println(message);

  createSafeString(messageRow, 140, "");
  createSafeString(keyToken, 20);
  createSafeString(valueToken, 20);

  size_t nextIdx = 0;
  //  nextIdx = message.stoken(messageRow, nextIdx, "#");
  //  nextIdx++; //step over delimiter
  //  nextIdx = appCmd.stoken(token2, nextIdx, ";}");


  while (message.nextToken(messageRow, "#")) {
    nextIdx = 0;
    Serial.println(messageRow);

    nextIdx = messageRow.stoken(keyToken, nextIdx, ":", true);
    nextIdx = messageRow.stoken(valueToken, nextIdx, ":", true);

    Serial.println(keyToken);
    Serial.println(valueToken);

    char messageType = keyToken[0];

    keyToken.substring(keyToken, 1);

    String keystr = keyToken.c_str();
    String valuestr = valueToken.c_str();

    if (messageType == 'T')
    {
      telemetryDoc[ keystr] = valuestr;
    }
    else if (messageType == 'P')
    {
      propertyDoc[keystr] = valuestr;
    }


  }

}


void ParseMessage(String message)
{
  telemetryDoc.clear();
  propertyDoc.clear();

  Serial.println("PARSE");

  Serial.println(message);

  char *str = new char[message.length() + 1];
  strcpy(str, message.c_str());

  Serial.println(str);

  //  const char messageDelimeter[2] = "#";
  //  const char valueDelimeter[2] = ":";
  char *messageDelimiterToken;

  char *valueDelimiterToken;
  char *keyDelimiterToken;
  char jsonKey[100] = {0};
  char keybuffer[100] = {0};

  messageDelimiterToken = strtok_r(str, "#", &str);

  while (messageDelimiterToken != NULL)
  {
    //  Serial.println( (char*) messageDelimiterToken );

    keyDelimiterToken = strtok_r(messageDelimiterToken, ":", &messageDelimiterToken);
    valueDelimiterToken = strtok_r(messageDelimiterToken, ":", &messageDelimiterToken);
    //  messageDictionary(keyDelimiterToken,valueDelimiterToken);


    Serial.println(messageDelimiterToken);
    Serial.println(keyDelimiterToken);
    Serial.println(valueDelimiterToken);

    strcpy(keybuffer, keyDelimiterToken);

    if (keybuffer != "\n")
    {
      strncpy(jsonKey, keybuffer + 1, strlen(keybuffer) - 1);

      char messageType = keybuffer[0];
      //   Serial.println(jsonKey);
      //     Serial.println(messageType);

      if (messageType == 'T')
      {
        telemetryDoc[jsonKey] = valueDelimiterToken;
      }
      else if (messageType == 'P')
      {
        propertyDoc[jsonKey] = valueDelimiterToken;
      }

    }


    messageDelimiterToken = strtok_r(str, "#", &str);
  }
}
