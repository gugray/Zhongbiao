#include <Arduino.h>
#include "globals.h"
#include "events.h"
#include "time_face.h"
#include "set_face.h"

// Definitions of globals
OSO_LCD lcd;
Time time;
Time prevTime;

// Local vars
volatile uint16_t wakeEventMask = 0;

// 0: time; 1: set; 2: tune
volatile uint8_t faceIx = 0;
TimeFace timeFace;
SetFace setFace;


// ========= Buttons =======
// Will move to its own file
// =========================

#define LONG_PRESS_TICKS 44
#define REPEAT_TICKS 8

volatile bool timer0Running = false;
volatile uint16_t counter0 = 0;
volatile uint16_t btnModePressedAt = 0xffff;
volatile uint16_t btnPlusPressedAt = 0xffff;
volatile uint16_t btnMinusPressedAt = 0xffff;

void updateOneButton(volatile uint16_t *pressedAt, uint8_t pin, uint16_t downEventFlag)
{
  // Button is currently pressed
  if (digitalRead(pin) == LOW)
  {
    // Not pressed before
    if (*pressedAt == 0xffff)
    {
      *pressedAt = counter0;
      wakeEventMask |= downEventFlag; // DOWN
    }
    // Was already pressed
    else
    {
      uint16_t elapsed = counter0 - *pressedAt;
      // This is a long press: send once
      if (elapsed == LONG_PRESS_TICKS)
      {
        wakeEventMask |= (downEventFlag << 3); // LONG
        // This is also the first repeat
        wakeEventMask |= (downEventFlag << 1); // REPEAT
      }
      else if (elapsed > LONG_PRESS_TICKS && (elapsed - LONG_PRESS_TICKS) % REPEAT_TICKS == 0)
      {
        wakeEventMask |= (downEventFlag << 1); // REPEAT
      }
    }
  }
  // Button is currently not pressed
  else
  {
    // Was pressed before
    if (*pressedAt != 0xffff)
    {
      // Was it a short press?
      // In the other case, we will have sent out a long press event before
      if (counter0 - *pressedAt < LONG_PRESS_TICKS)
        wakeEventMask |= (downEventFlag << 2); // SHORT
    }
    *pressedAt = 0xffff;
  }
}

bool updateButtons()
{
  updateOneButton(&btnModePressedAt, BTN_MODE_PIN, EVT_BTN_MODE_DOWN);
  updateOneButton(&btnPlusPressedAt, BTN_PLUS_PIN, EVT_BTN_PLUS_DOWN);
  updateOneButton(&btnMinusPressedAt, BTN_MINUS_PIN, EVT_BTN_MINUS_DOWN);

  // Quit timer if no button is pressed
  return btnModePressedAt == 0xffff && btnPlusPressedAt == 0xffff && btnMinusPressedAt == 0xffff;
}

void setupButtons()
{
  pinMode(BTN_MODE_PIN, INPUT_PULLUP);
  pinMode(BTN_PLUS_PIN, INPUT_PULLUP);
  pinMode(BTN_MINUS_PIN, INPUT_PULLUP);
  PCMSK1 |= bit(PCINT11) | bit(PCINT10) | bit(PCINT8);
  PCIFR |= bit(PCIF1);
  PCICR |= bit(PCIE1);
}

void stopTimer0()
{
  timer0Running = false;
  TCCR0B = 0; // Stop Timer 0
  TIMSK0 = 0; // Disable Timer 0 interrupts
}

void setupTimer0()
{
  if (timer0Running == true)
    return;

  counter0 = 0;
  timer0Running = true;
  TCNT0 = 0;                          // clear Timer 2 counter
  TCCR0A = (1 << WGM01);              // clear timer on compare match, no port output
  TCCR0B = (1 << CS02) | (1 << CS00); // prescaler 1024, interrupt enabled.
                                      // This is from 1 MHz CPU clock! CPU *must* be at 1 MHz.
                                      // 1MHz / 1024 = 976.5625
  TIFR0 = 1 << OCF0A;                 // clear compare match A flag
  OCR0A = 19;                         // reaches 20 about 50x per second
  TIMSK0 = 1 << OCIE0A;               // enable compare match interrupt
}

ISR(TIMER0_COMPA_vect)
{
  ++counter0;
  bool stopTimer = updateButtons();
  // Faces that need the quick tick
  if (stopTimer && faceIx == 0)
    stopTimer0();
  wakeEventMask |= EVT_QUICK_TICK;
}

ISR(PCINT1_vect)
{
  // If timer 0 is not running, start it
  setupTimer0();
}

// END button functions
// ================================================

// Forward declarations of local functions
void stopTimer0();
void setupTimer2();
void setupLowPower();
void powerSave();

void setup()
{
  digitalWrite(LED_PIN, LOW);
  pinMode(LED_PIN, OUTPUT);

  lcd.begin();
  timeFace.enter();

  stopTimer0();
  setupButtons();

  setupTimer2();
  setupLowPower();

  // Nightie
  powerSave();
}

void loop()
{
  uint16_t eventMask;
  // While we handle one set of events, other interrupts might be setting other events
  while (true)
  {
    cli();
    eventMask = wakeEventMask;
    wakeEventMask &= ~eventMask;
    sei();
    if (eventMask == 0)
      break;

    uint8_t ret;
    if (faceIx == 0)
    {
      ret = timeFace.loop(eventMask);
      if (ret == RET_NEXT)
      {
        faceIx = 1;
        setFace.enter();
        setupTimer0();
      }
    }
    else if (faceIx == 1)
    {
      ret = setFace.loop(eventMask);
      if (ret != RET_STAY)
      {
        faceIx = 0;
        timeFace.enter();
      }
    }
  }
  powerSave();
}

void setupLowPower()
{
  // For low power in sleep:
  // Disable BOD: Fuses do this
  // Disable ADC
  ADCSRA = 0;
  // Disable WDT
  cli();
  asm("WDR");
  MCUSR &= ~(1 << WDRF);
  WDTCSR |= (1 << WDCE) | (1 << WDE);
  WDTCSR = 0x00;
  sei();
}

void setupTimer2()
{
  TCCR2B = 0;        // stop Timer 2
  TIMSK2 = 0;        // disable Timer 2 interrupts
  ASSR = (1 << AS2); // select asynchronous operation of Timer 2

  // wait for TCN2UB and TCR2BUB to be cleared
  while (ASSR & ((1 << TCN2UB) | (1 << TCR2BUB)))
    ;

  TCNT2 = 0;                          // clear Timer 2 counter
  TCCR2A = (1 << WGM21);              // clear timer on compare match, no port output
  TCCR2B = (1 << CS21) | (1 << CS22); // prescaler 256, interrupt enabled

  // wait for TCN2UB and TCR2BUB to be cleared
  while (ASSR & ((1 << TCN2UB) | (1 << TCR2BUB)))
    ;

  TIFR2 = 1 << OCF2A;   // clear compare match A flag
  OCR2A = 127;          // reaches 128 once per second
  TIMSK2 = 1 << OCIE2A; // enable compare match interrupt
}

ISR(TIMER2_COMPA_vect)
{
  prevTime = time;
  time.tick();
  wakeEventMask |= EVT_SECOND_TICK;
}

void powerSave()
{
  // Disable TWI
  TWCR = bit(TWEN) | bit(TWIE) | bit(TWEA) | bit(TWINT);

  // set sleep mode: power save by default, but only idle if timer0 is alive
  if (timer0Running)
    SMCR = 0;
  else
    SMCR = (1 << SM1) + (1 << SM0);

  cli();
  SMCR |= (1 << SE); // enable sleep
  sei();
  asm("SLEEP");

  // When back: disable sleep
  SMCR &= ~(1 << SE);
}
