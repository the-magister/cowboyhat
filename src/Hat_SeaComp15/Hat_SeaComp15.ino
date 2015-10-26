#include <Streaming.h>
#include <FiniteStateMachine.h>
#include <Metro.h>
#include "FastLED.h"

#include "Accelerometer.h"

FASTLED_USING_NAMESPACE

Accelerometer accel = Accelerometer(0, 1, 2); // x=A0, y=A1, z=A2

State atRest = State(atRestEnter, atRestUpdate, NULL);
State lookDown = State(lookDownEnter, lookDownUpdate, NULL);
State areMoving = State(areMovingEnter, areMovingUpdate, NULL);

FSM fsm = FSM(atRest); //initialize state machine, start in state: atRest

#define DATA_PIN    4

#define LED_TYPE    WS2812
#define COLOR_ORDER RGB
#define NUM_LEDS    10
CRGB leds[NUM_LEDS];

#define FRAMES_PER_SECOND  30

#define N_UPDATES 5

int resting[3]; // resting state

void setup()
{
  // initialize the serial communications:
  Serial.begin(115200);

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  
  for(int i=0; i<N_UPDATES; i++) accel.update();
  accel.calibrate();
  for(int i=0; i<N_UPDATES; i++) accel.update();
  resting[0] = accel.accel(0);
  resting[1] = accel.accel(1);
  resting[2] = accel.accel(2);
}


void loop()
{
  // get accelerometer data
  for(int i=0; i<N_UPDATES; i++) accel.update();
//  int pitch = accel.pitch();
//  int roll = accel.roll();
//  int milligee = accel.milligee();
  
  // dump on an interval
  static Metro printEvery(500);
  if( printEvery.check() ) { 
    accel.dump();

  }
  
  // if the hat is roughly level for five seconds, do idle
  static Metro timeToIdle(500UL);
  static byte idleCount = 0;
  if( timeToIdle.check() ) {
    if( isAround(accel.accel(0), 0) && isAround(accel.accel(1), 0) && isAround(accel.accel(2), 100) ) idleCount ++;
    else idleCount = 0;
  }
  
  // gestures:
  // look down: get a headlamp
  if( accel.roll() >= 40 ) fsm.transitionTo(lookDown);
  // don't move: get an idle display
  else if( idleCount>=10 ) fsm.transitionTo(atRest);
  // otherwise, we're moving around
  else fsm.transitionTo(areMoving);

  fsm.update();
  
  static Metro ledUpdate(1000UL/FRAMES_PER_SECOND);
  if( ledUpdate.check() ) FastLED.show();
}

void atRestEnter() {
  Serial << F("atRest Enter") << endl;
  // set master brightness control
  FastLED.setBrightness(64);
}
void atRestUpdate() {
  // a colored dot sweeping back and forth, with fading trails
  static byte gHue = 0;

  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(20, 0, NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 255);
  if( pos==0 ) gHue=random8();

}

void lookDownEnter() {
  Serial << F("lookDown Enter") << endl;
    // set master brightness control
  FastLED.setBrightness(255);
}
void lookDownUpdate() {
 // fade down the sides
 fadeToBlackBy( leds, NUM_LEDS, 1 );
 // fade up the front
 leds[0] += CRGB(2,2,2);
 leds[NUM_LEDS-1] += CRGB(2,2,2);
 // brake lights in the back
 leds[4] += CRGB(2,0,0);
 leds[5] += CRGB(2,0,0);
}

void areMovingEnter() {
  Serial << F("areMoving Enter") << endl;
    // set master brightness control
//  FastLED.setBrightness(255);
}
void areMovingUpdate() {
/*
  static byte gHue = random8(0,255);
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16(13,0,NUM_LEDS);
  leds[pos] += CHSV( gHue, 255, 192);
*/  
/*
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16(i+7,0,NUM_LEDS)] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
*/
/*  
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
*/

  static byte gHue = random8(0,255);
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue++, 360/(NUM_LEDS-1));
  
  static byte mb = 128;

  byte ac = abs(accel.milligee()-1000);
  if( ac >= 75 && mb < 255 ) mb++;
  if( ac < 75 && mb > 50) mb--;
  
  FastLED.setBrightness(mb);

}

boolean isAround(int c, int y) {
  return( c >= y-3 && c <= y+3 );
}
