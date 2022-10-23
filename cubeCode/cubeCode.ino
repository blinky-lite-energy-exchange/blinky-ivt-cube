#include "IVT.h"
#include "wifiCredentials.h"

#define BLINKYMQTTBUSBUFSIZE  9
union BlinkyBusUnion
{
  struct
  {
    int16_t state;
    int16_t watchdog;
    int16_t chipTemp;
    int16_t power;
    int16_t mode;
    int16_t fan;
    int16_t temperature;
    int16_t hour;
    int16_t min;
  };
  int16_t buffer[BLINKYMQTTBUSBUFSIZE];
} blinkyBus;
void subscribeCallback(uint8_t address, int16_t value)
{
  if (address == 8) setHeatPump();
}

#include "blinky-wifiMqtt-cube.h"
int wirelessConnectedPin = 6;
unsigned long tnow;
unsigned long lastWirelessBlink;
boolean wirelessLed = false;
IVTHeatPump ivtHeatPump = IVTHeatPump();


void setup() 
{
  pinMode(wirelessConnectedPin, OUTPUT);
  digitalWrite(wirelessConnectedPin, 1);
  Serial.begin(115200);
  delay(5000);
  ivtHeatPump.init(22);
  initBlinkyBus(2000,true, 7);

  blinkyBus.power = POWER_OFF;
  blinkyBus.mode = MODE_HEAT;
  blinkyBus.fan = FAN_AUTO;
  blinkyBus.temperature = 18;
  blinkyBus.hour = 1;
  blinkyBus.min = 1;
  
  setHeatPump();
  digitalWrite(wirelessConnectedPin, 1);
  tnow = millis();
  lastWirelessBlink = tnow;
}

void loop() 
{
  tnow = millis();
  blinkyBusLoop();
//  publishBlinkyBusNow(); 
  if (g_wifiStatus == WL_CONNECTED)
  {
    wirelessLed = false;
    digitalWrite(wirelessConnectedPin, wirelessLed);
  }
  else
  {
    if ((tnow - lastWirelessBlink) > 1000)
    {
      wirelessLed = !wirelessLed;
      lastWirelessBlink = tnow;
      digitalWrite(wirelessConnectedPin, wirelessLed);
    }
  }
  blinkyBus.chipTemp = (int16_t) (analogReadTemp() * 100.0);
}
void setHeatPump()
{
  if (blinkyBus.state == 0)
  {
    Serial.println("Setting Heat Pump");
    ivtHeatPump.setIVTMode(blinkyBus.power, blinkyBus.mode, blinkyBus.fan, blinkyBus.temperature, blinkyBus.hour, blinkyBus.min);
  }
}
