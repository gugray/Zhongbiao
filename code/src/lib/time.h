#ifndef TIME_H
#define TIME_H

struct Time
{
  volatile uint8_t hour;
  volatile uint8_t min;
  volatile uint8_t sec;

  void tick();
};

#endif
