Name: Readme.txt
Author: Denis PEROTTO, from work published by Arduino
Copyright: Denis PEROTTO
License: GPL http://www.gnu.org/licenses/gpl-2.0.html
Project: TFTP Bootloader
Function: Project documentation
Contact : denis dot perotto at gmail dot com 
Compilator : gcc version 4.3.3 (WinAVR 20100110)
Version: 0.2 tftp functional on 328P
-------------------------------------------------
Ethernet bootloader for the ATmega328P / W5100
Tested on Arduino Ethernet
-------------------------------------------------

How it works
------------
This bootloader starts a TFTP server on default UDP Port 69, and listens for about 5 seconds at startup. During this time the user can upload
a new firmware using any TFTP client software.

It manages interrupted file transfers even if a power failure succeed.

Size
----
At this time the bootloader's size is about 1500 words (3000 Bytes). Fuses have to be configured in order to declare a 2Kwords booloader. 
Therefore the user code must not exceed 28672 Bytes.

EEPROM Usage
-------------
Bytes 0 to 20 are reserved for the TFTP Bootloader, please read "Settings stored in flash" below

Flashing bootloader:
--------------------
Connect a TinyUSB programmer and "make install".

Configuring your network:
-------------------------
The bootloader default address is 192.168.1.1.
Configure your computer network card to a static address of 192.168.1.x with a subnet of 255.255.255.0.
By default, the bootloader assumes an internet gateway is at address 192.168.1.254.

Converting firmware to the right format:
----------------------------------------
The bootloader accepts raw binary images, starting at address 0x0000.
These can be generated using avr-objcopy, part of WinAVR / AVR-GCC, using the "-O binary" option.
Example: avr-objcopy -j .text -j .data -O binary [firmware].elf [firmware].bin

Uploading firmware manually:
----------------------------
1) Check the target board is powered, and connected to the computer ethernet.
2) Verify the computer network settings: Static IP of 192.168.1.x, Subnet of 255.255.255.0.
3) Push reset button to start the bootloader. The LED will blink rapidly.
4) In a console window (Windows): tftp -i 192.168.1.1 put [firmware].bin
6) The board will be reprogrammed and reboot.

Flash codes:
------------
Rapid blinking: Ethernet bootloader is wainting for a connection.

Timing
------
-Timeout at startup is set to 5 seconds approximately
-Timeout will never occur if there is no firmware in user flash space
-Timeout (also 5 seconds) will occur if a download has been started and no response is sent by the client, and TFTP server will restart and wait for a new attempt
-Timeout will never occur after an hardware reset if there has been an interrupted transfer before (we suppose the image in flash to be incomplete)


IP Addressing:
--------------

This TFTP bootloader does not implement a DHCP client at this time. As it has been thought to be as tiny, reliable and autonomous as possible
only static addressing is supported. 

DHCP boot with TFTP server address recovery in specific DHCP Option could be an option in order to have a diskless station like behaviour, but
once again this bootloader has been thought to be rock-solid and automatic flashing of firmware at startup is a dangerous option.    

Settings stored in flash:
----------------------------------

In order to provide the possibility to configure the Ethernet settings used by the TFTP Bootloader
and the user application, the following settings can be stored in the MCU flash.

These are intended to be shared between the TFTP Bootloader and the client App. This will avoid any inconsistencies
and ARP resolution problems during the initialisation process that would succeed if different MAC addresses are
used for example.

0:SIG1(0x55), 1:SIG2(0XAA) --> Signature, if present Bootloader will use the following settings for configuring the Ethernet Chip
2:IMAGE STATUS : 0xFF: BAD or never uploaded, 0xBB: IMAGE OK
3:GWIP0, 4:GWIP1, 5:GWIP2, 6:GWIP3 --> Gateway IP address (Default 192.168.1.254)
7:MASK0, 8:MASK1, 9:MASK2, 10:MASK3 --> Mask (Default 255.255.255.0)
11:MAC0, 12:MAC1, 13:MAC2, 14:MAC3, 15:MAC4, 16:MAC5 --> MAC Address (Default 12:34:45:78:9A:BC)
17:IP0, 18:IP1, 19:IP2, 20:IP3 --> IP Address (Default 192.168.1.1)

The consistency of these settings have to be garanteed by the client application.

Errors handled during TFTP transfer:
---------------
The following error codes and error strings can be reported to the TFTP client software:

- "Opcode?", Code 1, TFTP Opcode sent by client is not supported
- "Flash Full", Code 3, Flash size exceeded 
- "Invalid Image", Code 5, Firmware sent by client don't start with 0x0C 0x94 0xWX 0xYZ (JMP 0xWXYZ)
- "Error", Code 0, Unknown error

Debugging
---------
Debugging can be enabled at time of compilation in debug.h, uncomment the following line, save and compile:

//#define DEBUG

should be replaced by :

#define DEBUG
 
Debug will be outputted to the serial port of the Arduino board at 115200 bps speed.
 
Version history
---------------
0.1 (xx/xx/xxxx): First internal release. Supports uploads on tftp. (Arduino unachieved published release)
0.2 (07/12/2011): Reviewed by Denis PEROTTO, comments added, additionnal error codes added and code cleaned

Todo List
---------------
- Return to Ethernet default settings when putting one pin HIGH during startup
- Enable TFTP debugging by setting a value in flash in the client application
- Add syslog functionnality in addition to serial debugging
- Add possibility to configure the timeout of the TFTP bootloader by setting a value in flash in the client application
