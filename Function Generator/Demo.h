#pragma once

#include "DAC_Class.h"
#include "pico/stdlib.h"

extern int Parm[] ;
extern int SetVal(DAC DACobj[], int _Parm, int _Value) ;
extern char MarginVW[], Margin[], inStr[] ;

bool SweepParm (DAC DACobj[], int _parm, int _start, int _stop, int _speed, int pause) ;
void Demo_01 (DAC DACobj[]) ;
void Demo_02 (DAC DACobj[]) ;