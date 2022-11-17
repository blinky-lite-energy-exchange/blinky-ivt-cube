#include <WiFi.h>
#include <PubSubClient.h>
#include "LittleFS.h"

union SubscribeData
{
  struct
  {
      uint8_t command;
      uint8_t address;
      int16_t value;
  };
  byte buffer[4];
};

static void   BlinkyMqttCubeWifiApButtonHandler();
static void   BlinkyMqttCubeCallback(char* topic, byte* payload, unsigned int length);

class BlinkyMqttCube
{
  private:
  
    String        g_ssid;
    String        g_wifiPassword;
    String        g_mqttServer;
    String        g_mqttUsername;
    String        g_mqttPassword;
    String        g_mqttPublishTopic;
    String        g_mqttSubscribeTopic;

    WiFiClient    g_wifiClient;
    PubSubClient  g_mqttClient;
    WiFiServer*   g_wifiServer;

    boolean       g_init = true;
    unsigned long g_lastMsgTime = 0;
    unsigned long g_lastWirelessBlink = 0;
    int           g_wifiStatus = 0;
    unsigned long g_wifiTimeout = 25000;
    unsigned long g_wifiRetry = 30000;
    unsigned long g_wifiLastTry = 0;
    int           g_publishInterval  = 2000;
    boolean       g_publishOnInterval = true;
    boolean       g_publishNow = false;
    int           g_commLEDPin = LED_BUILTIN;
    int           g_commLEDBright = 255;
    boolean       g_commLEDState = false;
    boolean       g_wifiAccessPointMode = false;
    boolean       g_webPageServed = false;
    boolean       g_webPageRead = false;
    unsigned int  g_cubeDataSize;
    boolean       g_chattyCathy = false;
    SubscribeData g_subscribeData;
    int16_t*      g_cubeData;
    int           g_resetButtonPin = -1;
    boolean       g_resetButtonDownState = false;
    unsigned long g_resetTimeout = 10000;
    volatile unsigned long g_resetButtonDownTime = 0;
    void          (*g_userMqttCallback)(uint8_t, int16_t);

    void          setupWifiAp();
    boolean       reconnect(); 
    void          setup_wifi();
    void          readWebPage();
    void          serveWebPage();
    String        replaceHtmlEscapeChar(String inString);
    void          setCommLEDPin(boolean ledState);

  public:
    BlinkyMqttCube();
    void          publishNow();
    void          loop();
    void          init(int publishInterval, boolean publishOnInterval, int commLEDPin,  int commLEDBright, int resetButtonPin, int16_t* cubeData,  unsigned int cubeDataSize, void (*userMqttCallback)(uint8_t, int16_t));
    void          setChattyCathy(boolean chattyCathy);
    void          mqttCubeCallback(char* topic, byte* payload, unsigned int length);
    void          wifiApButtonHandler();

};
BlinkyMqttCube::BlinkyMqttCube()
{

  g_init = true;
  g_lastMsgTime = 0;
  g_lastWirelessBlink = 0;
  g_wifiStatus = 0;
  g_wifiTimeout = 20000;
  g_wifiRetry = 20000;
  g_wifiLastTry = 0;
  g_publishInterval  = 2000;
  g_publishOnInterval = true;
  g_publishNow = false;
  g_commLEDPin = LED_BUILTIN;
  g_commLEDBright = 255;
  g_commLEDState = false;
  g_wifiAccessPointMode = false;
  g_webPageServed = false;
  g_webPageRead = false;
  g_chattyCathy = false;
  g_resetButtonPin = -1;
  g_resetButtonDownState = false;
  g_resetTimeout = 10000;
  g_resetButtonDownTime = 0;

}
void BlinkyMqttCube::setCommLEDPin(boolean ledState)
{
  g_commLEDState = ledState;
  if (g_commLEDPin == LED_BUILTIN)
  {
    digitalWrite(g_commLEDPin, g_commLEDState);
  }
  else
  {
    if (g_commLEDState)
    {
      analogWrite(g_commLEDPin, g_commLEDBright);
    }
    else
    {
      analogWrite(g_commLEDPin, 0);
    }
  }
}

void BlinkyMqttCube::loop()
{
  rp2040.wdt_reset();
  if (g_wifiAccessPointMode)
  {
    serveWebPage();
    readWebPage();
  }
  else
  {
    if (g_resetButtonPin > 0 )
    {
      if (digitalRead(g_resetButtonPin))
      {
        if ((millis() - g_resetButtonDownTime) < g_resetTimeout)
        {
          setCommLEDPin(false);
          return;
        }
        else
        {
          setCommLEDPin(true);
          return;
        }
      }
    }
    g_wifiStatus = WiFi.status();
    unsigned long now = millis();
    if (g_wifiStatus == WL_CONNECTED)
    {
      if (!g_mqttClient.connected()) 
      {
        if (!reconnect()) return;
      }
      g_mqttClient.loop();
    
      if (g_publishOnInterval)
      {
        if (g_commLEDState)
        {
          if (now - g_lastMsgTime > 10) 
          {
            setCommLEDPin(false);
          }
        }
        if (now - g_lastMsgTime > g_publishInterval) 
        {
          g_lastMsgTime = now;
          setCommLEDPin(true);
          g_cubeData[1] = g_cubeData[1] + 1;
          if (g_cubeData[1] > 32760) g_cubeData[1]= 0 ;
          g_mqttClient.publish(g_mqttPublishTopic.c_str(), (uint8_t*)g_cubeData, g_cubeDataSize);
        }
      }
      if (g_publishNow)
      {
          g_lastMsgTime = now;
          setCommLEDPin(true);
          g_publishNow = false;
          g_cubeData[1] = g_cubeData[1] + 1;
          if (g_cubeData[1] > 32760) g_cubeData[1] = 0 ;
          g_mqttClient.publish(g_mqttPublishTopic.c_str(), (uint8_t*) g_cubeData, g_cubeDataSize);
      }
    }
    else
    {
      setup_wifi();
      if ((now - g_lastWirelessBlink) > 100)
      {
        setCommLEDPin(!g_commLEDState);
        g_lastWirelessBlink = now;
      }
    }
  }

}
void BlinkyMqttCube::publishNow()
{
  g_publishNow = true;
}
void BlinkyMqttCube::init(int publishInterval, boolean publishOnInterval, int commLEDPin, int commLEDBright, int resetButtonPin, int16_t* cubeData,  unsigned int cubeDataSize, void (*userMqttCallback)(uint8_t, int16_t))
{
  g_init = true;
  g_cubeData = cubeData;
  g_cubeDataSize = cubeDataSize;
  g_userMqttCallback = userMqttCallback;
  g_wifiClient = WiFiClient();
  g_mqttClient = PubSubClient(g_wifiClient);
  g_wifiServer = new WiFiServer(80);
  g_commLEDBright =  commLEDBright;

  if (g_chattyCathy) Serial.println("Reading creds.txt file");
  LittleFS.begin();
  File file = LittleFS.open("/creds.txt", "r");
  if (file) 
  {
      String lines[7];
      String data = file.readString();
      int startPos = 0;
      int stopPos = 0;
      for (int ii = 0; ii < 7; ++ii)
      {
        startPos = data.indexOf("{") + 1;
        stopPos = data.indexOf("}");
        lines[ii] = data.substring(startPos,stopPos);
        if (g_chattyCathy) Serial.println(lines[ii]);
        data = data.substring(stopPos + 1);
      }
      g_ssid               = lines[0];
      g_wifiPassword       = lines[1];
      g_mqttServer         = lines[2];
      g_mqttUsername       = lines[3];
      g_mqttPassword       = lines[4];
      g_mqttPublishTopic   = lines[5];
      g_mqttSubscribeTopic = lines[6];
      file.close();
      g_mqttClient.setKeepAlive(60);
      g_mqttClient.setSocketTimeout(60);
  }
  else
  {
    if (g_chattyCathy) Serial.println("file open failed");
  }

  if (g_chattyCathy) Serial.println("Starting Communications");
  g_publishInterval = publishInterval;
  g_publishOnInterval = publishOnInterval;
  g_mqttClient.setServer(g_mqttServer.c_str(), 1883);
  g_mqttClient.setCallback(BlinkyMqttCubeCallback);
  g_cubeData[0] = 1; //init
  g_cubeData[1] = 0;
  g_commLEDPin = commLEDPin;
  pinMode(g_commLEDPin, OUTPUT);
  setCommLEDPin(false);
  g_resetButtonPin = resetButtonPin;
  if (g_resetButtonPin > 0 )
  { 
    attachInterrupt(digitalPinToInterrupt(g_resetButtonPin), BlinkyMqttCubeWifiApButtonHandler, CHANGE);
    pinMode(g_resetButtonPin, INPUT);
  }
  rp2040.wdt_begin(5000);
  g_wifiLastTry = 0;
  setup_wifi();
  g_init = false;
}
void BlinkyMqttCube::setup_wifi() 
{
  if (((millis() - g_wifiLastTry) < g_wifiRetry) && !g_init) return;
  setCommLEDPin(true);
  
  if (g_chattyCathy) Serial.println();
  if (g_chattyCathy) Serial.print("Connecting to ");
  if (g_chattyCathy) Serial.println(g_ssid);
  
  if (g_chattyCathy) Serial.print("SSID: ");
  if (g_chattyCathy) Serial.println(g_ssid);
  if (g_chattyCathy) Serial.print("Wifi Password: ");
  if (g_chattyCathy) Serial.println(g_wifiPassword);

  WiFi.mode(WIFI_STA);
  WiFi.begin(g_ssid.c_str(), g_wifiPassword.c_str());
  
  g_wifiLastTry = millis();
  g_wifiStatus = WiFi.status();
  while ((g_wifiStatus != WL_CONNECTED) && ((millis() - g_wifiLastTry) < g_wifiTimeout))
  {
    g_wifiStatus = WiFi.status();
    delay(500);
    if (g_chattyCathy) Serial.print(".");
    setCommLEDPin(!g_commLEDState);

    rp2040.wdt_reset();
  }
  g_wifiLastTry = millis();

  if (g_wifiStatus == WL_CONNECTED)
  {
    if (g_chattyCathy) Serial.println("");
    if (g_chattyCathy) Serial.println("WiFi connected");
    if (g_chattyCathy) Serial.println("IP address: ");
    if (g_chattyCathy) Serial.println(WiFi.localIP());
  }
  else
  {
    if (g_chattyCathy) Serial.println("");
    if (g_chattyCathy) Serial.println("WiFi not connected");
  }
}


boolean BlinkyMqttCube::reconnect() 
{
  boolean reconnectSuccess = false;
  if (!g_mqttClient.connected()) 
  {
    if (g_chattyCathy) Serial.print("Attempting MQTT connection using ID...");
    // Create a random client ID
    String mqttClientId = g_mqttUsername + "-";
    mqttClientId += String(random(0xffff), HEX);
    if (g_chattyCathy) Serial.print(mqttClientId);
    // Attempt to connect
    if (g_mqttClient.connect(mqttClientId.c_str(),g_mqttUsername.c_str(), g_mqttPassword.c_str())) 
    {
      if (g_chattyCathy) Serial.println("...connected");
      // Once connected, publish an announcement...
//      g_mqttClient.publish(g_mqttPublishTopic, "hello world");
      // ... and resubscribe
      g_mqttClient.subscribe(g_mqttSubscribeTopic.c_str());
      reconnectSuccess = true;
    } 
    else 
    {
      int mqttState = g_mqttClient.state();
      if (g_chattyCathy) Serial.print("failed, rc=");
      if (g_chattyCathy) Serial.print(mqttState);
      if (g_chattyCathy) Serial.print(": ");
      switch (mqttState) 
      {
        case -4:
          if (g_chattyCathy) Serial.println("MQTT_CONNECTION_TIMEOUT");
          break;
        case -3:
          if (g_chattyCathy) Serial.println("MQTT_CONNECTION_LOST");
          break;
        case -2:
          if (g_chattyCathy) Serial.println("MQTT_CONNECT_FAILED");
          break;
        case -1:
          if (g_chattyCathy) Serial.println("MQTT_DISCONNECTED");
          break;
        case 0:
          if (g_chattyCathy) Serial.println("MQTT_CONNECTED");
          break;
        case 1:
          if (g_chattyCathy) Serial.println("MQTT_CONNECT_BAD_PROTOCOL");
          break;
        case 2:
          if (g_chattyCathy) Serial.println("MQTT_CONNECT_BAD_CLIENT_ID");
          break;
        case 3:
          if (g_chattyCathy) Serial.println("MQTT_CONNECT_UNAVAILABLE");
          break;
        case 4:
          if (g_chattyCathy) Serial.println("MQTT_CONNECT_BAD_CREDENTIALS");
          break;
        case 5:
          if (g_chattyCathy) Serial.println("MQTT_CONNECT_UNAUTHORIZED");
          break;
        default:
          break;
      }
    }
  }
  else
  {
    reconnectSuccess = true;
  }
  return reconnectSuccess;
}
void BlinkyMqttCube::readWebPage()
{
  if (!g_webPageServed) return;
  if (g_webPageRead) return;
  WiFiClient client = g_wifiServer->available();

  if (client) 
  {
    String header = "";
    while (client.connected() && !g_webPageRead) 
    {
      rp2040.wdt_reset();
      if (client.available()) 
      {
        char c = client.read();
        header.concat(c);
      }
      else
      {
        break;
      }
    }
    if (header.indexOf("POST") >= 0)
    {
      if (g_chattyCathy) Serial.println(header);
      String data = header.substring(header.indexOf("ssid="),header.length());
      g_ssid               = replaceHtmlEscapeChar(data.substring(5,data.indexOf("&pass=")));
      g_wifiPassword       = replaceHtmlEscapeChar(data.substring(data.indexOf("&pass=")  + 6,data.indexOf("&serv=")));
      g_mqttServer         = replaceHtmlEscapeChar(data.substring(data.indexOf("&serv=")  + 6,data.indexOf("&unam=")));
      g_mqttUsername       = replaceHtmlEscapeChar(data.substring(data.indexOf("&unam=")  + 6,data.indexOf("&mpass=")));
      g_mqttPassword       = replaceHtmlEscapeChar(data.substring(data.indexOf("&mpass=") + 7,data.indexOf("&pubt=")));
      g_mqttPublishTopic   = replaceHtmlEscapeChar(data.substring(data.indexOf("&pubt=")  + 6,data.indexOf("&subt=")));
      g_mqttSubscribeTopic = replaceHtmlEscapeChar(data.substring(data.indexOf("&subt=")  + 6,data.length()));
      if (g_chattyCathy) Serial.println(header);
      if (g_chattyCathy) Serial.println(data);
      if (g_chattyCathy) Serial.print("X");
      if (g_chattyCathy) Serial.print(g_ssid);
      if (g_chattyCathy) Serial.print("X");
      if (g_chattyCathy) Serial.print(g_wifiPassword);
      if (g_chattyCathy) Serial.print("X");
      if (g_chattyCathy) Serial.print(g_mqttServer);
      if (g_chattyCathy) Serial.print("X");
      if (g_chattyCathy) Serial.print(g_mqttUsername);
      if (g_chattyCathy) Serial.print("X");
      if (g_chattyCathy) Serial.print(g_mqttPassword);
      if (g_chattyCathy) Serial.print("X");
      if (g_chattyCathy) Serial.print(g_mqttPublishTopic);
      if (g_chattyCathy) Serial.print("X");
      if (g_chattyCathy) Serial.print(g_mqttSubscribeTopic);
      if (g_chattyCathy) Serial.println("X");
      client.println("HTTP/1.1 200 OK");
      client.println("Content-Type: text/html");
      client.println("Connection: close");  // the connection will be closed after completion of the response
      client.println();
      client.println("<!DOCTYPE HTML>");
      client.println("<html><head><title>Blinky-Lite Credentials</title><style>");
      client.println("body{background-color: #083357 !important;font-family: Arial;}");
      client.println(".labeltext{color: white;font-size:250%;}");
      client.println(".formtext{color: black;font-size:250%;}");
      client.println(".cell{padding-bottom:25px;}");
      client.println("</style></head><body>");
      client.println("<h1 style=\"color:white;font-size:300%;text-align: center;\">Blinky-Lite Credentials</h1><hr><div>");
      client.println("<div>");
      client.println("<h1 style=\"color:yellow;font-size:300%;text-align: center;\">Accepted</h1><div>");
      client.println("</div>");
      client.println("</body></html>");
      g_webPageRead = true;
    }
    delay(100);
    client.stop();
    if (g_webPageRead)
    {
      g_wifiAccessPointMode = false;
      WiFi.disconnect();
      if (g_chattyCathy) Serial.println("Writing creds.txt file");
      File file = LittleFS.open("/creds.txt", "w");
      if (file) 
      {
        String line = "";
        line = "{" + g_ssid + "}";
        file.println(line);
        line = "{" + g_wifiPassword + "}";
        file.println(line);
        line = "{" + g_mqttServer + "}";
        file.println(line);
        line = "{" + g_mqttUsername + "}";
        file.println(line);
        line = "{" + g_mqttPassword + "}";
        file.println(line);
        line = "{" + g_mqttPublishTopic + "}";
        file.println(line);
        line = "{" + g_mqttSubscribeTopic + "}";
        file.println(line);
        file.println("");
        file.println("");
        file.println("");
        file.close();
      }
      else
      {
        if (g_chattyCathy) Serial.println("file open failed");
      }
      if (g_chattyCathy) Serial.println("Rebooting");
      delay(10000); //watchdog will kick in
    }
  }
}
void BlinkyMqttCube::serveWebPage()
{
  if (g_webPageServed) return;
  WiFiClient client = g_wifiServer->available();

  if (client) 
  {
    if (g_chattyCathy) Serial.println("new client");
    String header = "";
    while (client.connected() && !g_webPageServed) 
    {
      rp2040.wdt_reset();
      if (client.available()) 
      {
        char c = client.read();
        header.concat(c);
      }
      else
      {
        break;
      }
    }
    if (g_chattyCathy) Serial.println(header);
    client.println("HTTP/1.1 200 OK");
    client.println("Content-Type: text/html");
    client.println("Connection: close");  // the connection will be closed after completion of the response
    client.println();
    client.println("<!DOCTYPE HTML>");
    client.println("<html><head><title>Blinky-Lite Credentials</title><style>");
    client.println("body{background-color: #083357 !important;font-family: Arial;}");
    client.println(".labeltext{color: white;font-size:250%;}");
    client.println(".formtext{color: black;font-size:250%;}");
    client.println(".cell{padding-bottom:25px;}");
    client.println("</style></head><body>");
    client.println("<h1 style=\"color:white;font-size:300%;text-align: center;\">Blinky-Lite Credentials</h1><hr><div>");
    client.println("<form action=\"/disconnected\" method=\"POST\">");
    client.println("<table align=\"center\">");
    
    client.println("<tr>");
    client.println("<td class=\"cell\"><label for=\"ssid\" class=\"labeltext\">SSID</label></td>");
    String tag = "<td class=\"cell\"><input name=\"ssid\" id=\"ssid\" type=\"text\" value=\"" + g_ssid + "\" class=\"formtext\"/></td>";
    client.println(tag.c_str());
    client.println("</tr>");
    
    client.println("<tr>");
    client.println("<td class=\"cell\"><label for=\"pass\" class=\"labeltext\">Wifi Password</label></td>");
    tag = "<td class=\"cell\"><input name=\"pass\" id=\"pass\" type=\"password\" value=\"" + g_wifiPassword + "\" class=\"formtext\"/></td>";
    client.println(tag.c_str());
    client.println("</tr>");
    
    client.println("<tr>");
    client.println("<td class=\"cell\"><label for=\"serv\" class=\"labeltext\">MQTT Server</label></td>");
    tag = "<td class=\"cell\"><input name=\"serv\" id=\"serv\" type=\"text\" value=\"" + g_mqttServer + "\" class=\"formtext\"/></td>";
    client.println(tag.c_str());
    client.println("</tr>");
  
    client.println("<tr>");
    client.println("<td class=\"cell\"><label for=\"unam\" class=\"labeltext\">MQTT Username</label></td>");
    tag = "<td class=\"cell\"><input name=\"unam\" id=\"unam\" type=\"text\" value=\"" + g_mqttUsername + "\" class=\"formtext\"/></td>";
    client.println(tag.c_str());
    client.println("</tr>");
    
    client.println("<tr>");
    client.println("<td class=\"cell\"><label for=\"mpass\" class=\"labeltext\">MQTT Password</label></td>");
    tag = "<td class=\"cell\"><input name=\"mpass\" id=\"mpass\" type=\"password\" value=\"" + g_mqttPassword + "\" class=\"formtext\"/></td>";
    client.println(tag.c_str());
    client.println("</tr>");
   
    client.println("<tr>");
    client.println("<td class=\"cell\"><label for=\"pubt\" class=\"labeltext\">MQTT Pub. Topic</label></td>");
    tag = "<td class=\"cell\"><input name=\"pubt\" id=\"pubt\" type=\"text\" value=\"" + g_mqttPublishTopic + "\" class=\"formtext\"/></td>";
    client.println(tag.c_str());
    client.println("</tr>");
      
    client.println("<tr>");
    client.println("<td class=\"cell\"><label for=\"subt\" class=\"labeltext\">MQTT Sub. Topic</label></td>");
    tag = "<td class=\"cell\"><input name=\"subt\" id=\"subt\" type=\"text\" value=\"" + g_mqttSubscribeTopic + "\" class=\"formtext\"/></td>";
    client.println(tag.c_str());
    client.println("</tr>");
    
    client.println("<tr>");
    client.println("<td></td>");
    client.println("<td><input type=\"submit\" class=\"formtext\"/></td>");
    client.println("</tr>");
    client.println("</table>");
    client.println("</form>");
    client.println("</div>");
    client.println("</body></html>");
    g_webPageServed = true;
    delay(100);
    client.stop();
    if (g_chattyCathy) Serial.println("client disconnected");
  }
}
String BlinkyMqttCube::replaceHtmlEscapeChar(String inString)
{
  String outString = "";
  char hexBuff[3];
  char *hexptr;
  for (int ii = 0; ii < inString.length(); ++ii)
  {
    char testChar = inString.charAt(ii);
    if (testChar == '%')
    {
      inString.substring(ii + 1,ii + 3).toCharArray(hexBuff, 3);
      char specialChar = (char) strtoul(hexBuff, &hexptr, 16);
      outString += specialChar;
      ii = ii + 2;
    }
    else
    {
      outString += testChar;
    }
  }
  return outString;
}
void BlinkyMqttCube::setupWifiAp()
{
  if (g_wifiAccessPointMode) return;
  setCommLEDPin(true);
  WiFi.disconnect();
  WiFi.mode(WIFI_AP);
  WiFi.softAP(g_mqttUsername);
  if (g_chattyCathy) Serial.print("[+] AP Created with IP Gateway ");
  if (g_chattyCathy) Serial.println(WiFi.softAPIP());
  g_wifiServer = new WiFiServer(80);
  g_wifiAccessPointMode = true;
  g_wifiServer->begin();
  if (g_chattyCathy) Serial.print("Connected to wifi. My address:");
  IPAddress myAddress = WiFi.localIP();
  if (g_chattyCathy) Serial.println(myAddress);
  g_webPageServed = false;
  g_webPageRead = false;
}
void BlinkyMqttCube::setChattyCathy(boolean chattyCathy)
{
  g_chattyCathy = chattyCathy;

  return;
}
void BlinkyMqttCube::mqttCubeCallback(char* topic, byte* payload, unsigned int length)
{
  if (g_chattyCathy) Serial.print("Message arrived [");
  if (g_chattyCathy) Serial.print(topic);
  if (g_chattyCathy) Serial.print("] {command: ");
  for (int i = 0; i < length; i++) 
  {
    g_subscribeData.buffer[i] = payload[i];
  }
  if (g_chattyCathy) Serial.print(g_subscribeData.command);
  if (g_chattyCathy) Serial.print(", address: ");
  if (g_chattyCathy) Serial.print(g_subscribeData.address);
  if (g_chattyCathy) Serial.print(", value: ");
  if (g_chattyCathy) Serial.print(g_subscribeData.value);
  if (g_chattyCathy) Serial.println("}");
  if (g_subscribeData.command == 1)
  {
      g_cubeData[g_subscribeData.address] = g_subscribeData.value;
      g_userMqttCallback(g_subscribeData.address, g_subscribeData.value);
  }
  return;
}
void BlinkyMqttCube::wifiApButtonHandler()
{
  if (digitalRead(g_resetButtonPin)) 
  {
    if (g_resetButtonDownState) return;
    if ((millis() - g_resetButtonDownTime) > 50)
    {
      if (g_chattyCathy) Serial.println("Button Down");
      g_resetButtonDownTime = millis();
      g_resetButtonDownState = true;
    }
  }
  else
  {
    if (!g_resetButtonDownState) return;
    if (g_chattyCathy) Serial.println("Button up");
    g_resetButtonDownState = false;
    if ((millis() - g_resetButtonDownTime) > g_resetTimeout)
    {
      setupWifiAp();
    }
  }
}

BlinkyMqttCube BlinkyMqttCube;

void BlinkyMqttCubeCallback(char* topic, byte* payload, unsigned int length) 
{
  BlinkyMqttCube.mqttCubeCallback(topic, payload,  length);
}

void BlinkyMqttCubeWifiApButtonHandler()
{
  delay(50);
  BlinkyMqttCube.wifiApButtonHandler();
}
