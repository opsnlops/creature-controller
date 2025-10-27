#!/usr/bin/env bash
#
# Simple helper to set an ALSA mixer control to a desired volume percentage.
# Defaults target a typical Raspberry Pi / USB DAC layout but can be overridden.

set -euo pipefail

usage() {
    cat <<'EOF'
Usage: set-alsa-volume.sh [options]

Options:
  --card <index|name>     ALSA card index or name to target (default: 0)
  --control <name>        Mixer control to update (default: Master)
  --percent <0-100%>      Volume percentage (default: 95%)
  --unmute                Also clear mute on the control
  --dry-run               Print the amixer command without running it
  -h, --help              Show this help message

Examples:
  ./set-alsa-volume.sh
  ./set-alsa-volume.sh --card 1 --control PCM --percent 90%
EOF
}

card="0"
control="Master"
percent="95%"
unmute=false
dry_run=false

while [[ $# -gt 0 ]]; do
    case "$1" in
    --card)
        card="$2"
        shift 2
        ;;
    --control)
        control="$2"
        shift 2
        ;;
    --percent)
        percent="$2"
        shift 2
        ;;
    --unmute)
        unmute=true
        shift
        ;;
    --dry-run)
        dry_run=true
        shift
        ;;
    -h | --help)
        usage
        exit 0
        ;;
    *)
        echo "Unknown option: $1" >&2
        usage >&2
        exit 1
        ;;
    esac
done

if ! command -v amixer >/dev/null 2>&1; then
    echo "amixer binary not found; install alsa-utils first." >&2
    exit 1
fi

cmd=(amixer -c "${card}" sset "${control}" "${percent}")

if $unmute; then
    cmd+=("unmute")
fi

if $dry_run; then
    printf 'Would run:' >&2
    printf ' %q' "${cmd[@]}"
    printf '\n' >&2
else
    "${cmd[@]}"
fi
