# Function Generator on a RP2040. #
* Features:
  * Direct Digital Synthesis (DDS) Function Generator
  * Serial over USB interface (Putty)
  * Dual 8 bit R-2R Digital to Analog converter
  * 1Hz => 1MHz frequency range.
  * Variable output level (requires MCP41020 dual digital potentiometer with SPI interface)
  * Analog clock simulator
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

**Analog Clock Simulator using X-Y mode on a CRT Oscilloscope...**

![Hardware](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/IMG_E1221.JPG)

