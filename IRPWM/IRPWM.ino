#include "IVT.h"
IVTHeatPump ivtHeatPump = IVTHeatPump();

void setup() 
{
  ivtHeatPump.init(13);
  pinMode(2, OUTPUT);
  digitalWrite(2, HIGH);
  delay(1000);
  digitalWrite(2, LOW);
  delay(10000);
  digitalWrite(2, HIGH);
  ivtHeatPump.setIVTMode(POWER_ON, MODE_HEAT, FAN_AUTO, 23, 14, 8);
  delay(1000);
  digitalWrite(2, LOW);
  delay(20000);
  digitalWrite(2, HIGH);
  ivtHeatPump.setIVTMode(POWER_OFF, MODE_HEAT, FAN_AUTO, 23, 14, 8);
  delay(1000);
  digitalWrite(2, LOW);
}

void loop() 
{
}
