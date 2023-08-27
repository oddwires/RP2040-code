# Analog Clock on an RP2040. #

* Features:
  * Direct Digital Synthesis (DDS)
  * Dual 8 bit R-2R Digital to Analog converter
  * Target device = Pico (also works on Pimoroni PGA2040)
  * Set time through Putty terminal session over USB port
 
![Hardware](https://github.com/oddwires/RP2040/blob/master/Clock/Images/Pico_DAC_bb.jpg)

![Hardware](https://github.com/oddwires/RP2040/blob/master/Clock/Images/IMG_1215.JPG)

* Supported commands
  * ?   - Help
  * V   - Version info
  * S   - Set time: Notation: HH:MM:SS or HH,MM,SS<br>
HH can be either 12 or 24 hour notation.  e.g. '03:00:00' is the same as '15:00:00'<br>
Delimiter can be either ':' or ','. e.g. '15:00:00' is the same as '15,00,00'<br>
MM is in the range 0<=MM<=59<br>
SS is in the range 0<=SS<=59<br>
Leading zeros can be ommited. e.g. '1:2:3' is the same as '01:02:03'<br>
Trailing parameters can be ommited, and will be set to zero. e.g. '12:15' is the same as '12:15:00' and '12' is the same as '12:00:00'

* Limitations:
  * USB serial requires Windows (10 or later)
