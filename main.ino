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

CRGBPalette16 gradientSunrise = HeatColors_p;
//#define NUMBER_PATTERNS 5
// CRGBPalette16 palettes[NUMBER_PATTERNS] = {
//     RainbowColors_p,  //0
//     HeatColors_p,   // 1
//     CloudColors_p,  // 2
//     PartyColors_p,  // 3
//     OceanColors_p // 4
// };

enum FadeStates_t {
  FADE_IDLE,
  FADE_BEGIN,
  FADE_FADING,
  FADE_END
};
enum LoopStates_t {
  IDLE = 0,
  COLOR_CHANGED,
  BEGIN_FLASH,
  LOOP_FADING,
  FLASH_LIT_BEGIN,
  FLASH_LIT,
  FLASH_LIT_END,
  FLASH_DARK_BEGIN,
  FLASH_DARK, // 8
  FLASH_DARK_END,
  END_FLASH,
  BEGIN_SUNRISE,
  SUNRISE_RUNNING,
  SUNRISE_END,
  LIGHTING_BEGIN,
  LIGHTING_STRIKES,
  LIGHTING_END
};
typedef struct Status_t
{
  CRGB CurrentColor;  // Color currently displayed
  CRGB NewColor;      // Desired new color to display
  uint8_t FadeStep : 4; // Amount to adjust fade per loop
  uint8_t FadeStepDelay : 4;  // Time (in 10ms steps) between fade loops (Higher the number the longer it takes to fade)
  LoopStates_t LoopState : 5; // State of the main loop
  FadeStates_t FadeState : 3; // State of the fade loop
  uint16_t timer_10ms; // General purpose 10 ms timer used by main loop
  uint8_t timerFade_10ms; // 10ms time used by fade loop
  uint8_t FlashTimerOn_10ms; // Time (in ms) for LEDs to be on for flash
  uint8_t FlashTimerOff_10ms; // Time (in ms) for LEDs to be off for flash
  uint8_t PaletteDurationMinutes; // Time to cycle though entire gradient
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
  Flash(10, 10); // 100ms on, 100ms off
}

void FlashSlow()
{
  Flash(50,50); // 500ms on, 500ms off
}
void Flash(uint8_t OnDurationMs, uint8_t OffDurationMs)
{
  if (OnDurationMs == 0) OnDurationMs = 1;
  if (OffDurationMs == 0) OffDurationMs = 1;
  status.LoopState = BEGIN_FLASH;
  status.FlashTimerOn_10ms = OnDurationMs;
  status.FlashTimerOff_10ms = OffDurationMs;
  status.CurrentColor = CRGB::Red;
  status.CurrentColor = CRGB::Black;
}
// Begin fading to NewColor
void FadeToColor()
{
  status.LoopState = LOOP_FADING;
  status.FadeState = FADE_BEGIN;
}

void Off()
{
  status.NewColor = CRGB::Black;
  FadeToColor();
}

void Sunrise()
{
  status.LoopState = BEGIN_SUNRISE;
}

void loopFade()
{
  int8_t i = 0;
  switch (status.FadeState)
  {
    case FADE_IDLE:
      break;
    case FADE_BEGIN:
      status.FadeState = FADE_FADING;
      status.timerFade_10ms = status.FadeStepDelay;
      break;
    case FADE_FADING:
      if (status.timerFade_10ms > 0) return; // Not time to work yet.
      i = nblendU8TowardU8( status.CurrentColor.red, status.NewColor.red, status.FadeStep)
        + nblendU8TowardU8( status.CurrentColor.green, status.NewColor.green, status.FadeStep)
        + nblendU8TowardU8( status.CurrentColor.blue,  status.NewColor.blue,  status.FadeStep);
      leds[0] = status.CurrentColor;
      show();      
      if (i == 0){
        status.FadeState = FADE_END; return; // Fade complete.
      }
      status.timerFade_10ms = status.FadeStepDelay; // Wait 10ms before next fade step
      break;
    case FADE_END:
      //Serial << "FADE_END\n";
      break;
  }
}
void loopDoWork()
{
  static uint8_t sunriseInterval = 0; // Number of 10ms counts before finding next gradient
  static uint8_t paletteIndex = 0; // Which slice of the gradient to grab
  static uint8_t lighting_flashes = 0; // Number of lighting flashes
  EVERY_N_MILLIS(10)
  {
    if (status.timer_10ms > 0)
    {
      status.timer_10ms--;
    }
    if (status.timerFade_10ms > 0)
    {
      status.timerFade_10ms--;
    }
  }
  //Serial << "LoopState: " << status.LoopState << "\n";
  loopFade();
  switch(status.LoopState)
  {
    case IDLE:
      break;
    case LOOP_FADING:
      switch (status.FadeState){
        case FADE_IDLE:
          status.FadeState = FADE_BEGIN;
          break;
        case FADE_END:
          //Serial << "FADE_END COLOR_CHANGED\n";
          status.LoopState = COLOR_CHANGED;
          status.FadeState = FADE_IDLE;
          break;
      }
      break;
    case COLOR_CHANGED:
      status.LoopState = IDLE;
      break;
    case BEGIN_FLASH:
      status.LoopState = FLASH_DARK_BEGIN;
      break;
    case FLASH_DARK_BEGIN:
      leds[0] = status.NewColor;
      show();
      status.timer_10ms = status.FlashTimerOff_10ms;
      status.LoopState = FLASH_DARK;
      break;
    case FLASH_DARK:
      if (status.timer_10ms > 0) return; // Not time to work yet   
      status.LoopState = FLASH_DARK_END; 
      break;
    case FLASH_DARK_END:
      status.LoopState = FLASH_LIT_BEGIN;
      break;
    case FLASH_LIT_BEGIN:
      leds[0] = status.CurrentColor;
      show();
      status.timer_10ms = status.FlashTimerOn_10ms;
      status.LoopState = FLASH_LIT;
      break;
    case FLASH_LIT:
      if (status.timer_10ms > 0) return; // Not time to work yet 
      status.LoopState = FLASH_LIT_END;
      break;
    case FLASH_LIT_END:
      status.LoopState = FLASH_DARK_BEGIN;
      break;
    case END_FLASH:
      break;
    case BEGIN_SUNRISE:
      sunriseInterval = (uint8_t)(((status.PaletteDurationMinutes * 60.0) / 241.0) * 100); // 
      Serial << "sunriseInterval: " << sunriseInterval << "\n";
      status.LoopState = SUNRISE_RUNNING;
      paletteIndex = 0;
      break;
    case SUNRISE_RUNNING:
      if (status.timer_10ms > 0) return; // Not time yet
      if (paletteIndex == 241)
      {
        status.LoopState = SUNRISE_END;
        return;
      }
      leds[0] = ColorFromPalette(gradientSunrise, paletteIndex);
      show();
      Serial << "SUNRISE: Index " << paletteIndex << " RGB " << leds[0].red << " " << leds[0].green << " " << leds[0].blue << "\n";
      paletteIndex++;
      status.timer_10ms = sunriseInterval;
      break;
    case SUNRISE_END:
      status.LoopState = IDLE;
      break;
    case LIGHTING_BEGIN:
      lighting_flashes = random8(3, 8); // Number of flashes
      leds[0] = CHSV(255, 0, 51); // 1st flash isn't bright
      show();
      delay(random8(4,10)); // each flash only lasts 4-10 ms
      leds[0] = CRGB::Black;
      show();
      status.timer_10ms = random8(10, 15); // delay 100 to 150 ms
      status.LoopState = LIGHTING_STRIKES;
      break;
    case LIGHTING_STRIKES:
      if (status.timer_10ms > 0) return; // Not yet time
      if (lighting_flashes == 0){
        status.LoopState = LIGHTING_END;
        return; // No more flashes
      } 
      lighting_flashes--;
      leds[0] = CHSV(255, 0, (255/random8(1,3))); // return strokes are brighter
      show();
      delay(random8(4,10)); // Flash last between 4 and 10 ms.
      leds[0] = CRGB::Black;
      show();
      status.timer_10ms = (5 + random8(10)); // 50 to 150 ms delay until next flash
      break;
    case LIGHTING_END:
      //status.LoopState = IDLE;
      status.CurrentColor = CRGB::Black;
      FadeToColor();
      break;
    default:
      Serial << "UNKNOWN STATE! " << status.LoopState << "\n";
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
  status.LoopState = COLOR_CHANGED;
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
  status.LoopState = IDLE;
  status.FadeState = FADE_IDLE;
  status.FadeStep = 1;
  status.FadeStepDelay = 5;

  Serial.begin(115200);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  Serial.println(F("READY:"));
  status.PaletteDurationMinutes = 1;
  //Sunrise();
  //FadeToColor();
  //FlashSlow();
  status.LoopState = LIGHTING_BEGIN;
}

void loop() 
{
  loopDoWork();
  switch (status.LoopState)
  {
    case IDLE:
      break;
    // case BEGIN_FADE:
    //   Serial << "BEGIN FADE\n";
    //   break;
    // case FADING:
    //   break;
    // case END_FADE:
    //   Serial << "END FADE\n";
    //   break;
    case COLOR_CHANGED:
      Serial << "COLOR CHANGED: RGB " << status.CurrentColor.red << " " << status.CurrentColor.green << " " << status.CurrentColor.blue << "\n";
      break;
  }

  delay(10);
  // if (status.timer_10ms > 0)
  // {
  //   status.timer_10ms--;
  // }
  // if (status.timerFade_10ms > 0)
  // {
  //   status.timerFade_10ms--;
  // }

  
  //periodically set random pixel to a random color, to show the fading
  // EVERY_N_SECONDS( 5 ) {
  //   if (status.LoopState == IDLE)
  //   {
  //     CRGB color = CHSV( random8(), 255, 255);
  //     //status.NewColor = CRGB::White;
  //     status.NewColor = color;
  //     FadeToColor();
  //     Serial << "New color: RGB " << status.NewColor.red << " " << status.NewColor.green << " " << status.NewColor.blue << "\n";
  //   }
  // }


}