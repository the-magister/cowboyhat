#include <FastLED.h>
#include <Streaming.h>
#include <FiniteStateMachine.h>
#include <Metro.h>
#include <Bounce.h>

#include "Accelerometer.h"

FASTLED_USING_NAMESPACE

Accelerometer accel = Accelerometer(0, 1, 2); // x=A0, y=A1, z=A2

State atRest = State(atRestEnter, atRestUpdate, NULL);
State lookDown = State(lookDownEnter, lookDownUpdate, NULL);
State areMoving = State(areMovingEnter, areMovingUpdate, NULL);

FSM fsm = FSM(atRest); // initialize state machine, start in state: atRest

#define DATA_PIN 4 // data
#define GND_PIN 5 // will set to gnd.
#define LED_TYPE WS2812 // Could try WS2812B?
#define COLOR_ORDER RGB
#define NUM_LEDS 10
CRGB leds[NUM_LEDS];

#define FRAMES_PER_SECOND 30

#define N_UPDATES 10

// tie the pushbutton to pins 8 and 9.  We'll use 9 as a ground and 8 pulled-up.
#define BUTTON_IN 8
#define BUTTON_GND 9
Bounce calButton = Bounce(BUTTON_IN, 25UL);

void setup()
{
  // initialize the serial communications:
  Serial.begin(115200);

  digitalWrite(GND_PIN, LOW);
  pinMode(GND_PIN, OUTPUT);

  pinMode(BUTTON_IN, INPUT_PULLUP);
  
  digitalWrite(BUTTON_GND, LOW);
  pinMode(BUTTON_GND, OUTPUT);

  delay(1000); // for upload and to make sure we don't overload the USB to supply power.

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE,DATA_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  
//  accel.calibrate();

}

void loop()
{
  // check for recalibrate press
  if( calButton.update() && calButton.read()==LOW ) {
    Serial << F("Calibrating") << endl;
    accel.calibrate();
  }

  // update the lights  
  static Metro ledUpdate(1000UL/FRAMES_PER_SECOND);
  if( ledUpdate.check() ) { 
    FastLED.show();
  } else {
    // dump on an interval
    static Metro printEvery(500);
    if( printEvery.check() ) accel.dump();
  }

  // get accelerometer data
//  accel.update();
  for(int i=0; i<N_UPDATES; i++) accel.update();
//  int pitch = accel.pitch();
//  int roll = accel.roll();
//  int milligee = accel.milligee();
  
   // by default, we're moving
  fsm.transitionTo( areMoving );

  if( lookdownCheck() ) fsm.transitionTo( lookDown );
  
  if( atRestCheck() ) fsm.transitionTo( atRest );
         
  // update the state machine
  fsm.update();


}

boolean atRestCheck() {
  static Metro timer(10000UL);  
  static boolean inGesture = false;
  boolean isGesture = isAround(accel.accel(0), 0, 20) && isAround(accel.accel(1), 0, 20) && isAround(accel.accel(2), 100, 20); 

  if( isGesture != inGesture ) {
    // and enough time has elapsed
    if( timer.check() ) {
      inGesture = isGesture;
    }
  } else {
    // reset timer
    timer.reset();
  }
  
  return( inGesture );
}
void atRestEnter() {
  Serial << F("atRest Enter") << endl;
  // set master brightness control
  FastLED.setBrightness(32);
}
void atRestUpdate() {
  // a colored dot sweeping back and forth, with fading trails
  static byte gHue = 0;

  fadeToBlackBy(leds, NUM_LEDS, 1);
//  int pos = beatsin16(20, 0, NUM_LEDS);
  static byte pos=0;
  static int dir=1;
  static Metro advancePos(100);
  
  if(advancePos.check()) {
    if(dir>0) {
      pos+=dir;
      pos %= NUM_LEDS;
    } else {
      if(pos==0) pos=NUM_LEDS-1;
      else pos--;
    }
    advancePos.interval(random8(25,255));
    advancePos.reset();
    
    if(random8(100) < 10) dir=-dir;
  }
  
  leds[pos] += CHSV(gHue, 255, 255);
//  if( pos==0 ) gHue=random8();
  gHue++;

}

boolean lookdownCheck() {
  static Metro timer(1000UL);  
  static boolean inGesture = false;
  boolean isGesture = accel.roll() <= -40; 

  if( isGesture != inGesture ) {
    // and enough time has elapsed
    if( timer.check() ) {
      inGesture = isGesture;
    }
  } else {
    // reset timer
    timer.reset();
  }
  
  return( inGesture );
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
  FastLED.setBrightness(255);
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

  // update the lights
  static byte gHue = random8(0,255);
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue++, 360/(NUM_LEDS-1));
  
  // possibly adjust brightness on an interval
  static Metro updateInterval(5UL);
  if( ! updateInterval.check() ) return;

  static byte mb = 128;

  static int lmg = accel.milligee();
  int mg = accel.milligee();

  int diff = abs(lmg-mg); // could be 2000, at most.
  if( diff >= 50 && mb < 255 ) mb++;
  if( diff < 10 && mb > 10) mb--;  
  
  FastLED.setBrightness(mb);
  
//  Serial << diff << "\t" << mb << endl;
//  delay(25);
  
  lmg = mg;

}

boolean isAround(int c, int y, int delta) {
  return( c >= (y-delta) && c <= (y+delta) );
}
