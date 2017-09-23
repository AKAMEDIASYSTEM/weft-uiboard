#include <Encoder.h>
#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define DISPLAY_PRESENT false

#define sw 8 // encoder's pushbutton switch
#define pwmOut 3 // PWM out for boost converter
#define OLED_RESET 4 // not really conected, needed for adafruit oled lib
Adafruit_SSD1306 display(OLED_RESET);

#define NUMFLAKES 10
#define XPOS 0
#define YPOS 1
#define DELTAY 2
#define LOGO16_GLCD_HEIGHT 16
#define LOGO16_GLCD_WIDTH  16
static const unsigned char PROGMEM logo16_glcd_bmp[] =
{ B00000000, B11000000,
  B00000001, B11000000,
  B00000001, B11000000,
  B00000011, B11100000,
  B11110011, B11100000,
  B11111110, B11111000,
  B01111110, B11111111,
  B00110011, B10011111,
  B00011111, B11111100,
  B00001101, B01110000,
  B00011011, B10100000,
  B00111111, B11100000,
  B00111111, B11110000,
  B01111100, B11110000,
  B01110000, B01110000,
  B00000000, B00110000
};

#if (SSD1306_LCDHEIGHT != 64)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

// Change these two numbers to the pins connected to your encoder.
//   Best Performance: both pins have interrupt capability
//   Good Performance: only the first pin has interrupt capability
//   Low Performance:  neither pin has interrupt capability
Encoder myEnc(6, 5);
//   avoid using pins with LEDs attached


float minFreq = 0.0125 * 1000.0; // 0.0125 is too low for some people to feel, trying 0.016 now
float maxFreq = 0.2 * 1000.0;
float minDuty = 0.3;
float maxDuty = 0.98;
float dutyCycle = 0.95;
float minAmpl = 0.0; // not tested
float maxAmpl = 2000.0; // not tested
float DACamplitude = 2000.0; // hardcoded to maximum

float phase = 0.0;
float twopi = 3.14159 * 2;
float phaseOffset = 0.05;
volatile int wavelabelV = 0;
int wavelabel = 0;
int wavelabelOld = 1;
int encUpLimit = 300;
int encLoLimit = 0;
long newPosition = 0;
long oldPosition  = -999;
char* wavelabels[] = {"SINE", "SQUARE", "SAW_DESC", "SAW_ASC", "NOISE"};

void setup() {
  pinMode(sw, INPUT_PULLUP);
  pinMode(A14, OUTPUT);
  attachInterrupt(sw, swTriggered, FALLING);
  analogWriteResolution(12);
//  analogWriteFrequency(pwmOut, 375000*4);
analogWriteFrequency(pwmOut, 375000);
  pinMode(pwmOut, OUTPUT);
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.display();
  display.clearDisplay();
}

void loop() {
  readEnc();
  noInterrupts();
  wavelabel = wavelabelV;
  interrupts();

  analogWrite(pwmOut, int(4096 * dutyCycle)); // duty cycle is a % and should have been dynamically calculated before here

  // calculate and update the phase accumulator
  phaseOffset = (4 * phaseOffset + constrain(map(newPosition, encLoLimit, encUpLimit, minFreq, maxFreq), minFreq, maxFreq)) / 5;
  phase = phase + (phaseOffset / 1000.0);
  if (phase >= twopi) {
    phase = 0;
  }
  calculateFeedback(); // this doesn't exist yet
  float DACval = 1;
  switch (wavelabel) {
    case 0: // SINE
      DACval = sin(phase) * 2000.0 + 2050.0; // amplitude adjustment should occur here
      break;
    case 1: // SQUARE
      // if phase > pi then 1 else 0
      (phase > twopi / 2) ? (DACval = (DACamplitude / maxAmpl) * 4095.0) : (DACval = 0.0);
      break;
    case 2: // SAW_ASC
      // phase itself is linearly ramping
      DACval = floatmap(phase, 0, twopi, 1.0, 0.0) * (DACamplitude / maxAmpl) * 4095.0;
      break;
    case 3: // SAW_DESC
      // phase itself is linearly ramping
      DACval = floatmap(phase, 0, twopi, 0.0, 1.0) * (DACamplitude / maxAmpl) * 4095.0;
      break;
    case 4: // NOISE
      // check this on the scope!
      (random(0, 9) > 4.5) ? (DACval = (DACamplitude / maxAmpl) * 4095.0) : (DACval = 0.0);
      break;
    default: // SINE
      DACval = sin(phase) * 2000.0 + 2050.0; // amplitude adjustment should occur here
      break;
  }

  analogWrite(A14, (int)DACval);
  // only update display if something changed
  if ((newPosition != oldPosition) || (wavelabel != wavelabelOld) ) {
    oldPosition = newPosition;
    wavelabelOld = wavelabel;
    updateDisplay(newPosition);
  }
}

void swTriggered() {
  // interrupt handling pushbotton presses, incrementing wave type
  wavelabelV = (wavelabelV + 1) % 5;
}

void readEnc() {
  // read knob and
  newPosition = myEnc.read();
  if (newPosition > encUpLimit) {
    myEnc.write(encUpLimit);
  }
  if (newPosition < encLoLimit) {
    myEnc.write(encLoLimit);
  }
}
