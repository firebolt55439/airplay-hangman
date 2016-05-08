# airplay-hangman

A hangman game written in C++ that has a lot to offer:

- Can be broadcasted to any Airplay-compatible device in the LAN (or even via Bluetooth)
- Advertises itself as a Bonjour service and searches for Airplay devices via Bonjour
as well, allowing for easy and quick discovery of both this service and any Apple TV,
iPad, etc.
- Provides a web interface on port 8080 at the local IP address that can be easily accessed
over LAN and provides a sleek, resizeable interface that adapts to conform to any screen
size or bounds, from a 200x300 up to a 4K display.

Check it out! Can be executed by simply running the compiled executable (provided
in the obj/ directory) on any Mac, or recompiling using Cygwin or a similar
layer for Windows, and should compile on Linux without any problems.
