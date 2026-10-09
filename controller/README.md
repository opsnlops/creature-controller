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

## Audio Levels

Dialog and BGM gain are independently configurable in decibels. RTP playback
uses native ALSA output on Linux and CoreAudio on macOS. Linux builds can also
set an ALSA playback element directly at startup:

```json
{
  "audioDeviceName": "plughw:CARD=S3,DEV=0",
  "dialogGainDb": 0.0,
  "bgmGainDb": -6.0,
  "limiterCeilingDb": -1.0,
  "outputVolumePercent": 75,
  "alsaMixerCard": "default",
  "alsaMixerElement": "PCM"
}
```

These fields are optional. The stream gains default to 0 dB, and hardware
volume is left unchanged unless `outputVolumePercent` is present. See
`docs/audio-volume.md` for device and control-selection details.
  

## Multichannel audio router

Follow the [audio router setup manual](docs/audio-router-setup.md) when installing
and commissioning a Pi 5 and AVR. The package also installs `man creature-audio-router`.

`creature-audio-router` receives the same mono Opus/RTP streams as the controller
and mixes them into one HDMI PCM output. Both programs use the `creature::audio`
C++ library for reception, decoding, RTCP scheduling, gain, mixing, and native
output. The controller configures that engine with two inputs and one mono
output; the router supplies a routing matrix and optional bass crossover.

The router is a separate Debian package, versioned independently of the
controller. `./make_packages.sh` builds both packages using the same Docker target
and package validation as GitHub Actions, and exports them to `package/`. To build just the router:

```sh
cmake -S . -B build -DCREATURE_BUILD_CONTROLLER=OFF -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
# On Debian:
cd build
cpack --config CPackAudioRouterConfig.cmake -G DEB
```

### Configuration and services

Install `creature-audio-router`, edit `/etc/creature/audio-router.json`, and run:

```sh
creature-audio-router --list-sound-devices
creature-audio-router --check-config --config /etc/creature/audio-router.json
sudo systemctl enable --now creature-audio-router.service
journalctl -u creature-audio-router.service -f
```

The service is disabled on first installation because HDMI devices and speaker
routes are site-specific. The default configuration is a Debian conffile, so
package upgrades preserve local edits. Active services restart on upgrade.
`--check-config` validates the configuration without opening sockets or audio
hardware; actual device capabilities and interface addresses are checked at
startup. Unknown fields and invalid routes fail validation.

For the Pi 5's two HDMI outputs, use two template instances:

```sh
sudo install -m 644 /etc/creature/audio-router.json /etc/creature/audio-router/hdmi0.json
sudo install -m 644 /etc/creature/audio-router.json /etc/creature/audio-router/hdmi1.json
# Edit each file, selecting its HDMI device and speaker routes.
sudo systemctl enable --now creature-audio-router@hdmi0.service creature-audio-router@hdmi1.service
```

Use one process per physical device; enable either the default service or its
replacement template instance for that device. The services run as dynamic
users with membership in `audio`. Configuration files must be readable by them.
`networkInterface` accepts an interface name with one IPv4 address, or an
explicit local IPv4 address. Both instances can receive the same multicast
streams and RTCP packets on ports 5004 and 5005.

The example [configuration](config/audio-router.json) lists the eight channels
in ALSA's 7.1 order: front left, front right, rear left, rear right, center,
LFE, side left, side right. The backend sets or verifies the device's speaker
map and refuses an ambiguous map. Use the HDMI device reported by ALSA on your
Pi; card names in the example are illustrative. Eight-channel PCM must be
advertised by the AVR's EDID. Test physical speaker assignments at low volume
before connecting the workshop speakers.

`inputs` subscribes to bird channels 1–16 and BGM channel 17. Each stream is
decoded once. `routes` selects any number of output destinations, with an
optional `gainDb` on each route. Input gain applies before fanout. The default
routes bird 1 to front left, bird 2 to front right, and BGM to both. Unrouted
outputs remain silent. Add routes to distribute BGM to other speakers.

`output` also accepts `commonPlayoutDelayMs`, `audioDeviceCompensationMs`,
`limiterCeilingDb`, `outputVolumePercent`, `alsaMixerCard`, and
`alsaMixerElement`. Gains range from −90 to +12 dB. The ceiling clamps the final
PCM mix; provide mixing headroom with input/route gains to avoid clipping.
HDMI volume normally belongs on the AVR; leave optional ALSA mixer settings
unset if the HDMI device has no hardware volume control.

### Synchronization

The router defaults to `requireRtcp: true`: it stays silent until BGM has a
fresh, plausible RTCP mapping. BGM establishes the shared timeline; each bird
joins only after its own report matches that timeline and CNAME. A missing or
incompatible bird stays silent without interrupting BGM. The controller keeps
its existing arrival-time fallback when RTCP is unavailable.

Keep all Pis and the server synchronized through PTP, including their system
clocks. Use the same common playout delay on every synchronized instance.
The output device remains running through idle periods. RTCP aligns startup;
independent DAC clocks can still drift during long continuous playback. Actual
HDMI/AVR latency requires loopback or acoustic measurement, followed by
per-device compensation. Startup rejects a measured native pipeline that exceeds
the configured delay headroom; increase the common delay on all nodes in that
case. See [audio synchronization](docs/audio-synchronization.md).

### Optional software bass management

The installed example in
`/usr/share/doc/creature-audio-router/examples/audio-router-bass-example.json`
adds an 80 Hz Linkwitz–Riley fourth-order crossover. `bass.inputs` selects source
streams and send gains; each source contributes once even when BGM is routed
to several speakers. `highPassOutputs` selects the complementary high-pass
filters for the full-range mix. Omit those outputs to keep their full range
and use the LFE feed as an additional low-pass mix. `gainDb` and
`invertPolarity` let you calibrate the subwoofer feed. Filter state resets at
a new playout session.

The Denon AVR-3312CI provides seven full-range speaker destinations and one
HDMI LFE destination; its two subwoofer outputs do not create a second
independent PCM channel. In Direct mode, verify the receiver's speaker and
bass settings so they agree with the software crossover, and disable latency
features during calibration. Measure subwoofer level, phase, and HDMI channel
mapping on the actual receiver before using the bass example in production.
