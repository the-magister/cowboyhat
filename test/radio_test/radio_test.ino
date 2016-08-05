#include <Streaming.h>
#include <FiniteStateMachine.h>
#include <Metro.h>
#include <Bounce.h>
#include <FastLED.h>

// radio
#include <RFM69.h>
RFM69 radio;

#include "Accelerometer.h"

FASTLED_USING_NAMESPACE

Accelerometer accel = Accelerometer(0, 1, 2); // x=A0, y=A1, z=A2

void atRestEnter();
void atRestUpdate();
State atRest = State(atRestEnter, atRestUpdate, NULL);
void lookDownEnter();
void lookDownUpdate();
State lookDown = State(lookDownEnter, lookDownUpdate, NULL);
void areMovingEnter();
void areMovingUpdate();
State areMoving = State(areMovingEnter, areMovingUpdate, NULL);
void shadowsDisplayEnter();
void shadowsDisplayUpdate();
State shadowsDisplay = State(shadowsDisplayEnter, shadowsDisplayUpdate, NULL);

FSM fsm = FSM(atRest); //initialize state machine, start in state: atRest

#define DATA_PIN    4
#define GND_PIN     5
#define LED_TYPE    WS2812
#define COLOR_ORDER RGB
#define NUM_LEDS    10
CRGB leds[NUM_LEDS];

#define FRAMES_PER_SECOND  30

#define N_UPDATES 10

// tie the pushbutton
#define BUTTON_IN 8
#define BUTTON_GND 9
Bounce calButton = Bounce(BUTTON_IN, 25UL);

#define SHADOWS_GROUP 157
#define SIMON_GROUP 188
byte currentGroup = SHADOWS_GROUP;

void setup() {
  Serial.begin(115200);

  digitalWrite(GND_PIN, LOW);
  pinMode(GND_PIN, OUTPUT);

  pinMode(BUTTON_IN, INPUT_PULLUP);
  digitalWrite(BUTTON_GND, LOW);
  pinMode(BUTTON_GND, OUTPUT);

  // put your setup code here, to run once:
  radio.initialize(RF69_915MHZ, 212, currentGroup);
  radio.setHighPower(); // using RFM69HW board
  radio.promiscuous(true); // so broadcasts are received

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

}

void loop() {
  // check for recalibrate press
  if ( calButton.update() && calButton.read() == LOW ) {
    Serial << F("Calibrating") << endl;
    accel.calibrate();
  }


  // hear from network colleagues, do a display
  static Metro networkTimeout(10000UL);
  static boolean haveNetworkTraffic = false;
  if ( radio.receiveDone() ) {
    networkTimeout.reset();
    haveNetworkTraffic = true;
    if ( currentGroup == SHADOWS_GROUP ) fsm.transitionTo(shadowsDisplay);
  }

  if ( networkTimeout.check() ) haveNetworkTraffic = false;

  if ( ! haveNetworkTraffic ) {

    // get accelerometer data
    for (int i = 0; i < N_UPDATES; i++) accel.update();
    //  int pitch = accel.pitch();
    //  int roll = accel.roll();
    //  int milligee = accel.milligee();

    // dump on an interval
    static Metro printEvery(500);
    if ( printEvery.check() ) {
      accel.dump();
    }

    // if the hat is roughly level for five seconds, do idle
    static Metro timeToIdle(500UL);
    static byte idleCount = 0;
    if ( timeToIdle.check() ) {
      if ( isAround(accel.accel(0), 0) && isAround(accel.accel(1), 0) && isAround(accel.accel(2), 100) ) idleCount ++;
      else idleCount = 0;
    }

    // gestures:
    // look down: get a headlamp
    if ( accel.roll() <= -40 ) fsm.transitionTo(lookDown);
    // don't move: get an idle display
    else if ( idleCount >= 10 ) fsm.transitionTo(atRest);
    // otherwise, we're moving around
    else fsm.transitionTo(areMoving);
  }

  fsm.update();

  static Metro ledUpdate(1000UL / FRAMES_PER_SECOND);
  if ( ledUpdate.check() ) FastLED.show();

}
void shadowsDisplayEnter() {
  Serial << F("shadowsDisply Enter") << endl;
  // set master brightness control
  FastLED.setBrightness(255);
  // set off
  fill_solid( leds, NUM_LEDS, CRGB(0, 0, 0));
}
void shadowsDisplayUpdate() {
  unsigned long message; // message 
  word distance[3]; // distances from sensors to object
  static word maxd[3] = {0,0,0};
  static byte bright[3] = {0,0,0};
  
  if( radio.receiveDone() && radio.DATALEN==sizeof(message) ) {

    message = *(unsigned long*)radio.DATA; 
    
    distance[0] = (message >>  2) & 1023UL; // dumping six MSB
    distance[1] = (message >> 12) & 1023UL; // dumping six MSB
    distance[2] = (message >> 22) & 1023UL; // dumping six MSB

    for(byte i=0;i<3;i++) {
      if( distance[i] > maxd[i] ) maxd[i] = distance[i];
      byte newBright = map(distance[i], 0, maxd[i], 255, 8);
  
//      if( newBright>bright[i]) bright[i]++;
//      else if( newBright<bright[i]) bright[i]--;
      bright[i] = newBright;
      
      Serial << bright[i];
    }
    Serial << endl;

    fill_solid( leds, NUM_LEDS, CRGB(bright[0], bright[1], bright[2]));
  }
  
}

void atRestEnter() {
  Serial << F("atRest Enter") << endl;
  // set master brightness control
  FastLED.setBrightness(64);
}
void atRestUpdate() {
  // a colored dot sweeping back and forth, with fading trails
  static byte gHue = 0;

  fadeToBlackBy(leds, NUM_LEDS, 1);
  //  int pos = beatsin16(20, 0, NUM_LEDS);
  static byte pos = 0;
  static int dir = 1;
  static Metro advancePos(100);

  if (advancePos.check()) {
    if (dir > 0) {
      pos += dir;
      pos %= NUM_LEDS;
    } else {
      if (pos == 0) pos = NUM_LEDS - 1;
      else pos--;
    }
    advancePos.interval(random8(25, 255));
    advancePos.reset();

    if (random8(100) < 10) dir = -dir;
  }

  leds[pos] += CHSV(gHue, 255, 255);
  //  if( pos==0 ) gHue=random8();
  gHue++;

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
  leds[0] += CRGB(2, 2, 2);
  leds[NUM_LEDS - 1] += CRGB(2, 2, 2);
  // brake lights in the back
  leds[4] += CRGB(2, 0, 0);
  leds[5] += CRGB(2, 0, 0);
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

  // update the lights
  static byte gHue = random8(0, 255);
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue++, 360 / (NUM_LEDS - 1));

  // possibly adjust brightness on an interval
  static Metro updateInterval(5UL);
  if ( ! updateInterval.check() ) return;

  static byte mb = 128;

  static int lmg = accel.milligee();
  int mg = accel.milligee();

  int diff = abs(lmg - mg); // could be 2000, at most.
  if ( diff >= 50 && mb < 255 ) mb++;
  if ( diff < 10 && mb > 10) mb--;

  FastLED.setBrightness(mb);

  //  Serial << diff << "\t" << mb << endl;
  //  delay(25);

  lmg = mg;

}

boolean isAround(int c, int y) {
  return ( c >= y - 5 && c <= y + 5 );
}
