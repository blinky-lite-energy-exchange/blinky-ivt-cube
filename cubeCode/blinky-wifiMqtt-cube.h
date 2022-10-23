#include <WiFi.h>
#include <PubSubClient.h>

WiFiClient g_wifiClient;
PubSubClient g_mqttClient(g_wifiClient);
unsigned long g_lastMsgTime = 0;
int g_wifiStatus = 0;
unsigned long g_wifiTimeout = 15000;
unsigned long g_wifiRetry = 30000;
unsigned long g_wifiLastTry = 0;
int g_publishInterval  = 2000;
boolean g_publishOnInterval = true;
boolean g_publishNow = false;
int g_commLEDPin = LED_BUILTIN;
boolean g_commLEDState = false;

union SubscribeData
{
  struct
  {
      uint8_t command;
      uint8_t address;
      int16_t value;
  };
  byte buffer[4];
} g_subscribeData;



void setup_wifi() 
{
  if ((millis() - g_wifiLastTry) < g_wifiRetry) return;
  delay(10);
  g_wifiLastTry = millis();
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, wifiPassword);
  
  while ((g_wifiStatus != WL_CONNECTED) && ((millis() - g_wifiLastTry) < g_wifiTimeout))
  {
    g_wifiStatus = WiFi.status();
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());
  if (g_wifiStatus == WL_CONNECTED)
  {
    Serial.println("");
    Serial.println("WiFi connected");
    Serial.println("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else
  {
    Serial.println("");
    Serial.println("WiFi not connected");
  }
}

void mqttCallback(char* topic, byte* payload, unsigned int length) 
{
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] {command: ");
  for (int i = 0; i < length; i++) 
  {
    g_subscribeData.buffer[i] = payload[i];
  }
  Serial.print(g_subscribeData.command);
  Serial.print(", address: ");
  Serial.print(g_subscribeData.address);
  Serial.print(", value: ");
  Serial.print(g_subscribeData.value);
  Serial.println("}");
  if (g_subscribeData.command == 1)
  {
      blinkyBus.buffer[g_subscribeData.address] = g_subscribeData.value;
      subscribeCallback(g_subscribeData.address, g_subscribeData.value);
  }

}

void reconnect() 
{
  // Loop until we're reconnected
  while (!g_mqttClient.connected()) 
  {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String mqttClientId = "PicoWClient-";
    mqttClientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (g_mqttClient.connect(mqttClientId.c_str(),mqttUsername, mqttPassword)) 
    {
      Serial.println("connected");
      // Once connected, publish an announcement...
//      g_mqttClient.publish(mqttPublishTopic, "hello world");
      // ... and resubscribe
      g_mqttClient.subscribe(mqttSubscribeTopic);
    } 
    else 
    {
      Serial.print("failed, rc=");
      Serial.print(g_mqttClient.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}
void initBlinkyBus(int publishInterval, boolean publishOnInterval, int commLEDPin)
{
  Serial.println("Starting Communications");
  g_publishInterval = publishInterval;
  g_publishOnInterval = publishOnInterval;
  setup_wifi();
  g_mqttClient.setServer(mqttServer, 1883);
  g_mqttClient.setCallback(mqttCallback);
  blinkyBus.state = 1; //init
  blinkyBus.watchdog = 0;
  g_commLEDPin = commLEDPin;
  pinMode(g_commLEDPin, OUTPUT);
  digitalWrite(g_commLEDPin, g_commLEDState);
  
}
void blinkyBusLoop()
{
  g_wifiStatus = WiFi.status();
  if (g_wifiStatus == WL_CONNECTED)
  {
    if (!g_mqttClient.connected()) 
    {
      reconnect();
    }
    g_mqttClient.loop();
  
    unsigned long now = millis();
    if (g_publishOnInterval)
    {
      if (g_commLEDState > 0)
      {
        if (now - g_lastMsgTime > 10) 
        {
          g_commLEDState = 0;
          digitalWrite(g_commLEDPin, g_commLEDState);
        }
      }
      if (now - g_lastMsgTime > g_publishInterval) 
      {
        g_lastMsgTime = now;
        g_commLEDState = 1;
        digitalWrite(g_commLEDPin, g_commLEDState);
        blinkyBus.watchdog = blinkyBus.watchdog + 1;
        if (blinkyBus.watchdog > 32760) blinkyBus.watchdog = 0 ;
        g_mqttClient.publish(mqttPublishTopic, (uint8_t*)blinkyBus.buffer, BLINKYMQTTBUSBUFSIZE * 2);
      }
    }
    if (g_publishNow)
    {
        g_lastMsgTime = now;
        g_commLEDState = 1;
        digitalWrite(g_commLEDPin, g_commLEDState);
        g_publishNow = false;
        blinkyBus.watchdog = blinkyBus.watchdog + 1;
        if (blinkyBus.watchdog > 32760) blinkyBus.watchdog = 32760 ;
        g_mqttClient.publish(mqttPublishTopic, (uint8_t*)blinkyBus.buffer, BLINKYMQTTBUSBUFSIZE * 2);
    }
  }
  else
  {
    setup_wifi();
  }
}
void publishBlinkyBusNow()
{
  g_publishNow = true;
}
