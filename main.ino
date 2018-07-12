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

enum FadeStates_t {
  IDLE = 0,
  BEGIN_FADE,
  IS_FADING,
  END_FADE,
  COLOR_CHANGED
};
typedef struct Status_t
{
  CRGB CurrentColor;
  CRGB NewColor;
  uint8_t FadeAmount : 8;
  FadeStates_t FadeState : 4;
  bool IsColorBlending : 1;
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
void fadeTowardColorLoop()
{
//  FadeStates_t statusStart = IDLE;
  int8_t result = nblendU8TowardU8( status.CurrentColor.red, status.NewColor.red, status.FadeAmount)
    + nblendU8TowardU8( status.CurrentColor.green, status.NewColor.green, status.FadeAmount)
    + nblendU8TowardU8( status.CurrentColor.blue,  status.NewColor.blue,  status.FadeAmount);
  
//  statusStart = status.FadeState;
  switch(status.FadeState)
  {
    case IDLE:
      if (result > 0) status.FadeState = BEGIN_FADE;
      break;
    case BEGIN_FADE:
      if (result == 0){
        status.FadeState = END_FADE;
      }else{
        status.FadeState = IS_FADING;
        leds[0] = status.CurrentColor;
        show();
      }
      break;
    case IS_FADING:
      if (result == 0){
        status.FadeState = END_FADE;
      }else{
        leds[0] = status.CurrentColor;
        show();
      }
      break;
    case END_FADE:
      if (result == 0){
        status.FadeState = COLOR_CHANGED;
      }else{
        status.FadeState = BEGIN_FADE;
      }
      break;
    case COLOR_CHANGED:
      if (result == 0)
      {
        status.FadeState = IDLE;
      }else{
        status.FadeState = BEGIN_FADE;
      }
      break;
  }    
  // if (statusStart != status.FadeState)
  // {
  //   Serial << "Start Status: " << statusStart << " End Status: " << status.FadeState << "\n";
  // }
}

void show()
{
  CRGB* rgb = FastLED.leds();
  analogWrite(REDPIN,   rgb->r );
  analogWrite(GREENPIN, rgb->g );
  analogWrite(BLUEPIN,  rgb->b );
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
  status.IsColorBlending = false;
  status.FadeState = IDLE;
  status.FadeAmount = 5;

  Serial.begin(115200);
  FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
  Serial.println(F("READY:"));

}

void setColorInstantly()
{
  status.CurrentColor = status.NewColor;
  leds[0] = status.CurrentColor;
  show();
  status.FadeState = COLOR_CHANGED;
}
void loop() 
{
   
  //CRGB bgColor( 0, 15, 2); // pine green ?
  
  // fade all existing pixels toward bgColor by "5" (out of 255)
  fadeTowardColorLoop();
  switch (status.FadeState)
  {
    case IDLE:
      break;
    case BEGIN_FADE:
      Serial << "BEGIN FADE\n";
      break;
    case IS_FADING:
      break;
    case END_FADE:
      Serial << "END FADE\n";
      break;
    case COLOR_CHANGED:
      Serial << "COLOR CHANGED: RGB " << status.CurrentColor.red << " " << status.CurrentColor.green << " " << status.CurrentColor.blue << "\n";
      break;
  }
  //periodically set random pixel to a random color, to show the fading
  EVERY_N_MILLISECONDS( 5000 ) {
    if (status.FadeState == IDLE)
    {
      CRGB color = CHSV( random8(), 255, 255);
      //status.NewColor = CRGB::White;
      status.NewColor = color;
      Serial << "New color: RGB " << status.NewColor.red << " " << status.NewColor.green << " " << status.NewColor.blue << "\n";
    }
  }

  FastLED.delay(10);
}