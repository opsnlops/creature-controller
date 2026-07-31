#!/usr/bin/env bash

set -u
set -o pipefail

section() {
    printf '\n== %s ==\n' "$1"
}

section "Report"
printf 'Generated: '
date --iso-8601=ns
printf 'Host: '
hostname
printf 'Uptime: '
uptime -p

section "Clock status"
timedatectl status --no-pager

section "NTP synchronization"
timedatectl timesync-status

section "All timesync properties"
timedatectl show-timesync --all

section "Effective timesyncd configuration"
systemd-analyze cat-config systemd/timesyncd.conf

section "timesyncd service"
systemctl status systemd-timesyncd --no-pager --full || true

section "Recent timesyncd log"
journalctl -u systemd-timesyncd -n 50 --no-pager || true

section "Route to configured NTP server"
ip route get 10.3.2.6 || true
