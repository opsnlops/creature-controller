# RTCP audio synchronization

The controller receives Opus/RTP audio on UDP port 5004 and RTCP Sender
Reports on UDP port 5005. Dialog uses the creature's multicast group and BGM
uses `239.19.63.17`.

The server sends 10 ms / 480-sample RTP packets. Motion is updated every
20 ms, so the default common audio playout delay is 20 ms: two audio packets
and one motion frame.

## Clock requirement

Every controller host must synchronize its system clock to the Creature Server
through the LAN NTP service. RTCP maps the server's 48 kHz RTP timeline to NTP
wall time; it does not synchronize the host clock itself.

At the beginning of an RTP SSRC generation, the controller waits briefly for
fresh RTCP Sender Reports for both its dialog stream and the BGM stream. It
uses RTCP only when:

- each report matches the current RTP SSRC;
- both reports contain the same SDES CNAME;
- their NTP/RTP clock relationships agree;
- the mapped deadline is consistent with the packet's LAN arrival time.

The last check rejects mappings more than 50 ms from the arrival-based
estimate. The fallback warning includes a signed `mapping/arrival delta`; a
large value usually means the controller and server clocks need attention.

BGM is the master timeline. The dialog packet with the same RTP timestamp is
decoded into the same output frame.

The output device remains running between sounds. With the default 20 ms
common delay, the idle keepalive holds approximately 15 ms of silence in the
output path. The remaining 5 ms is reserved for multicast arrival, Opus
decoding, and enqueue scheduling. The controller places the first real frame
behind that silence so the sample reaches the speaker at the RTCP presentation
deadline. It does not restart the audio device for each sound.

If a usable RTCP mapping is unavailable, the controller selects the existing
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
- dialog and BGM SSRCs and the shared CNAME;
- the selected NTP/RTP mapping;
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
