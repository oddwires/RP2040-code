****(Work in progress)****
# Function Generator on a Pimoroni PGA2040. #

* Development + Debug environment:
  * VSCode on Win 7
  * OpenOCD and GDB
  * Picoprobe running on PI Pico providing SWG debugger for target device
* Target dievice changed from Pi Pico to Pimoroni PGA2040 to provide more GPIO pins.
* Nixie tube display
  * Multiplexed to reduce hardware
  * 170 volt PSU for tubes created from 4 x 1.5v batteries + custom SMPS
* 8 bit R2R ADC creating various waveform outputs for oscilloscope display

![Hardware](https://user-images.githubusercontent.com/3483157/163587205-dd22d308-fde1-4668-b7d6-f42ee1dcb94b.JPG)
