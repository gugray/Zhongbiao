#ifndef TIME_FACE_H
#define TIME_FACE_H

struct TimeFace
{
  static void enter();
  static uint8_t loop(uint16_t event);
};

#endif
