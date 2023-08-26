# Analog Clock on an RP2040. #

* Features:
  * Direct Digital Synthesis (DDS)
  * Dual 8 bit R-2R Digital to Analog converter
  * Target device = Pico (also works on Pimoroni PGA2040)
  * Set time through Putty terminal session over USB port
 
![Hardware](https://github.com/oddwires/RP2040/blob/master/Clock/Images/Pico_DAC_bb.jpg)

![Hardware](https://github.com/oddwires/RP2040/blob/master/Clock/Images/IMG_1215.JPG)

* Limitations:
  * USB serial requires Windows (10 or later)
  * The code is working, but is a cut down version of the Function Generator code. This has resulted in some redundant code + variables that need tidying up.
