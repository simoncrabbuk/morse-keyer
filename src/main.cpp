#include <Arduino.h>

/*
 * Arduino Nano Iambic Keyer (Mode B Squeeze Logic with Built-in Serial Decoder)
 * Pins:
 *   D2: Dit Paddle (active LOW, internal pull-up)
 *   D3: Dah Paddle (active LOW, internal pull-up)
 *   D4: Audio Sidetone Out (700 Hz square wave)
 *   D13: Onboard LED (flashes with tone)
 *   A7: Speed Potentiometer Wiper (outer legs to 3V3 and GND)
 */

const int PIN_DIT   = 2;
const int PIN_DAH   = 3;
const int PIN_AUDIO = 4;
const int PIN_LED   = LED_BUILTIN;
const int PIN_SPEED = A7;

const int SIDETONE_FREQ   = 700;
const int SPEED_PIP_FREQ   = 1000; // Hz for milestone beep
const int SPEED_PIP_MS     = 12;   // Click duration (ms)
const int SPEED_PIP_GAP_MS = 25;   // Silence gap between double clicks (ms)

const int MIN_WPM = 5;
const int MAX_WPM = 35;
const int POT_MIN_ADC = 10;   // ADC 0-10 maps to 5 WPM
const int POT_MAX_ADC = 850;  // ADC 850+ maps to 35 WPM

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

// Non-blocking speed pip generator
int pipBeepsRemaining = 0;
unsigned long pipNextActionTime = 0;
bool pipToneIsOn = false;

void triggerPip(bool isDouble) {
  if (state != IDLE) return;
  tone(PIN_AUDIO, SPEED_PIP_FREQ);
  pipToneIsOn = true;
  pipNextActionTime = millis() + SPEED_PIP_MS;
  pipBeepsRemaining = isDouble ? 1 : 0;
}

void updatePip() {
  if (pipNextActionTime == 0) return;
  if (state != IDLE) {
    pipNextActionTime = 0;
    pipBeepsRemaining = 0;
    pipToneIsOn = false;
    return;
  }

  unsigned long now = millis();
  if (now >= pipNextActionTime) {
    if (pipToneIsOn) {
      noTone(PIN_AUDIO);
      pipToneIsOn = false;
      if (pipBeepsRemaining > 0) {
        pipNextActionTime = now + SPEED_PIP_GAP_MS;
      } else {
        pipNextActionTime = 0;
      }
    } else {
      if (pipBeepsRemaining > 0) {
        tone(PIN_AUDIO, SPEED_PIP_FREQ);
        pipToneIsOn = true;
        pipNextActionTime = now + SPEED_PIP_MS;
        pipBeepsRemaining--;
      } else {
        pipNextActionTime = 0;
      }
    }
  }
}

void checkSpeedPip(int currentWpm) {
  static int lastWpm = -1;
  static int lastBeepedMilestone = -1;

  if (lastWpm == -1) {
    lastWpm = currentWpm;
    if (currentWpm % 5 == 0) {
      lastBeepedMilestone = currentWpm;
    }
    return;
  }

  if (currentWpm != lastWpm) {
    int low = min(lastWpm, currentWpm);
    int high = max(lastWpm, currentWpm);

    for (int m = 5; m <= MAX_WPM; m += 5) {
      if (m >= low && m <= high) {
        if (m != lastBeepedMilestone) {
          bool isDouble = (m % 10 == 0);
          triggerPip(isDouble);
          lastBeepedMilestone = m;
          break;
        }
      }
    }

    if (lastBeepedMilestone != -1 && abs(currentWpm - lastBeepedMilestone) >= 2) {
      lastBeepedMilestone = -1;
    }

    lastWpm = currentWpm;
  }
}

int getDitLength() {
  int potVal = analogRead(PIN_SPEED);
  int clampedPot = constrain(potVal, POT_MIN_ADC, POT_MAX_ADC);
  int wpm = map(clampedPot, POT_MIN_ADC, POT_MAX_ADC, MIN_WPM, MAX_WPM);

  checkSpeedPip(wpm);

  static int lastReportedPot = -999;
  if (abs(potVal - lastReportedPot) > 8) {
    lastReportedPot = potVal;
    Serial.print("[Speed] ADC A7: ");
    Serial.print(potVal);
    Serial.print(" -> ");
    Serial.print(wpm);
    Serial.println(" WPM");
  }

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
  updatePip();

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