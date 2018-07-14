#include <Arduino.h>
//#include <Homie.h>
template<class T> inline Print &operator <<(Print &obj, T arg) { obj.print(arg); return obj; }

#define FASTLED_ESP8266_NODEMCU_PIN_ORDER

#include <FastLED.h>

#define NUM_LEDS 1
CRGB leds[NUM_LEDS];
// LED  ESP-8266
#define REDPIN 13   // D7
#define GREENPIN 12 // D6
#define BLUEPIN 14  // D5
#define DATA_PIN 4  // D2

#define NUMBER_PATTERNS 7
CRGBPalette16 palettes[NUMBER_PATTERNS] = {
    RainbowColors_p,  //0
    RainbowStripesColors_p, // 1
    HeatColors_p,   // 2
    CloudColors_p,  // 3
    PartyColors_p,  // 4
    LavaColors_p,   // 5
    OceanColors_p // 6
};

enum FadeStates_t {
  IDLE = 0,
  BEGIN_FADE,
  FADING,
  END_FADE,
  COLOR_CHANGED,
  BEGIN_FLASH,
  FLASH_LIT_BEGIN,
  FLASH_LIT,
  FLASH_LIT_END,
  FLASH_DARK_BEGIN,
  FLASH_DARK, // 10
  FLASH_DARK_END,
  END_FLASH,
  BEGIN_SUNRISE,
  SUNRISE_RUNNING,
  SUNRISE_END
};
typedef struct Status_t
{
  CRGB CurrentColor;
  CRGB NewColor;
  uint8_t FadeAmount : 8;
  FadeStates_t FadeState : 4;
  uint16_t timer_ms;
  uint8_t FlashTimerOnMs;
  uint8_t FlashTimerOffMs;
  uint8_t sunriseLengthMinutes;
} ;

Status_t status;

// Helper function that blends one uint8_t toward another by a given amount
int8_t nblendU8TowardU8( uint8_t& cur, const uint8_t target, uint8_t amount)
{
  if( cur == target) {
    return 0;
  }

  if( cur < target ) {
    uint8_t delta = target - cur;
    delta = scale8_video( delta, amount);
    cur += delta;
  } else {
    uint8_t delta = cur - target;
    delta = scale8_video( delta, amount);
    cur -= delta;
  }
  return 1;
}

void FlashFast()
{
  Flash(100, 100); // 100ms on, 100ms off
}

void FlashSlow()
{
  Flash(500,500); // 500ms on, 500ms off
}
void Flash(uint8_t OnDurationMs, uint8_t OffDurationMs)
{
  if (OnDurationMs == 0) OnDurationMs = 1;
  if (OffDurationMs == 0) OffDurationMs = 1;
  status.FadeState = BEGIN_FLASH;
  status.FlashTimerOnMs = OnDurationMs;
  status.FlashTimerOffMs = OffDurationMs;
  status.CurrentColor = CRGB::Red;
  status.CurrentColor = CRGB::Black;
}
// Begin fading to NewColor
void FadeToColor()
{
  status.FadeState = BEGIN_FADE;
}

void Off()
{
  status.NewColor = CRGB::Black;
  FadeToColor();
}

void Sunrise()
{
  status.FadeState = BEGIN_SUNRISE;
}
void loopDoWork()
{
  int8_t i = 0;
  static uint8_t sunriseInterval = 0;
  static uint8_t paletteIndex = 0;

  switch(status.FadeState)
  {
    case IDLE:
      break;
    case BEGIN_FADE:
      status.FadeState = FADING;
      status.timer_ms = 10;
      break;
    case FADING:
      if (status.timer_ms > 0) return; // Not time to work yet.
      i = nblendU8TowardU8( status.CurrentColor.red, status.NewColor.red, status.FadeAmount)
        + nblendU8TowardU8( status.CurrentColor.green, status.NewColor.green, status.FadeAmount)
        + nblendU8TowardU8( status.CurrentColor.blue,  status.NewColor.blue,  status.FadeAmount);
      leds[0] = status.CurrentColor;
      show();      
      if (i == 0){
        status.FadeState = END_FADE; return; // Fade complete.
      }
      status.timer_ms = 10; // Wait 10ms before next fade step
      break;
    case END_FADE:
      status.FadeState = COLOR_CHANGED;
      break;
    case COLOR_CHANGED:
      status.FadeState = IDLE;
      break;
    case BEGIN_FLASH:
      status.FadeState = FLASH_DARK_BEGIN;
      break;
    case FLASH_DARK_BEGIN:
      leds[0] = status.NewColor;
      show();
      status.timer_ms = status.FlashTimerOffMs;
      status.FadeState = FLASH_DARK;
      break;
    case FLASH_DARK:
      if (status.timer_ms > 0) return; // Not time to work yet   
      status.FadeState = FLASH_DARK_END; 
      break;
    case FLASH_DARK_END:
      status.FadeState = FLASH_LIT_BEGIN;
      break;
    case FLASH_LIT_BEGIN:
      leds[0] = status.CurrentColor;
      show();
      status.timer_ms = status.FlashTimerOnMs;
      status.FadeState = FLASH_LIT;
      break;
    case FLASH_LIT:
      if (status.timer_ms > 0) return; // Not time to work yet 
      status.FadeState = FLASH_LIT_END;
      break;
    case FLASH_LIT_END:
      status.FadeState = FLASH_DARK_BEGIN;
      break;
    case END_FLASH:
      break;
    case BEGIN_SUNRISE:
      sunriseInterval = (uint8_t)(((status.sunriseLengthMinutes * 60.0) / 256.0) * 100); // 
      Serial << "sunriseInterval: " << sunriseInterval << "\n";
      status.FadeState = SUNRISE_RUNNING;
      paletteIndex = 0;
      break;
    case SUNRISE_RUNNING:
      if (status.timer_ms > 0) return; // Not time yet
      if (paletteIndex == 240)
      {
        status.FadeState = SUNRISE_END;
        return;
      }
      leds[0] = ColorFromPalette(palettes[2], paletteIndex);
      show();
      Serial << "SUNRISE: Index " << paletteIndex << " RGB " << leds[0].red << " " << leds[0].green << " " << leds[0].blue << "\n";
      paletteIndex++;
      status.timer_ms = sunriseInterval;
      break;
    case SUNRISE_END:
      break;
  }
}


void show()
{
  CRGB* rgb = FastLED.leds();
  analogWrite(REDPIN,   rgb->r );
  analogWrite(GREENPIN, rgb->g );
  analogWrite(BLUEPIN,  rgb->b );
}

void setColorInstantly()
{
  status.CurrentColor = status.NewColor;
  leds[0] = status.CurrentColor;
  show();
  status.FadeState = COLOR_CHANGED;
}

void setup() {
  pinMode(REDPIN, OUTPUT); // RED
  pinMode(GREENPIN, OUTPUT); // GREEN
  pinMode(BLUEPIN, OUTPUT); // BLUE    
  digitalWrite(REDPIN, LOW);
  digitalWrite(GREENPIN, LOW);
  digitalWrite(BLUEPIN, LOW);    

  status.CurrentColor = CRGB::Black;
  status.NewColor = CRGB::Red;
  status.FadeState = IDLE;
  status.FadeAmount = 5;

  Serial.begin(115200);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  Serial.println(F("READY:"));
  status.sunriseLengthMinutes = 1;
  Sunrise();
}

void loop() 
{

  switch (status.FadeState)
  {
    case IDLE:
      break;
    case BEGIN_FADE:
      Serial << "BEGIN FADE\n";
      break;
    case FADING:
      break;
    case END_FADE:
      Serial << "END FADE\n";
      break;
    case COLOR_CHANGED:
      Serial << "COLOR CHANGED: RGB " << status.CurrentColor.red << " " << status.CurrentColor.green << " " << status.CurrentColor.blue << "\n";
      break;
  }
  loopDoWork();

  EVERY_N_MILLISECONDS(1)
  {
    if (status.timer_ms > 0)
    {
      status.timer_ms--;
    }
  }


  /*
  //periodically set random pixel to a random color, to show the fading
  EVERY_N_MILLISECONDS( 5000 ) {
    if (status.FadeState == IDLE)
    {
      CRGB color = CHSV( random8(), 255, 255);
      //status.NewColor = CRGB::White;
      status.NewColor = color;
      FadeToColor();
      Serial << "New color: RGB " << status.NewColor.red << " " << status.NewColor.green << " " << status.NewColor.blue << "\n";
    }
  }
*/
  delay(1);
}