![alt text](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/work-in-progress.gif)****This is a Work In Progress project.****
![alt text](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/work-in-progress.gif)  
(My current thinking is this would all be so much better if it had two channels)
# Function Generator on a Pimoroni PGA2040. #

* Development + Debug environment:
  * VSCode on Win 7
  * OpenOCD and GDB
  * Picoprobe running on PI Pico providing SWG debugger
* Direct digital synthesis (DDS) Function Generator
  * Target device = Pimoroni PGA2040 to maximise GPIO connections
  * Nixie tube display with 170 volt SMPU
  * Digital level control
  * 8 bit R2R Analog to Digital converter

![Hardware](https://github.com/oddwires/RP2040/blob/master/Function%20Generator/Images/FunctionGenerator.jpg)

(150Hz Sine wave + 3rd harmonic on my GEC MiniScope)
