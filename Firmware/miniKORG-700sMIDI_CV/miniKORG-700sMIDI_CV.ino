// Compile for Teensy LC as Serial + MIDI

#include <Bounce2.h>

//#define DEBUG
#ifdef DEBUG
#define DEBUG_PRINT(x)     Serial.print (x)
#define DEBUG_PRINTLN(x)   Serial.println (x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

Bounce2::Button button = Bounce2::Button();
byte noteDownCounter = 0;
#define GATEPIN 11
#define OUTPUTINDICATORPIN 9
#define NOTEOUTOFRANGEPIN 8
#define KILLNOTESBUTTONPIN 14
const byte cvPin = A12;

// these values start at midi 48 and are expressed as the target millivolts required by the KORG. These need to be scaled to DAC values.
float noteMilliVolts[] = {1160, 1230, 1300, 1380, 1460, 1550, 1640, 1740, 1840, 1950, 2060, 2190, 2320, 2430, 2600, 2750, 2910, 3090, 3270, 3460, 3670, 3890, 4120, 4360, 4620, 4900, 5190, 5500, 5820, 6160, 6530, 6920, 7320, 7760, 8220, 8710, 9220};
float ampGain = (1496.f / 815.f) + 1;
/*
   ampGain calculation
   Rf    1496r
         ----- = 1.83558282208589 + 1 = 2.8558282208589
   RIn  =  815r
*/
void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  delay(500);
  Serial.flush();
#endif

  // KILL ALL NOTES PIN
  button.attach(KILLNOTESBUTTONPIN, INPUT_PULLUP);
  button.interval(100);
  button.setPressedState(LOW);

  // GATE PIN
  pinMode(GATEPIN, OUTPUT);
  // pull this high on startup
  digitalWriteFast(GATEPIN, HIGH);
  // gate pin is pulled low when the current needs to pass
  // GATE INDICATOR LED
  pinMode(OUTPUTINDICATORPIN, OUTPUT);
  // ERROR LED
  pinMode(NOTEOUTOFRANGEPIN, OUTPUT);

  // ADC
  analogWriteResolution(12);
  analogWrite(cvPin, 0);

  usbMIDI.setHandleNoteOn(OnNoteOn);
  usbMIDI.setHandleNoteOff(OnNoteOff);

  // flash the leds in sequence to show start up has finished
  digitalWriteFast(NOTEOUTOFRANGEPIN, HIGH);
  delay(100);
  digitalWriteFast(NOTEOUTOFRANGEPIN, LOW);

  digitalWriteFast(OUTPUTINDICATORPIN, HIGH);
  delay(100);
  digitalWriteFast(OUTPUTINDICATORPIN, LOW);

  digitalWriteFast(GATEPIN, LOW);
  delay(100);
  digitalWriteFast(GATEPIN, HIGH);
}

void loop() {
  usbMIDI.read();
  button.update();
  if (button.pressed()) allNotesOff();
}

void OnNoteOn(byte channel, byte pitch, byte velocity) {
  // we ignore channel
  DEBUG_PRINT("NoteOn ");
  // constrain pitches to C3 (48) -> C6 (84) in Midi
  // this will limit, don't ignore lower or higher - makes it easier to debug if we at least get some signal.
  // the error light will stay on unless a note off is sent on the out of range note
  if (pitch > 84 || pitch < 48) {
    DEBUG_PRINTLN("out of range");
    digitalWriteFast(NOTEOUTOFRANGEPIN, HIGH);
  } else {
    float korgVal = noteMilliVolts[pitch - 48];
    float scaledVal = ((korgVal / ampGain) / 3300.0);
    float dacVal = scaledVal * 4095;

    analogWrite(cvPin, dacVal);
    digitalWriteFast(GATEPIN, LOW);
    digitalWriteFast(OUTPUTINDICATORPIN, HIGH);
    noteDownCounter++;

    DEBUG_PRINT("note: ");
    DEBUG_PRINT(pitch);
    DEBUG_PRINT(" dac value: ");
    DEBUG_PRINT(dacVal);
    DEBUG_PRINT("\tkorg millivolts: ");
    DEBUG_PRINTLN(korgVal);
  }
}

void OnNoteOff(byte channel, byte pitch, byte velocity) {
  if (pitch > 84 || pitch < 48) {
    digitalWriteFast(NOTEOUTOFRANGEPIN, LOW);
  } else {
    // decrement the notedowncounter so that only the last possible noteoff event of any chord takes the gate signal high
    noteDownCounter--;
    if (noteDownCounter <= 0) {
      digitalWriteFast(GATEPIN, HIGH);
      digitalWriteFast(OUTPUTINDICATORPIN, LOW);
    }
  }
}

void allNotesOff() {
  
  DEBUG_PRINTLN("all notes off");
  noteDownCounter = 0;
  digitalWriteFast(GATEPIN, HIGH);
  digitalWriteFast(OUTPUTINDICATORPIN, LOW);
  digitalWriteFast(NOTEOUTOFRANGEPIN, LOW);
}
