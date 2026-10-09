# RTCP audio synchronization

The controller receives Opus/RTP audio on UDP port 5004 and RTCP Sender
Reports on UDP port 5005. Dialog uses the creature's multicast group and BGM
uses `239.19.63.17`.

The server sends 10 ms / 480-sample RTP packets. Motion is updated every
20 ms, so the default common audio playout delay is 20 ms: two audio packets
and one motion frame.

## Clock requirement

Every controller and router host must synchronize its system clock to the Creature Server
through PTP (including synchronization of the system clock to the PTP clock). RTCP maps the server's 48 kHz RTP timeline to NTP
wall time; it does not synchronize the host clock itself.

At the beginning of a BGM RTP SSRC generation, the shared audio engine waits
briefly for a fresh BGM Sender Report. It uses RTCP only when the report matches
the current RTP SSRC, contains an SDES CNAME, and its mapped deadline is
consistent with the packet's LAN arrival time. Each bird is independently
qualified against BGM: its matching-SSRC report must have the same CNAME and
agree with BGM's NTP/RTP relationship within 1 ms. Missing or incompatible
birds stay silent; BGM keeps its selected timing.

The last check rejects mappings more than 10 ms from the arrival-based
estimate. The fallback warning includes a signed `mapping/arrival delta`; a
large value usually means the controller and server clocks need attention.
This ±10 ms guard qualifies whether RTCP timing is safe to use; it is not a
promise of speaker-to-speaker alignment. Two controllers near opposite limits
could still differ by almost 20 ms, so all controllers and the server must use
the same LAN time source and keep their clocks substantially closer than the
guard.

BGM is the master timeline. The dialog packet with the same RTP timestamp is
decoded into the same output frame.

The output device remains running between sounds. With the default 20 ms
common delay, the idle keepalive holds approximately 15 ms of silence in the
output path. The remaining 5 ms is reserved for multicast arrival, Opus
decoding, and enqueue scheduling. The controller places the first real frame
behind that silence so the sample reaches the speaker at the RTCP presentation
deadline. It does not restart the audio device for each sound.

The router defaults to requiring RTCP and remains silent without a usable BGM
mapping. If a usable RTCP mapping is unavailable, the controller selects the existing
arrival-time behavior for that complete SSRC generation. It never changes an
active generation from fallback timing to RTCP timing because that would cause
an audible timeline step.

## Configuration

```json
{
  "commonPlayoutDelayMs": 20,
  "audioDeviceCompensationMs": 0
}
```

`commonPlayoutDelayMs` is the shared delay added to the server media timeline.
All synchronized controllers should use the same value. The default is 20 ms.

`audioDeviceCompensationMs` corrects output latency that the native backend
cannot measure. A positive value means the device takes additional time to
present queued samples, so the controller enqueues audio that many
milliseconds earlier. Leave it at zero until a loopback or acoustic
measurement shows a repeatable per-device offset. Positive compensation
reduces enqueue headroom; increase `commonPlayoutDelayMs` when necessary.

## Logs and field testing

At each stream generation, the controller reports:

- selected timing mode (`rtcp` or `arrival_fallback`);
- BGM SSRC, RTP timestamp, and CNAME;
- common delay and device compensation;
- the mapping/arrival delta used to diagnose clock synchronization;
- predicted first-sample lateness in microseconds.

The periodic audio summary includes valid and invalid RTCP report counts,
report age, fallback count, timing mode, and the most recent start lateness.

For a two-controller test, start the same BGM track and compare the
`start lateness` values. A dual-channel recording or loopback capture is the
preferred final measurement because it includes output-device and analog-path
latency.

RTCP start scheduling aligns the generation epoch. Slow adaptive resampling
for long-running DAC clock drift is a separate follow-up.
