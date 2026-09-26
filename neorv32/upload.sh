#!/bin/bash
set -e

TTY="${1:-/dev/ttyACM0}"
BIN="kernel.bin"
BAUD1="115200"
BAUD="115200"

# make -C ../kernel clean && make -C ../kernel
# make clean
# make

stty -F "$TTY" "$BAUD1" raw -echo -echoe -echok

# 1. Send 'u' to trigger XMODEM receiver on the FPGA
printf "u" > "$TTY"

# 2. sx will automatically wait for the bootloader's initial NAK byte
sx -vv "$BIN" < "$TTY" > "$TTY"

# 3. Immediately drop into terminal to see kernel output
exec picocom -b "$BAUD" "$TTY" --imap lfcrlf
