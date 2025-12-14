# AgIsoVirtualTerminal Headless

Headless ISOBUS Virtual Terminal server with Bluetooth CAN support. Implements ISO 11783-6 VT protocol (Version 6, 800x480, 64 virtual soft keys) using [AgIsoStack++](https://github.com/Open-Agriculture/AgIsoStack-plus-plus). Supports automatic object pool reception, persistent storage, and rendering of common VT objects. Built with C++17 and CMake.

## Build
```bash
mkdir build && cd build
cmake .. && make -j8
./AgISOVirtualTerminalHeadless
```

## Features
Bluetooth CAN connectivity via CoreBluetooth (macOS), object pool persistence in `iso_data/`, VT color palette support, and rendering for OutputString, OutputNumber, OutputRectangle, OutputLine, OutputEllipse, Button, Container, DataMask, AlarmMask, ObjectPointer, and PictureGraphic objects.
