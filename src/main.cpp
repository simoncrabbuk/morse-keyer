#include <Arduino.h>

/*
 * Arduino Nano Iambic Keyer (Mode B Squeeze Logic with Built-in Serial Decoder)
 * Pins:
 *   D2: Dit Paddle (active LOW, internal pull-up)
 *   D3: Dah Paddle (active LOW, internal pull-up)
 *   D4: Audio Sidetone Out (700 Hz square wave)
 *   D13: Onboard LED (flashes with tone)
 *   A0: Speed Potentiometer Wiper (outer legs to 5V and GND)
 */

const int PIN_DIT   = 2;
const int PIN_DAH   = 3;
const int PIN_AUDIO = 4;
const int PIN_LED   = LED_BUILTIN;
const int PIN_SPEED = A0;

const int SIDETONE_FREQ = 700;

const int MIN_WPM = 6;
const int MAX_WPM = 35;

// Debounce / Lockout timing
const unsigned long LOCKOUT_MS = 8;
unsigned long ditLastChange = 0;
unsigned long dahLastChange = 0;
bool ditStable = false;
bool dahStable = false;

enum KeyerState { IDLE, SEND_ELEMENT, PAUSE_ELEMENT };
KeyerState state = IDLE;

enum ElementType { NONE, DIT, DAH };
ElementType currentElement = NONE;
ElementType lastElement    = NONE;

unsigned long elementStartTime = 0;
unsigned long elementDuration  = 0;
unsigned long idleStartTime    = 0;

bool ditMemory = false;
bool dahMemory = false;

// Decoder buffer and tracking
String morseBuffer = "";
bool charDecoded = true;
bool wordSpaceSent = true;

// Morse Code Lookup Table
struct MorseMap {
  const char* pattern;
  char character;
};

const MorseMap morseTable[] = {
  {".-", 'A'},    {"-...", 'B'},  {"-.-.", 'C'},  {"-..", 'D'},
  {".", 'E'},     {"..-.", 'F'},  {"--.", 'G'},   {"....", 'H'},
  {"..", 'I'},    {".---", 'J'},  {"-.-", 'K'},   {".-..", 'L'},
  {"--", 'M'},    {"-.", 'N'},    {"---", 'O'},   {".--.", 'P'},
  {"--.-", 'Q'},  {".-.", 'R'},   {"...", 'S'},   {"-", 'T'},
  {"..-", 'U'},   {"...-", 'V'},  {".--", 'W'},   {"-..-", 'X'},
  {"-.--", 'Y'},  {"--..", 'Z'},
  {"-----", '0'}, {".----", '1'}, {"..---", '2'}, {"...--", '3'},
  {"....-", '4'}, {".....", '5'}, {"-....", '6'}, {"--...", '7'},
  {"---..", '8'}, {"----.", '9'},
  {".-.-.-", '.'}, {"--..--", ','}, {"..--..", '?'}, {"-..-.", '/'},
  {"-....-", '-'}, {".-... ", '&'}, {".-.-.", '+'},  {"-...-", '='}
};

char decodeMorse(const String& code) {
  for (size_t i = 0; i < sizeof(morseTable) / sizeof(morseTable[0]); i++) {
    if (code.equals(morseTable[i].pattern)) {
      return morseTable[i].character;
    }
  }
  return '?'; // Unrecognized pattern
}

int getDitLength() {
  int potVal = analogRead(PIN_SPEED);
  int wpm = map(potVal, 0, 1023, MIN_WPM, MAX_WPM);
  return 1200 / wpm;
}

void toneOn() {
  tone(PIN_AUDIO, SIDETONE_FREQ);
  digitalWrite(PIN_LED, HIGH);
}

void toneOff() {
  noTone(PIN_AUDIO);
  digitalWrite(PIN_LED, LOW);
}

void updatePaddles() {
  unsigned long now = millis();

  bool rawDit = (digitalRead(PIN_DIT) == LOW);
  if (rawDit != ditStable && (now - ditLastChange >= LOCKOUT_MS)) {
    ditStable = rawDit;
    ditLastChange = now;
  }

  bool rawDah = (digitalRead(PIN_DAH) == LOW);
  if (rawDah != dahStable && (now - dahLastChange >= LOCKOUT_MS)) {
    dahStable = rawDah;
    dahLastChange = now;
  }
}

void setup() {
  Serial.begin(115200);
  pinMode(PIN_DIT, INPUT_PULLUP);
  pinMode(PIN_DAH, INPUT_PULLUP);
  pinMode(PIN_AUDIO, OUTPUT);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, LOW);

  Serial.println("\n--- Iambic Keyer Ready ---");
}

void loop() {
  int ditLen = getDitLength();
  unsigned long now = millis();

  updatePaddles();

  // Mode B Latch Rules
  if (state == SEND_ELEMENT) {
    if (now - elementStartTime > (elementDuration / 2)) {
      if (currentElement == DIT && dahStable) dahMemory = true;
      if (currentElement == DAH && ditStable) ditMemory = true;
    }
  } else if (state == PAUSE_ELEMENT) {
    if (lastElement == DIT && dahStable) dahMemory = true;
    if (lastElement == DAH && ditStable) ditMemory = true;
  } else if (state == IDLE) {
    if (ditStable) ditMemory = true;
    if (dahStable) dahMemory = true;
  }

  switch (state) {
    case IDLE:
      // Check for Character Space (3 dit units of silence)
      if (!charDecoded && (now - idleStartTime >= (unsigned long)ditLen * 3)) {
        if (morseBuffer.length() > 0) {
          Serial.print(decodeMorse(morseBuffer));
          morseBuffer = "";
        }
        charDecoded = true;
      }

      // Check for Word Space (7 dit units of silence)
      if (charDecoded && !wordSpaceSent && (now - idleStartTime >= (unsigned long)ditLen * 7)) {
        Serial.print(" ");
        wordSpaceSent = true;
      }

      if (ditMemory && dahMemory) {
        currentElement = (lastElement == DIT) ? DAH : DIT;
      } else if (ditMemory) {
        currentElement = DIT;
      } else if (dahMemory) {
        currentElement = DAH;
      } else {
        currentElement = NONE;
      }

      if (currentElement != NONE) {
        if (currentElement == DIT) {
          ditMemory = false;
          elementDuration = ditLen;
          morseBuffer += ".";
        } else {
          dahMemory = false;
          elementDuration = ditLen * 3;
          morseBuffer += "-";
        }
        charDecoded = false;
        wordSpaceSent = false;

        toneOn();
        elementStartTime = now;
        state = SEND_ELEMENT;
      }
      break;

    case SEND_ELEMENT:
      if (now - elementStartTime >= elementDuration) {
        toneOff();
        lastElement = currentElement;
        elementDuration = ditLen; // 1 dit unit gap between elements
        elementStartTime = now;
        state = PAUSE_ELEMENT;
      }
      break;

    case PAUSE_ELEMENT:
      if (now - elementStartTime >= elementDuration) {
        if (lastElement == DIT && ditStable) ditMemory = true;
        if (lastElement == DAH && dahStable) dahMemory = true;

        if (ditMemory && dahMemory) {
          currentElement = (lastElement == DIT) ? DAH : DIT;
        } else if (ditMemory) {
          currentElement = DIT;
        } else if (dahMemory) {
          currentElement = DAH;
        } else {
          currentElement = NONE;
        }

        if (currentElement != NONE) {
          if (currentElement == DIT) {
            ditMemory = false;
            elementDuration = ditLen;
            morseBuffer += ".";
          } else {
            dahMemory = false;
            elementDuration = ditLen * 3;
            morseBuffer += "-";
          }
          toneOn();
          elementStartTime = now;
          state = SEND_ELEMENT;
        } else {
          idleStartTime = now;
          state = IDLE;
        }
      }
      break;
  }
}