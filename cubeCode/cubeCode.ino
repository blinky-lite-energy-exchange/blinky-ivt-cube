#include "BlinkyBus.h"
#include "IVT.h"
#define BAUD_RATE  19200
#define commLEDPin    2
#define IRPin    13
#define BLINKYBUSBUFSIZE  7

union BlinkyBusUnion
{
  struct
  {
    int16_t state;
    int16_t power;
    int16_t mode;
    int16_t fan;
    int16_t temperature;
    int16_t hour;
    int16_t min;
  };
  int16_t buffer[BLINKYBUSBUFSIZE];
} bb;
BlinkyBus blinkyBus(bb.buffer, BLINKYBUSBUFSIZE, Serial1, commLEDPin);
IVTHeatPump ivtHeatPump = IVTHeatPump();


void setup()
{
  ivtHeatPump.init(IRPin);

  bb.state = 1;
  bb.power = POWER_OFF;
  bb.mode = MODE_HEAT;
  bb.fan = FAN_AUTO;
  bb.temperature = 18;
  bb.hour = 1;
  bb.min = 1;

  Serial1.begin(BAUD_RATE);
  blinkyBus.start();
  delay(1000);
}

void loop()
{
  if (blinkyBus.poll() == 2)
  {
    if (bb.state == 0)
    {
      if (blinkyBus.getLastWriteAddress() == 6)
      {
        ivtHeatPump.setIVTMode(bb.power, bb.mode, bb.fan, bb.temperature, bb.hour, bb.min);
      }
    }
  }

}
