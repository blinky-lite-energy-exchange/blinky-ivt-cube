# Blinky IVT Cube
The Blinky-IVT cube controls an infrared diode to send pulses to an IVT heat pump. The pulse sequence is defined in the IVT.h file in the cubeCode folder. The pulse sequence was modified from [ToniA/arduino-heatpumpir](https://github.com/ToniA/arduino-heatpumpir). The [ToniA/arduino-heatpumpir](https://github.com/ToniA/arduino-heatpumpir) project contains pulse sequences for 29 different models of heat pumps so it possible to easily extend the Blinky IVT cube to many different models of heat pumps. The The microcontroller is a [Raspberry Pi Pico W](https://www.raspberrypi.com/products/raspberry-pi-pico/) that connects wirelessly to soft Blinky-Lite tray the resides on a Blinky-Lite box.

The Blinky IVT Cube sends and receives data from the [Blinky IVT tray](https://github.com/blinky-lite-energy-exchange/blinky-ivt-tray).

<img src="ivt.png"/><br>
