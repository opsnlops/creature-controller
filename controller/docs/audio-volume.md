# Audio level configuration

The controller manages two different kinds of audio level:

- dialog and BGM gain are applied independently before the streams are mixed;
- output volume changes the Linux audio device's ALSA playback control.

RTP playback uses ALSA directly on Linux and CoreAudio directly on macOS. The
native backends expose the device clock and hardware delay needed for precise
playout monitoring.

The output stream remains active between RTP sessions with a bounded queue of
silence. This prevents USB audio devices from dropping the beginning of a
sound while their playback path wakes up. The controller stops the native
output only during shutdown or device-error recovery.

`audioDeviceName` selects an output using the exact name printed by
`creature-controller --list-sound-devices`. It is preferred over
`audioDevice`, whose numeric index remains available for backward
compatibility. Names such as `plughw:CARD=S3,DEV=0` remain stable when ALSA
enumeration order changes. If a configured name is unavailable, audio
initialization fails instead of silently selecting another output.

Device discovery does not require configuration files:

```sh
creature-controller --list-sound-devices
creature-controller --list-network-devices
```

Controller device 0 is ALSA's `default` entry, not hardware card 0. On a Pi,
`default` may resolve to HDMI even when playback should use an I²S or USB DAC.
For example, Kenny's BossDAC uses `"audioDeviceName": "plughw:CARD=BossDAC,DEV=0"`.
An ALSA open error of -524 means playback is unsupported by the selected device;
an HDMI output without a connected audio-capable sink can produce this error.
Use the discovery output to select the intended DAC explicitly.

On CoreAudio, `audioDeviceName` matches the displayed device name and must
identify exactly one device. When no name is configured, device 0 remains the
platform's default output.

All audio fields are optional:

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

`dialogGainDb` and `bgmGainDb` accept values from -90 dB through +12 dB. A
runtime gain change uses a 2 ms de-click transition; incoming audio does not
fade in.

`limiterCeilingDb` accepts values from -90 dB through 0 dB. It bounds the
mixed signal before conversion to 16-bit PCM.

When `outputVolumePercent` is omitted, the controller does not change the
hardware mixer. When it is present, Linux builds use the ALSA mixer API to set
the selected playback element to a value from 0 through 100 percent.

`alsaMixerCard` defaults to `default`. `alsaMixerElement` may identify a
specific playback element such as `Master`, `PCM`, or `Speaker`. If the
element is omitted, the controller prefers those names in that order and then
uses the first active playback-volume element.

Some USB audio devices expose no hardware playback volume. The controller
logs a warning and continues playing audio when no suitable element exists.
Dialog and BGM software gain remain available in that case.

`outputVolumePercent` maps linearly onto the element's raw steps, not onto
decibels: the controller sets `round(max × percent / 100)`. On a control
measured in dB, such as the PCM5122's, the percentage therefore says little
about the actual level, so check the table for your device.

## PCM5122 (Creature CM5 carrier)

The Creature CM5 carrier has a TI PCM5122 DAC on the board itself, driving the
red (right) and white (left) RCA jacks at line level. The `creature-cm5-audio`
overlay names the ALSA card `CreatureAnalog`. The controller's mono output is
copied to both channels, so either jack carries the full signal.

```json
{
  "audioDeviceName": "plughw:CreatureAnalog",
  "outputVolumePercent": 81,
  "alsaMixerCard": "hw:CreatureAnalog",
  "alsaMixerElement": "Digital"
}
```

The PCM5122 has two playback-volume elements:

- `Digital` is the main volume. It runs from raw 0 to 255 in 0.5 dB steps,
  with raw 207 at 0 dB and raw 255 at +24 dB.
- `Analogue` has only two settings: raw 0 is −6 dB and raw 1 is 0 dB.
  `Analogue Playback Boost` adds a small extra gain and should stay off.

Always set `alsaMixerElement` to `Digital`. With `Analogue`, every percentage
from 50 to 100 gives the same 0 dB, and `Digital` stays wherever it was last
set. ALSA restores the mixer state at boot, so that can be a large gain left
over from an earlier configuration. Omitting the element doesn't work either:
the PCM5122 has no `Master`, `PCM`, or `Speaker` element, so the controller
falls back to the first one it finds, which is `Analogue`.

`outputVolumePercent` values for `Digital`, where each percent is about 1.3 dB:

| `outputVolumePercent` | raw | level |
|---:|---:|---:|
| 100 | 255 | +24.0 dB |
| 95 | 242 | +17.5 dB |
| 90 | 230 | +11.5 dB |
| 85 | 217 | +5.0 dB |
| **81** | **207** | **0.0 dB** |
| 80 | 204 | −1.5 dB |
| 78 | 199 | −4.0 dB |
| 75 | 191 | −8.0 dB |
| 70 | 179 | −14.0 dB |
| 60 | 153 | −27.0 dB |
| 50 | 128 | −39.5 dB |

Use **81** (0 dB) with powered speakers and set the listening level on the
speaker. At 0 dB the DAC passes the controller's output unchanged, so
`limiterCeilingDb` still protects against clipping and the RCA jacks deliver
full line level. Anything above 81 adds gain after the limiter, so loud
passages clip in the DAC. Values below 81 work, but lowering the level in the
DAC only reduces the signal-to-noise ratio.

To check the current level on a running system:

```sh
amixer -c CreatureAnalog sget Digital
```
