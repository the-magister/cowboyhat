#ifndef Accelerometer_h
#define Accelerometer_h

#include <Arduino.h>

/* Accelerometer:
 * Receive input from a 3-axis device, and perform some useful calculations.
 *
 * Specify the three axis pins, and the VCC and GND pins, using analog pin
 * numbers.  These are usually adjacent on the common breakout boards.
 *
 * Call the accelerometer's update() method occasionally to update the
 * current values from the hardware.
 *
 * Get the axis acceleration features with the axis() method, or other useful
 * calculations such as milligee(), pitch() and roll().
 *
 * Note that some functions like pitch() and roll() use floating point math,
 * and can therefore really increase the size of the program on the chip.
 * You can save about 2000 bytes if you don't need any floating point math.
 */
#define ANALOG0 14

class Accelerometer
{
  int p[3]; // which analog pins
  int a[3]; // acceleration, zero-based
  int b[3]; // acceleration bias/calibration information
  int g, t, r; // cached copies of calculations
  int scale; // scaling factor between ADC and gravity

public:
  Accelerometer(int pinX, int pinY, int pinZ)
  {
    pinMode((p[0] = pinX)+ ANALOG0, INPUT);
    pinMode((p[1] = pinY)+ ANALOG0, INPUT);
    pinMode((p[2] = pinZ)+ ANALOG0, INPUT);
    for (int i = 0; i < 3; i++)
      b[i] = 512;
    g = t = r = 0;
    scale = 100;
  }

  void update()
  {
    for (int i = 0; i < 3; i++)
      a[i] = analogRead(p[i]) - b[i];
    g = t = r = 0;
  }

  void dump()
  {
    Serial.print(  "x="); 
    Serial.print(a[0]);
    Serial.print("\ty="); 
    Serial.print(a[1]);
    Serial.print("\tz="); 
    Serial.print(a[2]);
    Serial.print("\tmg="); 
    Serial.print(milligee());
    Serial.print("\tpitch="); 
    Serial.print(pitch());
    Serial.print("\troll="); 
    Serial.print(roll());
    Serial.println();
  }

  void calibrate()
  {
    for (int i = 0; i < 3; i++)
      b[i] = analogRead(p[i]);
    b[2] -= scale;
    update();
  }

  int milligee()
  {
    if (g != 0) return g;
    long squared = 0.0;
    for (int i = 0; i < 3; i++)
      squared += (long)a[i] * (long)a[i];
    g = squared * 1000 / (scale*scale);
    return g;
  }

  int accel(int axis)
  {
    if (axis < 0 || axis > 3) return 0;
    return a[axis];
  }

  int roll()
  {
    if (r != 0) return r;
    r = (int)(atan2(a[0], a[2]) * 180. / M_PI);
    return r;
  }

  int pitch()
  {
    if (t != 0) return t;
    t = (int)(acos(a[1] / (float)scale) * 180. / M_PI);
    return t;
  }

  void loop()
  {
    update();
  }
};


#endif

