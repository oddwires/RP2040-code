# Analog Clock on an RP2040. #

There is also an asscociated <a href="https://www.instructables.com/Oscilloscope-Clock-on-a-Raspberry-Pi-Pico/" target="_blank" />Instructable</a> detailing the hardware.

https://github.com/oddwires/RP2040-code/assets/3483157/f9f16b66-ea9f-45c7-9ec1-8131fb11000c

### Features:
 * Two independent 8-bit Digital to Analog (DAC) channels using R-2R networks.
 * Oscilloscope XY mode to plot an analogue clock.
 * C++ program
 * Single USB provides power, programming and control.
 * Solder-less construction.
  
### Supported commands
 * ? - Help
 * T - Set time
   * Notation: HH:MM:SS or HH,MM,SS
   * HH can be either 12 or 24 hour notation. e.g. '03:00:00' is the same as '15:00:00'
   * Delimiter can be either ':' or ','. e.g. '15:00:00' is the same as '15,00,00'
   * MM is in the range 0<=MM<=59
   * SS is in the range 0<=SS<=59
   * Leading zeros can be omitted. e.g. '1:2:3' is the same as '01:02:03'
   * Trailing parameters can be omitted. e.g. '12:15' is the same as '12:15:00'
 * L - Set level
   * Notation: 2 digit percentage 0<=NN<=100
 * V - Version info
 * X - Invert X axis
 * Y - Invert Y axis

### Limitations:
  * USB serial requires Windows (10 or later)
