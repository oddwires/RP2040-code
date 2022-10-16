![alt text](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/work-in-progress.gif)****Work In Progress.****
![alt text](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/work-in-progress.gif)
* TBD:
  * Move the display to an SPI module to free up some additional GPIO ports.
  * Use the additional GPIO ports to implement a second DMA channel and add code for Lissajou figures.
  * Port the clock code from [here](https://www.micro-examples.com/articles/index.php?title=PicOClock)

# Function Generator on a RP2040. #

* Direct digital synthesis (DDS) Function Generator
  * Target device = Pimoroni PGA2040 to maximise GPIO connections
  * Nixie tube display with 170 volt SMPU
  * Digital level control
  * Single 8 bit R2R Digital to Analog converter

![Hardware](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/FunctionGenerator.jpg)

(150Hz Sine wave + 3rd harmonic on a vintage GEC MiniScope)
