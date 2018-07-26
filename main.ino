#include <ArduinoJson.h>

#include <Arduino.h>
#include <Homie.h>
#include <FastLED.h>

#define REDPIN  D7
#define GREENPIN D6
#define BLUEPIN  D5

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
  PALETTE_BEGIN,
  PALETTE_LOOP_BEGIN,
  PALETTE_RUNNING,
  PALETTE_LOOP_END,
  PALETTE_END,
  LIGHTING_BEGIN,
  LIGHTING_LOOP_BEGIN,
  LIGHTING_STRIKES,
  LIGHTING_LOOP_END,
  LIGHTING_LOOP_DELAY,
  LIGHTING_END
};

typedef struct Status_t
{
  uint8_t FadeStep : 4; // Amount to adjust fade per loop
  uint8_t FadeStepDelay : 4;  // Time (in 10ms steps) between fade loops (Higher the number the longer it takes to fade)
  LoopStates_t LoopState : 5; // State of the main loop
  FadeStates_t FadeState : 3; // State of the fade loop
  uint16_t timer_10ms; // General purpose 10 ms timer used by main loop
  uint8_t timerFade_10ms; // 10ms time used by fade loop
  uint8_t timerFadeReporting_10ms; // Every this * 10 ms, send a MQTT update
} ;

CRGB led;
Status_t status;
const int BUFFER_SIZE = 200;
char buffer[BUFFER_SIZE];

CRGBPalette16 CurrentPalette = CRGBPalette16(CRGB::Black);
CRGBPalette16 TargetPalette = CRGBPalette16(CRGB::Red);

HomieNode nodeLed("json", "light"); //("light", "switch") ID of light, type of switch

bool sendMqttReport()
{
  // {"state": "ON", "color": {"r": 63, "g": 0, "b": 255}}
  int jsonSize = 0;
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["state"] = ((led.r == 0) && (led.g == 0) && (led.b == 0)) ? "OFF" : "ON";
  JsonObject& color = root.createNestedObject("color");
  color["r"] = led.red;
  color["g"] = led.green;
  color["b"] = led.blue;

  jsonSize = root.measureLength() + 1;
  if (jsonSize > BUFFER_SIZE)
  {
    Homie.getLogger() << "JSON size " << jsonSize << " exceeds BUFFER_SIZE " << BUFFER_SIZE << endl;
    return false;
  }
  root.printTo(buffer, jsonSize);
  nodeLed.setProperty("ha").send(buffer);
  return true;
}

bool jsonReceivedHandler(const HomieRange& range, const String& value)
{
  StaticJsonBuffer<500> jsonBuffer;
  Homie.getLogger() << "JSON!: " << value << endl;
  JsonObject& root = jsonBuffer.parseObject(value);
  if (!root.success())
  {
    //Homie.getLogger() <<  "parseObject() failed" << endl;
    return false;
  }

  if (root.containsKey("state"))
  {
    //Homie.getLogger() << "contains state" << endl;
    if (strcmp(root["state"], "ON") == 0)
    {
      On();
    }else{
      Off();
    }
  }
  if (root.containsKey("color")) {
    Homie.getLogger() << "changing Color" << endl;
    TargetPalette = CRGBPalette16(CRGB(root["color"]["r"], root["color"]["g"], root["color"]["b"]));
    FadeToTargetPalette();
    Homie.getLogger() << "changed Color" << endl;
    return true;
  }
  if (root.containsKey("flash"))
  {

  }
  return false;
}

// Begin fading to NewColor
void FadeToTargetPalette()
{
  status.LoopState = LOOP_FADING;
  status.FadeState = FADE_BEGIN;
}

void Off()
{
  TargetPalette = CRGBPalette16(CRGB::Black);
  FadeToTargetPalette();
}
void On()
{
  TargetPalette = CRGBPalette16(CRGB::White);
  FadeToTargetPalette();
}

void dumpColors()
{
  CRGB rgb = ColorFromPalette(CurrentPalette, 0, 255);
  Homie.getLogger() << "CURRENT  R:" << rgb.red << "\tG:" << rgb.green << "\tB:" << rgb.blue << endl;
  rgb = ColorFromPalette(TargetPalette, 0, 255);
  Homie.getLogger() << "NewColor R:" << rgb.red << "\tG:" << rgb.green << "\tB:" << rgb.blue << endl;
}

void homieLoopHandler()
{
  int8_t i = 0;
//  float f = 0.0;

  EVERY_N_MILLIS_I(ticktock, 10)
  {
    if (status.timer_10ms > 0) status.timer_10ms--;
    if (status.timerFade_10ms > 0) status.timerFade_10ms--;
    if (status.timerFadeReporting_10ms > 0) status.timerFadeReporting_10ms--;
  }

  switch(status.LoopState)
  {
    case IDLE:
      break;
    case LOOP_FADING:
      switch (status.FadeState){
        case FADE_IDLE:
          break;
        case FADE_END:
          status.LoopState = COLOR_CHANGED;
          status.FadeState = FADE_IDLE;
          break;
      }
      break;
    case COLOR_CHANGED:
      status.LoopState = IDLE;
      break;    
  }
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
      nblendPaletteTowardPalette(CurrentPalette, TargetPalette, 48);
      showCurrentPalette();
      if (CurrentPalette == TargetPalette){
        status.FadeState = FADE_END; return; // Fade complete.
      }
      status.timerFade_10ms = status.FadeStepDelay; // Wait 10ms before next fade step
      break;
    case FADE_END:
      break;
  }

  switch(status.LoopState)
  {
    case LOOP_FADING:
      switch(status.FadeState){
        case FADE_BEGIN:
          Homie.getLogger() << "FADE BEGIN!" << endl;
          break;          
        case FADE_END:
          Homie.getLogger() << "FADE DONE!" << endl;
          break;
        case FADE_FADING:
          if (status.timerFadeReporting_10ms == 0)
          {
              status.timerFadeReporting_10ms = 20;
              //Homie.getLogger() << "Fading R:" << led.red << "\tG:" << led.green << "\tB:" << led.blue << endl;
              sendMqttReport();
          }
          break;
      }
      break;
    case COLOR_CHANGED:
      //Homie.getLogger() << "COLOR CHANGED R:" << led.red << "\tG:" << led.green << "\tB:" << led.blue << endl;
      sendMqttReport();
      break;
  }
}

void show()
{
  analogWrite(REDPIN,   led.r );
  analogWrite(GREENPIN, led.g );
  analogWrite(BLUEPIN,  led.b );
}

void showCurrentPalette()
{
  led = ColorFromPalette(CurrentPalette,0, 255);
  show();
}

void showTargetPallette()
{
  led = ColorFromPalette(TargetPalette, 0, 255);
  show();
}

void homieSetupHandler()
{
  nodeLed.advertise("ha").settable(jsonReceivedHandler);
}

void setup() {
  pinMode(REDPIN, OUTPUT); // RED
  pinMode(GREENPIN, OUTPUT); // GREEN
  pinMode(BLUEPIN, OUTPUT); // BLUE    
  digitalWrite(REDPIN, LOW);
  digitalWrite(GREENPIN, LOW);
  digitalWrite(BLUEPIN, LOW);    

  status.LoopState = IDLE;
  status.FadeState = FADE_IDLE;
  status.FadeStep = 1;
  status.FadeStepDelay = 1;
  status.timerFadeReporting_10ms = 20;

  Serial.begin(115200);

  Homie_setFirmware("HA-LedFade", "1.0.0");
  Homie.setSetupFunction(homieSetupHandler).setLoopFunction(homieLoopHandler);
  Homie.setup();
  Homie.getLogger() << "READY" << endl;
  FadeToTargetPalette();
}

void loop() 
{
  Homie.loop();
  delay(1);

}