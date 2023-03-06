#include <Arduino.h>
#include "measure_face.h"
#include "events.h"
#include "globals.h"

static uint16_t timeoutStart;

static void draw();

uint8_t MeasureFace::loop(uint16_t event)
{
  if (ISEVENT(EVT_ENTER_FACE))
  {
    timeoutStart = (totalSeconds & 0xffff);
    draw();
    return RET_STAY;
  }
  if (ISEVENT(EVT_BTN_MODE_SHORT))
  {
    return RET_NEXT;
  }
  if (ISEVENT(EVT_BTN_MODE_LONG))
  {
    timeoutStart = (totalSeconds & 0xffff);
    dsState |= 0x01; // Kick off temp conversion. Main loop will handle this.
    draw();
  }
  if (ISEVENT(EVT_SECOND_TICK))
  {
    uint16_t elapsed = (totalSeconds & 0xffff) - timeoutStart;
    if (elapsed > TIMEOUT_SECONDS)
    {
      return RET_HOME;
    }
    draw();
  }
  return RET_STAY;
}

static void draw()
{
  lcd.fill(false);
  lcd.buffer[5] = 0b11001010; // degree
  lcd.buffer[6] = 0b11010001; // C
  // Converting
  if (dsState != 0)
  {
    lcd.buffer[2] = 0b00000010;
    lcd.buffer[3] = 0b00000010;
    lcd.buffer[4] = 0b00000010 | OSO_SYMBOL_DOT;
  }
  // Got a reading
  else
  {
    uint16_t val = latestMeasuredTemp;
    lcd.buffer[4] = digits[val % 10] | OSO_SYMBOL_DOT;
    val /= 10;
    lcd.buffer[3] = digits[val % 10];
    val /= 10;
    if (val > 0)
      lcd.buffer[2] = digits[val % 10];
  }
  lcd.show();
}
