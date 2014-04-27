# JY-MCU Memory Module V1.0
## License and attribution
This program is based on Peter Fleury's UART library and Ulricht Radig's MMC-SD Library which are both licensed under the GPL.
This code is also Licensed under the GNU General Public License.

## Hardware setup

This program is a little tool that allows you to access an SD Card in the **Jiayuan JY-MCU SD Card Module V1.0**. It is tested with the following configuration:
MCU: Atmega644
Board: Pollin Evaluationsboard

Pin connection:
```
____________          ________________
|       PB1 |--------| CS            |
|       PB5 |--------| MOSI = DO     |
|       PB6 |--------| MISO = DI     |
| AVR   PB7 |--------| SCLK  SD Card |
|       GND |--------| GND           |
|       +5V |--------| +5V           |
|___________|        |_______________|
```

You don't need to connect any other wiring,
no need for a second GND or a +3.3V connection.
Actually MOSI = DI and MISO = DO, but it seems like
this is labelled the wrong way round on the
Jiayuan JY-MCU SD Card Module V1.0

**Do not use SDHC cards, use MMC or SD cards!**

## Installation

Just compile this code using `make` and flash it using `make program` (you may need to change the Makefile for that to select your programmer, port etc.) to your ATMega 644 (other MCUs require definition changes, should work but aren't tested. Make sure you get the wiring configuration for other MCUs in mmc.h right).

## Usage
Connect to your atmega using a serial cable with a Terminal at 57600 baud (tested with GtkTerm) and reset the Microcontroller. This will drop you in some type of shell, no matter if the SD Card is connected and working or not. Just type `help` and get started.
In order to write files to your SD / MMC you should have created an empty, large file before.
If the SD Card could not be initialized while booting, type `init` in the shell. If it does not work, make sure you got the pinning right, you may want to try a different SD Card or try if you swapped two pins (You might have got MOSI / MISO wrong).

## Have fun!
