
# DAPLink Users Guide
There are three interface that DAPLink provides.  These are drag-and-drop programming, a serial port and debugging support.

## Drag-and-drop Programming
Program the target microcontroller by copying or saving a file in one of the supported formats to the DAPLink drive.  Apon completion the drive will re-mount.  If a failure occurs then the file FAIL.TXT will appear on the drive containing information about the failure.

Supported file formats
* Raw binary file
* Intel Hex

## Serial Port

Supported baud rates:
* 9600
* 14400
* 19200
* 28800
* 38400
* 56000
* 57600
* 115200

Note - Most DAPLink implementations support  suppport b



## Debugging

## Firmware Update

To update the firmware on a device hold the reset button while attaching USB.  The device will boot into bootloader mode.  From there copy the appropriate firmware on to the drive.  If successful the device will leave bootloader mode and start running the new firmware.  Otherwise the bootloader will display FAIL.TXT with an explination of what went wrong.