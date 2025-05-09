# April's Creature Workshop Creature Controller

This is the code I use to control creatures! It receives DMX frames from the 
Creature Server (either via RS-485, or over UDP with e1.31), calculates 
moments, and adjusts the position of the motors as requested.

There's two parts to this code.

The first part runs on a generic Linux host with USB ports. (I mostly target 
Raspberry Pis running Debian, but any Linux host will work.) The second part 
is the firmware that runs on a Pi Pico 2040 that's used as a motor controller.

The Linux host and the Pi Pico 2040 speak a protocol similar to the G-Code 
that 3D printers use. (But is not G-Code.) The protocol is documented below.

## History
This is a re-implementation of my Creature Controller code. The first 
version was designed for the Raspberry Pi Pico 2040. While this worked very 
well, having to do the build -> flash dance got old really fast. I wanted to 
be able to use a normal file system for the creature's config so I could 
move quicker.

Using a USB port on the creature is also hardier. The first few times I 
tried to use DMX on the creature directly I had to deal with a DMX 
terminator hanging off the back, and it was easy to damage things. With this 
version the DMX connection is terminated at the Linux host, and doesn't even 
have to be RS-485. e1.31 is also supported over UDP.


# Theory of Operation

The RP 2040 connects to the Linux host as a CDC device. (Serial port 
emulation.) This allows the Creature Controller app to be run as non-root and 
makes the protocol easier to debug.

Creature Server -> DMX -> Creature Controller -> Creature

# Control Protocol

## Controller ➡️ Pi Pico

### Position 

    POS xxx yyy zzz ...

Sets the position of the motors, where `xxx` the motor at slot 0, etc.

### Limit 

    LIMIT # <lower> <upper>

Tells the controller the expected upper and lower bound of the motors. This 
is for safety. Anything beyond this at the creature will halt, assuming a 
coding error. (Servos are strong and snapping PLA is easy!)


# Uploading a Creature's Config

   curl -v -d @kenny.json https://server.prod.chirpchirp.dev/api/v1/creature
  