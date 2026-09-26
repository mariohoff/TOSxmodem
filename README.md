# XModem sender in for TempleOS in holyC

- `Serial.HC` - RS232 driver
- `XmodemSend.HC` - Simple entity to send binary files via xmodem
- `Xmodem.HC` - Simple include

Just do:
```
#include "Xmodem"
XmodemSendFile("mybinary.BIN");
```

To attach serial port on QEMU you can either use a serial port or even just a
FIFO with these flags:
```
SERIAL_PORT=/dev/ttyACM0
[...] -serial ${SERIAL_PORT}

# OR

mkfifo /tmp/guest.in /tmp/guest.out
[...] -serial pipe:/tmp/guest
# then just read and write on guest.in and guest.out
```

neorv32 dir contains a very hacky receiver side for my neorv32 CPU on an FPGA.
Just as example.

`neorv32/upload.sh` is my simple bash script that I use to use xmodem on Linux (needs picocom)

## Links
 - Simple serial communication in x86: <https://wiki.osdev.org/Serial_Ports#Example_Code>
 - Terry's Comm.HC: <https://github.com/cia-foundation/TempleOS/blob/c26482bb6ad3f80106d28504ec5db3c6a360732c/Doc/Comm.HC>
 -  XMODEM Protocol Explained: <https://www.youtube.com/watch?v=0HZqiFqp77>
