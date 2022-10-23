//taken from https://github.com/ToniA/arduino-heatpumpir

#define MODEL_SHARP 0
#define MODEL_IVT   1

// Sharp codes
#define SHARP_AIRCON1_MODE_HEAT  0x01
#define SHARP_AIRCON1_MODE_COOL  0x02
#define SHARP_AIRCON1_MODE_DRY   0x03
#define SHARP_AIRCON1_MODE_OFF   0x21 // Power OFF
#define SHARP_AIRCON1_MODE_ON    0x31 // Power ON
#define SHARP_AIRCON1_FAN_AUTO   0x20 // Fan speed
#define SHARP_AIRCON1_FAN1       0x40
#define SHARP_AIRCON1_FAN2       0x30
#define SHARP_AIRCON1_FAN3       0x50
#define SHARP_AIRCON1_FAN4       0x70

// IVT codes
#define SHARP_AIRCON2_MODE_ON    0x11 // Power ON
// Power state
#define POWER_OFF   0
#define POWER_ON    1

// Operating modes
#define MODE_AUTO   1
#define MODE_HEAT   2
#define MODE_COOL   3
#define MODE_DRY    4
#define MODE_FAN    5
#define MODE_MAINT  6

// Fan speeds. Note that some heatpumps have less than 5 fan speeds
#define FAN_AUTO    0
#define FAN_1       1
#define FAN_2       2
#define FAN_3       3
#define FAN_4       4
#define FAN_5       5
#define FAN_SILENT  6  //SILENT/NIGHTMODE


#define N38PULSE     18
#define N38HIGHDELAY 12
#define N38LOWDELAY  12

class IVTHeatPump
{
  private:
    int irPin;
    void writePulse(boolean on);
    void writeBit(boolean bit);
    void writeByte(uint8_t data);
    void sendIVT(uint8_t powerMode, uint8_t operatingMode, uint8_t fanSpeed, uint8_t temperature, uint8_t hours, uint8_t mins);
  public:
    IVTHeatPump();
    void setIVTMode(uint8_t powerModeCmd, uint8_t operatingModeCmd, uint8_t fanSpeedCmd, uint8_t temperatureCmd, uint8_t hours, uint8_t mins); 
    void init(int irPin);
};

IVTHeatPump::IVTHeatPump()
{
}
void IVTHeatPump::init(int irPin)
{
    this->irPin = irPin;
    pinMode(irPin, OUTPUT);
}

void IVTHeatPump::writePulse(boolean on)
{
  for (int ii = 0; ii < N38PULSE; ++ii)
  {
    digitalWrite(irPin,on);
    delayMicroseconds(N38HIGHDELAY);
    digitalWrite(irPin,false);
    delayMicroseconds(N38LOWDELAY);
  }
}
void IVTHeatPump::writeBit(boolean bit)
{
  if (bit)
  {
    writePulse(false);
    writePulse(false);
    writePulse(false);
    writePulse(true);
  }
  else
  {
    writePulse(false);
    writePulse(true);
  }
}
void IVTHeatPump::writeByte(uint8_t data)
{
  for (int ii = 0; ii < 8; ++ii) writeBit((bitRead(data, ii) > 0));
}

void IVTHeatPump::setIVTMode(uint8_t powerModeCmd, uint8_t operatingModeCmd, uint8_t fanSpeedCmd, uint8_t temperatureCmd, uint8_t hours, uint8_t mins)
{
  uint8_t sharpModel = MODEL_IVT;
  // Sensible defaults for the heat pump mode

  uint8_t powerMode     = sharpModel == MODEL_SHARP ? SHARP_AIRCON1_MODE_ON : SHARP_AIRCON2_MODE_ON;
  uint8_t operatingMode = SHARP_AIRCON1_MODE_HEAT;
  uint8_t fanSpeed      = SHARP_AIRCON1_FAN_AUTO;
  uint8_t temperature   = 23;

  if (powerModeCmd == 0)
  {
    powerMode = SHARP_AIRCON1_MODE_OFF;
  }

  switch (operatingModeCmd)
  {
    case MODE_HEAT:
      operatingMode = SHARP_AIRCON1_MODE_HEAT;
      break;
    case MODE_COOL:
      operatingMode = SHARP_AIRCON1_MODE_COOL;
      break;
    case MODE_DRY:
      operatingMode = SHARP_AIRCON1_MODE_DRY;
      temperatureCmd = 10;
      break;
    case MODE_FAN:
      operatingMode = SHARP_AIRCON1_MODE_COOL;
      // Temperature needs to be set to 32 degrees for 'simulated' FAN mode
      temperatureCmd = 32;
      break;
    case MODE_MAINT: // Maintenance mode is just the heat mode at +8 or +10, FAN5
      operatingMode |= SHARP_AIRCON1_MODE_HEAT;
      temperature = 10;
      fanSpeedCmd = FAN_5;
      break;
  }

  switch (fanSpeedCmd)
  {
    case FAN_AUTO:
      fanSpeed = SHARP_AIRCON1_FAN_AUTO;
      break;
    case FAN_1:
      fanSpeed = SHARP_AIRCON1_FAN1;
      break;
    case FAN_2:
      fanSpeed = SHARP_AIRCON1_FAN2;
      break;
    case FAN_3:
      fanSpeed = SHARP_AIRCON1_FAN3;
    case FAN_4:
      fanSpeed = SHARP_AIRCON1_FAN4;
      break;
  }

  if ( temperatureCmd > 16 && temperatureCmd < 32)
  {
    temperature = temperatureCmd;
  }

  sendIVT(powerMode, operatingMode, fanSpeed, temperature, hours, mins);
}

void IVTHeatPump::sendIVT(uint8_t powerMode, uint8_t operatingMode, uint8_t fanSpeed, uint8_t temperature, uint8_t hours, uint8_t mins)
{
//  uint8_t SharpTemplate[] = { 0xAA, 0x5A, 0xCF, 0x10, 0x00, 0x00, 0x00, 0x06, 0x08, 0x80, 0x04, 0xF0, 0x01 };
  uint8_t SharpTemplate[] = { 0xAA, 0x5A, 0xCF, 0x10, 0x00, 0x00, 0x00,   13, 0x1A, 0x80,   52, 0xE0, 0x01 };
  //                             0     1     2     3     4     5     6     7     8     9    10    11    12

  uint8_t checksum = 0x00;

  // Set the power mode on the template message
  SharpTemplate[5] = powerMode;

  // Set the fan speed & operating mode on the template message
  SharpTemplate[6] = fanSpeed | operatingMode;

  // Set the temperature on the template message
  if (temperature == 10) {
    SharpTemplate[4] = 0x00; // Maintenance mode temperature
  } else {
    SharpTemplate[4] = (temperature + 177);
  }
  SharpTemplate[7] = hours;
  SharpTemplate[10] = mins;

  // Calculate the checksum
  for (int i=0; i<12; i++) {
    checksum ^= SharpTemplate[i];
  }

  checksum ^= SharpTemplate[12] & 0x0F;
  checksum ^= (checksum >> 4);
  checksum &= 0x0F;

  SharpTemplate[12] |= (checksum << 4);
//  Serial.println(SharpTemplate[12]);

  
  for (int ii = 0; ii < 8; ++ii) writePulse(true);
  for (int ii = 0; ii < 4; ++ii) writePulse(false);
  for (int ii = 0; ii < 1; ++ii) writePulse(true);

  for (int ii = 0; ii < 13; ++ii) writeByte(SharpTemplate[ii]);
}
