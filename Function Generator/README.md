****(Work in progress)****
# Code to create a Function Generator using a RPI pico. #

* Devlopment + Debug environment:
  * VSCode on Win 7
  * Picoprobe running on 2nd RPI pico (foreground)
  * OpenOCD and GDB
* Rotary encoder input using State Machine 0
* Onboard LED flashing using State Machine 1
* Nixie tube display
  * Multiplexed to reduce hardware
  * 170 volt PSU for tubes created from 4 x 1.5v batteries + custom SMPS
* Software generated table of Sine wave values
* 5 bit R2R ADC creating Sine wave output for oscilloscope

![Hardware](https://user-images.githubusercontent.com/3483157/163587205-dd22d308-fde1-4668-b7d6-f42ee1dcb94b.JPG)
