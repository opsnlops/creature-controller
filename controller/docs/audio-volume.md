# Audio level configuration

The controller manages two different kinds of audio level:

- dialog and BGM gain are applied independently before the streams are mixed;
- output volume changes the Linux audio device's ALSA playback control.

RTP playback uses ALSA directly on Linux and CoreAudio directly on macOS. The
native backends expose the device clock and hardware delay needed for precise
playout monitoring.

`audioDevice` selects the index printed by `creature-controller --list-sound-devices`.
Device 0 is the platform's default output. Native device ordering can differ
from older SDL-based builds, so run the listing once when upgrading a
controller that selected a non-default device.

All audio fields are optional:

```json
{
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
