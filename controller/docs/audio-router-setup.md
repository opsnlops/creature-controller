# Creature audio router setup manual

Use this procedure to set up a Raspberry Pi 5, a Denon AVR-3312CI, and the
workshop's bird speakers. Start with one HDMI output and two birds. Verify
speaker assignments and timing before adding more birds, a second AVR, or
software bass management.

Each router process owns one HDMI audio device. It receives bird voices and
BGM from the Creature Server, then mixes them into the configured speakers.
BGM is the timing master, even when you do not route it to a speaker. Keep
`requireRtcp` enabled for normal workshop use.

## Setup order

1. [Connect and configure the AVR](#connect-and-configure-the-denon).
2. [Install the router](#install-the-software) and [identify the HDMI device](#identify-the-network-and-hdmi-devices).
3. [Check the physical speaker assignments](#check-the-speaker-assignments).
4. [Configure two birds and BGM](#configure-two-birds-and-bgm), then [start the service](#start-and-check-playback).
5. [Verify RTCP timing and measure alignment](#verify-synchronization-and-calibrate-latency).
6. [Add the remaining birds](#add-more-birds) and [optional bass management](#enable-optional-software-bass-management).
7. [Configure the second HDMI output](#use-both-pi-hdmi-outputs) if needed.

## Connect and configure the Denon

Connect the Pi's micro-HDMI output to an HDMI input on the Denon. Use that
input for all calibration and playback. Turn the AVR on before checking the
Pi's audio devices, and begin with its master volume low.

The AVR provides seven full-range destinations and one HDMI LFE destination.
Its two subwoofer terminals carry the same signal. BGM shares your chosen
speaker destinations with the birds.

Use these starting settings:

| Denon setting | Setting for workshop setup |
| --- | --- |
| Listening mode | Direct |
| Amp Assign | NORMAL, preserving the surround-back outputs |
| HDMI Audio Out | Amp |
| Auto Lip Sync | OFF |
| Audio Delay | 0 ms on the selected input |
| Speaker Config | Enable the connected speakers; use two surround-back speakers when both are connected |
| Front Height and Front Wide | None for the conventional seven-speaker arrangement |
| Distance and channel levels | Begin with equal distances and neutral trims for channel identification; record subsequent calibration changes |

Direct mode still requires correct speaker and bass configuration. Check
these settings in the [Denon owner's manual](https://assets.denon.com/DocumentMaster/us/AVR-3312CIE3_ENG_CD-ROM_v00.pdf),
particularly printed pages 103 and 105–110. Bass settings are covered below.

## Install the software

Use the ARM64 package on a 64-bit Debian Pi installation. Check the architecture:

```sh
dpkg --print-architecture
```

It should report `arm64` for the supplied ARM64 build. Copy the `.deb` to the
Pi, change into that directory, and install it:

```sh
sudo apt install ./creature-audio-router_0.1.2_arm64.deb
sudo apt install alsa-utils tcpdump
creature-audio-router --version
```

Use the actual filename for later releases. Installing the router does not
require installing `creature-controller`.

The package provides:

| Path | Purpose |
| --- | --- |
| `/usr/bin/creature-audio-router` | Application |
| `/etc/creature/audio-router.json` | Default service configuration |
| `/etc/creature/audio-router/` | Configuration directory for named instances |
| `/usr/lib/systemd/system/creature-audio-router.service` | Default service |
| `/usr/lib/systemd/system/creature-audio-router@.service` | Named instance template |
| `/usr/share/doc/creature-audio-router/examples/audio-router-bass-example.json` | Optional bass example |

The service stays disabled on first installation. Configure the device and
routes before enabling it.

## Identify the network and HDMI devices

List the Pi's IPv4 interfaces and audio devices:

```sh
ip -4 address show
creature-audio-router --list-sound-devices
aplay -l
aplay -L
```

Choose the network interface carrying the workshop multicast traffic. The
examples use `eth0`. An interface name must resolve to one IPv4 address; use
an explicit local IPv4 address if the interface has several.

Choose the HDMI device connected to the AVR. For example:

```text
hdmi:CARD=vc4hdmi0,DEV=0
```

This name is an example, not a guarantee of the Pi's port assignment. Check
both HDMI outputs and record which device name reaches each AVR. Prefer
named cards over numeric card indices.

The router requires 48 kHz signed 16-bit PCM. The eight-channel configuration
also requires an HDMI connection advertising eight-channel PCM. If eight
channels are unavailable, confirm that the AVR is powered on, the correct
input is selected, and the Pi is connected directly to the AVR. A TV or
adapter in the audio path can change the capabilities reported to the Pi.

## Check the speaker assignments

Stop the router service before using another program on its HDMI device:

```sh
sudo systemctl stop creature-audio-router.service
```

For template instances, stop the instance that owns the device instead.
Set a shell variable to the exact device name you found:

```sh
HDMI_DEVICE='hdmi:CARD=vc4hdmi0,DEV=0'
speaker-test -D "$HDMI_DEVICE" -c 8 -r 48000 -t sine -f 440 -l 1
```

Record which physical speaker sounds for each channel. The 440 Hz test is
for the full-range outputs; the LFE output may be inaudible at this frequency.
With a connected subwoofer and the AVR's Subwoofer setting at Yes, test that
destination separately at low volume:

```sh
speaker-test -D "$HDMI_DEVICE" -c 8 -r 48000 -t sine -f 60 -s 6
```

The [Debian speaker-test manual](https://manpages.debian.org/trixie/alsa-utils/speaker-test.1.en.html)
describes the channel numbering and test options. The Pi's HDMI driver uses
a fixed CEA-861 channel order, which the example configuration matches:

| PCM channel | Router name | Expected Denon connection | Your bird or speaker |
| --- | --- | --- | --- |
| 1 | `front_left` | Front L | |
| 2 | `front_right` | Front R | |
| 3 | `lfe` | Subwoofer | Shared bass |
| 4 | `center` | Center | |
| 5 | `rear_left` | Surround L | |
| 6 | `rear_right` | Surround R | |
| 7 | `rear_left_center` | Surround Back L | |
| 8 | `rear_right_center` | Surround Back R | |

Keep the router's `output.channels` array in exactly this order. The Pi's
speaker map is read-only, so the router cannot request a different order; it
verifies that the configured order matches the driver's map and refuses to
start otherwise. Changing route destinations changes where a bird plays.

On a mismatch, the startup log lists the requested map, the map the driver
reported, and the maps the device offers. After the tone tests, repeat the
assignment check using actual bird streams through the router; that verifies
the complete path.

## Configure two birds and BGM

Edit the default configuration:

```sh
sudoedit /etc/creature/audio-router.json
```

Use this complete starting configuration, replacing the network interface,
HDMI device, and bird channel numbers with your assignments:

```json
{
  "networkInterface": "eth0",
  "logLevel": "info",
  "rtpPort": 5004,
  "requireRtcp": true,
  "output": {
    "deviceName": "hdmi:CARD=vc4hdmi0,DEV=0",
    "channels": [
      "front_left", "front_right", "lfe", "center",
      "rear_left", "rear_right", "rear_left_center", "rear_right_center"
    ],
    "commonPlayoutDelayMs": 20,
    "audioDeviceCompensationMs": 0,
    "limiterCeilingDb": -1.0
  },
  "inputs": [
    {"channel": 1, "gainDb": 0.0},
    {"channel": 2, "gainDb": 0.0},
    {"channel": 17, "gainDb": -6.0}
  ],
  "routes": [
    {"input": 1, "output": "front_left"},
    {"input": 2, "output": "front_right"},
    {"input": 17, "output": "front_left"},
    {"input": 17, "output": "front_right"}
  ]
}
```

Bird channel numbers are the server's audio assignments, from 1 through 16.
They are independent of the HDMI speaker numbers. Channel 17 is always BGM.
The router subscribes to multicast group `239.19.63.<channel>` for each input.
RTP uses `rtpPort`, and RTCP uses the next UDP port, normally 5005.
Allow these multicast streams through the workshop network and any host
firewall. Routing between subnets requires multicast forwarding.

`inputs` determines which streams the router receives and their initial
software gain. `routes` determines where those streams play. A stream can
have several routes and is decoded once before fanout. You can add `gainDb`
to an individual route to trim that destination. Unrouted outputs remain
silent.

Validate before starting:

```sh
creature-audio-router --check-config --config /etc/creature/audio-router.json
```

This checks JSON, field names, ranges, and route references. It does not open
HDMI hardware or sockets, or prove that the configured interface exists.
The startup check covers those resources.

## Start and check playback

```sh
sudo systemctl enable --now creature-audio-router.service
systemctl status creature-audio-router.service --no-pager
journalctl -u creature-audio-router.service -f
```

Play BGM and each bird individually from the server. Confirm that:

- BGM reaches both configured speakers.
- Bird 1 reaches only front left and bird 2 only front right.
- The unused outputs stay silent.
- The log reports `RTCP playout` and `timing=rtcp` while playback is active.

BGM packets and usable BGM RTCP must accompany the shared timeline. Omitting
BGM from the audible routes does not remove this requirement. With strict
RTCP enabled, missing BGM timing leaves the output silent. An active bird
with missing or incompatible RTCP stays silent independently while BGM
continues.

`timing=waiting` is normal between sessions. During expected playback, a
warning that BGM is waiting for usable RTCP calls for checking the network,
server reports, and clock synchronization.

The output device stays running through idle periods. The service restarts
on application failure; it does not restart the device for every sound.

## Add more birds

Add one `inputs` entry and one voice route per bird. For example, adding a
third bird on server channel 5 requires these entries in the existing arrays:

```json
{"channel": 5, "gainDb": 0.0}
```

```json
{"input": 5, "output": "center"}
```

To put BGM on that speaker too, add another route:

```json
{"input": 17, "output": "center", "gainDb": 0.0}
```

Use the seven full-range destinations in the assignment table. Keep `lfe`
for bass. A second AVR provides another set of speaker destinations when
needed.

After each change, validate the complete file and restart the owning service:

```sh
creature-audio-router --check-config --config /etc/creature/audio-router.json
sudo systemctl restart creature-audio-router.service
```

Changes take effect on restart. Leave enough headroom for a voice and BGM to
play together. `limiterCeilingDb` clamps the final PCM mix; repeated clipping
still sounds distorted. Reduce input or route gains if simultaneous playback
sounds rough. Software gains accept −90 through +12 dB. Leave optional ALSA
hardware-volume settings unset initially and use the AVR's master volume.

## Verify synchronization and calibrate latency

Keep the server and every Pi synchronized through PTP. Synchronization must
reach the Pi's system clock as well as any PTP hardware clock, because RTCP
scheduling uses system wall time. Use the workshop's existing PTP services;
check their lock state and offsets before changing audio compensation.

All synchronized router instances and controllers should use the same
`commonPlayoutDelayMs`. Start at 20 ms. If device latency or scheduling
headroom requires a larger delay, raise the common value across the group.
The accepted range is 20–500 ms. For example, a +8 ms compensation does not
fit the 20 ms common delay; raise the common value to 40 ms on all nodes before
applying that compensation.

Calibrate with the same BGM transient or click sequence on both systems:

1. Begin with `audioDeviceCompensationMs` at zero and stable AVR settings.
2. Capture both outputs simultaneously using a shared recording clock.
3. Compare corresponding transients over several fresh playback sessions.
4. If one path is consistently 8 ms later, add 8 ms to that path's compensation
   relative to the reference. Positive compensation enqueues earlier.
5. Validate, restart, and repeat the capture.

Compensation accepts −100 through +100 ms. A large positive value reduces
available enqueue headroom; increase the common delay on all participating
systems if necessary. Do not use compensation to hide changing clock offsets
or intermittent packet loss.

A line-level capture measures the output electronics. A microphone capture
also includes speaker distance and room reflections; compare equivalent
positions when judging device latency. Record the reference path, AVR
settings, common delay, and each compensation value.

Logs include the signed `mapping/arrival delta` and predicted `start lateness`
in microseconds. They help diagnose timing but do not include every delay
inside the AVR and speakers. Startup rejects implausible mappings more than
10 ms from the arrival-based estimate; that threshold is not an alignment
accuracy promise.

Run a long BGM session as well as short start tests. RTCP aligns the session
start. Continuous correction of independent DAC clock drift is not currently
implemented, so measure long-run alignment before relying on hours of
uninterrupted BGM. See [audio synchronization](audio-synchronization.md).

## Enable optional software bass management

Finish speaker assignment and timing checks before adding bass. Save the
working configuration, then add this top-level `bass` object, alongside
`output`, `inputs`, and `routes`:

```json
{
  "output": "lfe",
  "crossoverHz": 80,
  "gainDb": -6.0,
  "invertPolarity": false,
  "inputs": [
    {"channel": 1, "gainDb": 0.0},
    {"channel": 2, "gainDb": 0.0},
    {"channel": 17, "gainDb": 0.0}
  ],
  "highPassOutputs": ["front_left", "front_right"]
}
```

The fragment is the value of `"bass"`; it is not a complete router config.
Change its input channels to match your subscribed streams. The complete example in `config/audio-router-bass-example.json` in the repository
(or `/usr/share/doc/creature-audio-router/examples/audio-router-bass-example.json`
in the installed package) shows its placement.

This creates an 80 Hz Linkwitz–Riley fourth-order low-pass mix on LFE and
complementary high-pass filters on the two listed full-range outputs. Each
bass source contributes once, even if BGM has routes to every bird speaker.
The input gain applies to both its ordinary routes and its bass sends.

For software-managed bass, start with the Denon's connected main speakers
set to Large and Subwoofer set to Yes. Select Subwoofer Mode LFE to avoid
adding another full-range bass mix. Set LPF for LFE above the software
crossover, such as 250 Hz, then measure the combined response. These controls
are described on printed pages 105–106 of the
[Denon manual](https://assets.denon.com/DocumentMaster/us/AVR-3312CIE3_ENG_CD-ROM_v00.pdf).

Here, Large selects the AVR's full-band path; the software high-pass provides
bass removal for the listed outputs. For speakers that need bass removal,
include every such routed output in `highPassOutputs` and choose an appropriate
crossover. The accepted software crossover range is 20–200 Hz.

Start with modest subwoofer level. Measure around the crossover, adjust
`bass.gainDb`, and compare both values of `invertPolarity`. Receiver and
subwoofer filtering still affect phase and level in Direct mode. Recheck
synchronization after changing AVR settings.

Omitting `highPassOutputs` keeps the ordinary routes full range and adds only
the low-pass LFE mix. Omitting the entire `bass` object disables software bass
management. Do not add direct ordinary routes to the same output used by
`bass.output`; validation rejects that combination.

## Use both Pi HDMI outputs

Run one named instance per HDMI device. Once the default configuration works,
copy it to the two instance files:

```sh
sudo install -d -m 755 /etc/creature/audio-router
sudo install -m 644 /etc/creature/audio-router.json /etc/creature/audio-router/hdmi0.json
sudo install -m 644 /etc/creature/audio-router.json /etc/creature/audio-router/hdmi1.json
sudoedit /etc/creature/audio-router/hdmi0.json
sudoedit /etc/creature/audio-router/hdmi1.json
```

Set each file's `output.deviceName` to its own verified HDMI device. Set its
bird inputs and routes for that AVR. Keep channel 17 and the same common
playout delay in both files. Compensation may differ after measurement.

```sh
creature-audio-router --check-config --config /etc/creature/audio-router/hdmi0.json
creature-audio-router --check-config --config /etc/creature/audio-router/hdmi1.json
sudo systemctl disable --now creature-audio-router.service
sudo systemctl enable --now creature-audio-router@hdmi0.service creature-audio-router@hdmi1.service
```

Each physical device should have one owner. Disable the default service when
replacing it with named instances. Both instances can receive the same BGM
multicast stream on the same UDP ports.

```sh
systemctl status creature-audio-router@hdmi0.service --no-pager
systemctl status creature-audio-router@hdmi1.service --no-pager
journalctl -u creature-audio-router@hdmi0.service -u creature-audio-router@hdmi1.service -f
```

The services use dynamic users with membership in `audio`. Keep configuration
files readable by those users; the `644` modes above do that.

## Troubleshoot startup and playback

Use the unit name for the service you are diagnosing. Capture recent logs:

```sh
journalctl -u creature-audio-router.service -b -n 100 --no-pager
```

To check incoming workshop audio packets, substitute the configured interface
and ports if you changed them:

```sh
ip maddr show dev eth0
sudo tcpdump -ni eth0 'udp and (dst port 5004 or dst port 5005)'
```

Receiving packets alone does not establish valid RTCP timing or correct
speaker routing.

| Symptom | Check or action |
| --- | --- |
| Config check fails | Fix the reported JSON field, channel, or route. Comments and trailing commas are not valid JSON. Every route and bass send must refer to a subscribed input. |
| Config check passes but startup fails | Check the exact device name, local interface address, permissions, and device capabilities in the startup log. Offline validation does not test these. |
| HDMI device missing or only stereo available | Check power, input selection, cable, and direct Pi-to-AVR connection. Repeat device enumeration after the AVR is ready. |
| Device busy | Stop speaker-test, other sound applications, and any other router instance using that same HDMI device. |
| Ambiguous speaker map | Compare the requested and reported maps in the log, and set `output.channels` to the reported order. An empty or unknown reported map means the device cannot verify positions; check the HDMI connection and AVR power. |
| Output pipeline exceeds headroom | Increase the common delay on all synchronized nodes; check positive compensation values. |
| Silent output with BGM waiting for RTCP | Check BGM RTP and RTCP, the server timeline, matching reports, and PTP system-clock synchronization. |
| BGM plays but one bird is silent | Check its server channel, subscription, route, matching RTCP SSRC/CNAME, and clock mapping. |
| Bird sounds from the wrong speaker | Repeat physical assignment tests and check the route destination, wiring, and AVR amplifier assignment. |
| Subwoofer silent | Check the bass sends, LFE route ownership, subwoofer connection and level, and Denon Subwoofer Yes/LFE settings. |
| Distortion with voice and BGM together | Lower input or route gains and check AVR/subwoofer levels. The PCM ceiling does not restore a clipped mix. |
| Instances start together but separate during long playback | Measure DAC clock drift. Startup compensation fixes a constant offset, not accumulating drift. |
| Service repeatedly restarts | Inspect the first startup or receiver/output error in the journal, rather than only the restart messages. |

For a foreground diagnostic run, stop the owning service first, then run its
exact configuration:

```sh
sudo systemctl stop creature-audio-router.service
creature-audio-router --config /etc/creature/audio-router.json
```

The interactive account needs access to the ALSA device. A permissions failure
here can differ from the service, which explicitly joins `audio`. Press
Ctrl-C to stop the foreground process before restarting the service.

## Maintain the installation

Validate the edited file and restart its service after configuration changes.
For a named instance:

```sh
creature-audio-router --check-config --config /etc/creature/audio-router/hdmi0.json
sudo systemctl restart creature-audio-router@hdmi0.service
```

Install future `.deb` releases with `sudo apt install ./<package-file>.deb`.
The default configuration is a Debian conffile, so upgrades preserve local
edits and may ask how to handle an upstream change. Keep the working local
configuration and compare any new defaults before adopting them. Active
router services restart on upgrade.

Keep a copy of the configuration files and your calibration record before
upgrading. Removing the package retains the default conffile; purging removes
it. Treat your named instance files as site configuration and back them up
separately.

## Commissioning checklist

- [ ] Each HDMI device name is matched to the correct physical port and AVR.
- [ ] Every used full-range speaker passes a physical assignment test.
- [ ] Actual bird playback confirms the configured route destinations.
- [ ] BGM reaches exactly the intended speakers.
- [ ] PTP is locked and the participating system clocks are synchronized.
- [ ] Active playback reports RTCP timing.
- [ ] All participating controllers and routers use the same common delay.
- [ ] Measured compensation and AVR settings are recorded.
- [ ] Simultaneous birds and BGM sound clean at the intended volume.
- [ ] Optional LFE level and crossover behavior have been measured.
- [ ] Long BGM playback meets the workshop's alignment requirements.
- [ ] Each HDMI device has one enabled owner and survives a Pi reboot.
