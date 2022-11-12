#include "BlinkyMqttCube.h"
#include "IVT.h"

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
} cubeData;

void subscribeCallback(uint8_t address, int16_t value)
{
  if (address == 8) setHeatPump();
}

int cubeAlivePin = 6;
IVTHeatPump ivtHeatPump = IVTHeatPump();


void setup() 
{
  pinMode(cubeAlivePin, OUTPUT);
  analogWrite(cubeAlivePin, 20);
  Serial.begin(115200);
  delay(1000);
  ivtHeatPump.init(22);
  BlinkyMqttCube.setChattyCathy(false);
  BlinkyMqttCube.init(2000,true, 7, -1, (int16_t*)& cubeData,  sizeof(cubeData), subscribeCallback);

  cubeData.power = POWER_OFF;
  cubeData.mode = MODE_HEAT;
  cubeData.fan = FAN_AUTO;
  cubeData.temperature = 18;
  cubeData.hour = 1;
  cubeData.min = 1;
  
  setHeatPump();
}

void loop() 
{
  BlinkyMqttCube.loop();
  cubeData.chipTemp = (int16_t) (analogReadTemp() * 100.0);
}
void setHeatPump()
{
  if (cubeData.state == 0)
  {
    analogWrite(cubeAlivePin, 0);
    delay(500);
    Serial.println("Setting Heat Pump");
    ivtHeatPump.setIVTMode(cubeData.power, cubeData.mode, cubeData.fan, cubeData.temperature, cubeData.hour, cubeData.min);
    delay(500);
    int ledBright = 20 + 7 * (cubeData.temperature - 18);
    if (ledBright < 10) ledBright = 10;
    if (cubeData.power == POWER_ON) analogWrite(cubeAlivePin, ledBright);
  }
}
