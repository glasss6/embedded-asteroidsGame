# embedded-asteroidsGame
Terminal-based Asteroids game for embedded platforms

This project recreates the classic Asteroids game in a terminal application (e.g PuTTY, TeraTerm) over UART protocol. This uses [embedded-software library](https://github.com/muhlbaier/embedded-software) for UART abstraction, terminal functions, and task management systems. Therefore, this project can be run on any embedded platform supported for the embedded-software library. The project was developed and tested using a MSP430F5529.

## Usage
The UART communication is configured to utilize the back-channel port at 460800 baud. A USB to TTL/FTDI converter is recommended. On MSP430 processors it is highly recommended to NOT use the built-in Application UART due to slow speeds. 

## PuTTY Configuration
Any terminal with Serial communication support is suitable by this README will discuss configuration using PuTTY.
* Set baud rate to 460800
* In "Window" menu set columns and rows to appropiate size (game configured for 60 x 25 by default)
* In "Window" -> "Translation" menu set "Remote character set" to "CP866"

## Running the game
With the terminal open and the code running, type "$game fly1 play" to start playing the game

## Prerequistes for building code
This project uses [embedded-software library](https://github.com/muhlbaier/embedded-software). Please download and refer to library documentation to configure the project for your embedded platform.

## Author
* Stephen Glass - [https://stephen.glass](https://stephen.glass)

## License
This project is licensed under the MIT License - see the [LICENSE.md](LICENSE.md) file for details.

## Screenshot
![screen of game](screenshot.jpg)