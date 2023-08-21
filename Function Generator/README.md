# Function Generator on a RP2040. #
* TBD:
  * Port the clock code from [here](https://www.micro-examples.com/articles/index.php?title=PicOClock)  <-- Changed my mind, going for a full analogue clock face instead

* Features:
  * Direct Digital Synthesis (DDS) Function Generator 1Hz to 1MHz frequency range.
  * Dual 8 bit R-2R Digital to Analog converter
  * Variable output level (requires MCP41020 dual digital potentiometer with SPI interface)
  * Target device = Pico (also works on Pimoroni PGA2040)
  * External, 3 digit, SPI display
  * Independant control of either channel through Putty terminal session over USB port
 
* Limitations:
  * USB serial requires Windows (10 or later)
  * Phase sync mechanism not effective at high frequencies

**Two independanty controled, phase locked, DMA channels...**

![Hardware](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/Capture.JPG)

**Early version of hardware running single channel 150Hz Sine wave + 3rd harmonic on a vintage oscilloscope...**

![Hardware](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/FunctionGenerator.jpg)

