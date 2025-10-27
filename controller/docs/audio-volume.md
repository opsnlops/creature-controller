# Audio Volume Initialization

This application pushes audio through SDL/ALSA, but the actual hardware volume is controlled by ALSA mixer levels. To guarantee a predictable loudness at boot, use the helper script shipped in `scripts/set-alsa-volume.sh` and optionally wire it into a systemd unit.

## Prerequisites

- `alsa-utils` (provides `amixer`)
- Access to the mixer you want to control (e.g., `Master`, `PCM`, or a USB DAC control)

## Script Usage

```
./scripts/set-alsa-volume.sh \
    --card 0 \
    --control Master \
    --percent 95% \
    --unmute
```

### Options

- `--card <index|name>`: ALSA card target. Use `aplay -l` to list cards.
- `--control <name>`: Mixer control to adjust (default `Master`).
- `--percent <0-100%>`: Desired volume percentage (default `95%`).
- `--unmute`: Clears the mute flag on the control.
- `--dry-run`: Prints the `amixer` command without executing it.

Install `alsa-utils` if the script reports `amixer` missing:

```
sudo apt-get update
sudo apt-get install -y alsa-utils
```

## Systemd Integration

Create a unit so the mixer level is enforced during boot:

```
# /etc/systemd/system/alsa-volume.service
[Unit]
Description=Set ALSA mixer level for Creature Controller audio
After=sound.target

[Service]
Type=oneshot
ExecStart=/usr/local/bin/set-alsa-volume.sh --card 0 --control Master --percent 95% --unmute

[Install]
WantedBy=multi-user.target
```

Steps:

1. Copy `scripts/set-alsa-volume.sh` to `/usr/local/bin/set-alsa-volume.sh` (or adjust `ExecStart` to point to the repo copy) and ensure it is executable.
2. Put the unit file above into `/etc/systemd/system/alsa-volume.service`.
3. Enable and start it:
   ```
   sudo systemctl enable --now alsa-volume.service
   ```

On the next boot, the mixer level will be set before the creature controller launches.
