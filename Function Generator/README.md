# Function Generator on a RP2040. #
* TBD:
  * Port the clock code from [here](https://www.micro-examples.com/articles/index.php?title=PicOClock)

* Features:
  * Direct Digital Synthesis (DDS) Function Generator
  * Dual 8 bit R-2-R Digital to Analog converter
  * Target device = Pimoroni PGA2040 (header can be modified to use Pico)
  * External, 3 digit, SPI display
  * Independant control of either channel through Putty terminal session
  * Putty terminal session over USB port  


**Two independanty controled, phase locked, DMA channels...**

![Hardware](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/Capture.JPG)

**Early version of hardware running single channel 150Hz Sine wave + 3rd harmonic on a vintage oscilloscope...**

![Hardware](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/FunctionGenerator.jpg)

