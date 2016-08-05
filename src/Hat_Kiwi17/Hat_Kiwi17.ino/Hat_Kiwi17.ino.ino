#include <Streaming.h>
#include <FiniteStateMachine.h>
#include <Metro.h>
#include <Bounce.h>
#include <FastLED.h>

// radio for Shadows
#include <RFM69_Sh.h>
RFM69_Sh radio_Sh;
// radio for Simon
#include <RFM69_Si.h>
RFM69_Si radio_Si;

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
void simonDisplayEnter();
void simonDisplayUpdate();
State simonDisplay = State(simonDisplayEnter, simonDisplayUpdate, NULL);

FSM fsm = FSM(atRest); //initialize state machine, start in state: atRest

#define DATA_PIN    4
#define GND_PIN     5
#define LED_TYPE    WS2812
#define COLOR_ORDER RGB
#define NUM_LEDS    10
CRGB leds[NUM_LEDS];

#define FRAMES_PER_SECOND  20

#define N_UPDATES 10

// tie the pushbutton
#define BUTTON_IN 8
#define BUTTON_GND 9
Bounce calButton = Bounce(BUTTON_IN, 25UL);

#define SHADOWS_GROUP 157
#define SIMON_GROUP 188
const byte Ngroups = 2;
byte groups[Ngroups] = {SHADOWS_GROUP, SIMON_GROUP};
byte currentGroup;

void setup() {
  Serial.begin(115200);
  Serial << F("Cowboyhat Startup.") << endl;
  
  digitalWrite(GND_PIN, LOW);
  pinMode(GND_PIN, OUTPUT);

  pinMode(BUTTON_IN, INPUT_PULLUP);
  digitalWrite(BUTTON_GND, LOW);
  pinMode(BUTTON_GND, OUTPUT);

  // tell FastLED about the LED strip configuration
  FastLED.addLeds<LED_TYPE, DATA_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);

  // start the radio on the Shadows network.
  start_Si_radio();
}

void start_Sh_radio() {
  Serial << F("Shadows: radio startup.") << endl;
  currentGroup=0;

  radio_Sh.initialize(RF69_915MHZ, 212, groups[currentGroup]);
  radio_Sh.setHighPower(); // using RFM69HW board
  radio_Sh.promiscuous(true); // so broadcasts are received
}

void start_Si_radio() {
  Serial << F("Simon: radio startup.") << endl;
  currentGroup=1;

  radio_Si.initialize(RF69_915MHZ, 212, groups[currentGroup]);
  radio_Si.setHighPower(); // using RFM69HW board
  radio_Si.promiscuous(true); // so broadcasts are received
}


void loop() {
  // hear from network colleagues, do a display
  static Metro networkTimeout(10000UL);
  static boolean haveNetworkTraffic = false;
  if ( groups[currentGroup] == SHADOWS_GROUP && radio_Sh.receiveDone() ) {
    networkTimeout.reset();
    haveNetworkTraffic = true;
    fsm.transitionTo(shadowsDisplay);
  }
  if ( groups[currentGroup] == SIMON_GROUP && radio_Si.receiveDone() ) {
    networkTimeout.reset();
    haveNetworkTraffic = true;
    fsm.transitionTo(simonDisplay);
  }

  if ( networkTimeout.check() ) haveNetworkTraffic = false;

  static Metro groupSwapTimer(10000UL);
  if( ! haveNetworkTraffic && groupSwapTimer.check() ) {
    Serial << F("Rotating radio group") << endl;
    if( currentGroup == 0 ) start_Si_radio();
    else if( currentGroup == 1 ) start_Sh_radio();
  }

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

    // check for recalibrate press
    if ( calButton.update() && calButton.read() == LOW ) {
      Serial << F("Calibrating") << endl;
      accel.calibrate();
    }
  }

  fsm.update();

}
void shadowsDisplayEnter() {
  Serial << F("shadowsDisply Enter") << endl;
  // set master brightness control
  FastLED.setBrightness(255);
  // set off
  fill_solid( leds, NUM_LEDS, CRGB(8, 8, 8));
}
void shadowsDisplayUpdate() {
  unsigned long message; // message 
  word distance[3]; // distances from sensors to object
  static word maxd[3] = {600,600,600};
  static word mind[3] = {60,60,60};
  static byte bright[3] = {0,0,0};
  
  if( radio_Sh.receiveDone() && radio_Sh.DATALEN==sizeof(message) ) {

    message = *(unsigned long*)radio_Sh.DATA; 
    
    distance[0] = (message >>  2) & 1023UL; // dumping six MSB
    distance[1] = (message >> 12) & 1023UL; // dumping six MSB
    distance[2] = (message >> 22) & 1023UL; // dumping six MSB
    
    Serial << F("Shadows: distances=") << F("\t") << distance[0] << F("\t") << distance[1] << F("\t") << distance[2] << endl;
    
    for(byte i=0;i<3;i++) {
      if( distance[i] > maxd[i] ) maxd[i] = distance[i];
      if( distance[i] < mind[i] ) mind[i] = distance[i];
 
      bright[i] = map(distance[i], mind[i], maxd[i], 255, 8);
    }

    fill_solid( leds, NUM_LEDS, CRGB(bright[0], bright[1], bright[2]));
    FastLED.show();
  }
  
}
void simonDisplayEnter() {
  Serial << F("simonDisply Enter") << endl;
  // set master brightness control
  FastLED.setBrightness(255);
  // set off
  fill_solid( leds, NUM_LEDS, CRGB(8, 8, 8));
}
void simonDisplayUpdate() {

  static byte lastPacket=99;

  if( radio_Si.receiveDone() && radio_Si.DATALEN==23 ) {

    byte thisPacket = radio_Si.DATA[0];
    if( thisPacket == lastPacket ) return;
    lastPacket = thisPacket;
    
    Serial << F("Simon: packet=") << thisPacket << endl;

    CRGB tower0 = CRGB(radio_Si.DATA[2+0], radio_Si.DATA[2+1], radio_Si.DATA[2+2]);
    CRGB tower1 = CRGB(radio_Si.DATA[5+0], radio_Si.DATA[5+1], radio_Si.DATA[5+2]);
    CRGB tower2 = CRGB(radio_Si.DATA[8+0], radio_Si.DATA[8+1], radio_Si.DATA[8+2]);
    CRGB tower3 = CRGB(radio_Si.DATA[11+0], radio_Si.DATA[11+1], radio_Si.DATA[11+2]);
    CRGB min = CRGB(8,8,8);

    CRGB color = tower0+tower1+tower2+tower3+min;
    
    fill_solid( leds, NUM_LEDS, color );
    FastLED.show();
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

  FastLED.show();
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

  FastLED.show();
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

  FastLED.show();
}

boolean isAround(int c, int y) {
  return ( c >= y - 5 && c <= y + 5 );
}
