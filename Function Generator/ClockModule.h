#pragma once

#include "DAC_Class.h"
#include "pico/stdio.h"
#include "pico/time.h"

extern uint8_t FaceX[], FaceY[];
extern uint8_t HandsX[], HandsY[];
extern int tmp, LEDctr, Parm[], Hours, Mins, Secs;
extern char MarginVW[], Margin[], inStr[];
extern void getLine();
extern struct repeating_timer timer;

bool ClockModule ( DAC DACobj[] );