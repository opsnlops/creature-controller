#!/usr/bin/env python3
"""
Byte-faithful USB-CDC capture for diagnosing the BSENSE/LOG splice.

Reads the raw serial stream with no line discipline / no display rendering,
mirrors it to stdout, and tees it to a file. Use this INSTEAD of Serial.app
to determine whether the splice is introduced by the firmware/USB or by the
terminal app. The firmware stamps its own timestamps, so this capture is
directly comparable to a Serial.app capture.

Usage:
    python3 tools/raw_cdc_capture.py /dev/cu.usbmodem14202 [outfile]

Quit Serial.app first - only one process can hold the port.
"""
import sys

try:
    import serial  # pyserial
except ImportError:
    sys.exit("pyserial not installed: pip3 install pyserial")

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbmodem14202"
outfile = sys.argv[2] if len(sys.argv) > 2 else "/tmp/usb-raw.log"

# 115200,N,8,1 per the firmware's binary-info. For true USB-CDC the baud is
# nominally a no-op, but this host/driver clearly honors line coding, so set
# it explicitly to match.
ser = serial.Serial(port, baudrate=115200, bytesize=8,
                     parity="N", stopbits=1, timeout=1)

print(f"Capturing {port} @115200 raw -> {outfile} (Ctrl-C to stop)\n",
      file=sys.stderr)

with open(outfile, "wb") as f:
    try:
        while True:
            data = ser.read(256)
            if data:
                f.write(data)
                f.flush()
                sys.stdout.buffer.write(data)
                sys.stdout.buffer.flush()
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
