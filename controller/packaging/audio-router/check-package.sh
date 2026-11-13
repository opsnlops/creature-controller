#!/bin/sh
# Run as root in a disposable Debian container.
set -eu
package=${1:?Usage: check-package.sh router.deb}
[ "$(dpkg-deb -f "$package" Package)" = creature-audio-router ]
version=$(dpkg-deb -f "$package" Version)
apt-get update
apt-get install -y "$package" systemd
creature-audio-router --version | grep -Fxq "$version"
creature-audio-router --check-config
# Debian container images exclude documentation during installation; inspect the archive too.
dpkg-deb -x "$package" /tmp/router-package-content
gzip -t /tmp/router-package-content/usr/share/man/man1/creature-audio-router.1.gz
# Debian Policy 10.1: nothing ships under an aliased top-level directory such as /bin.
if dpkg-deb -c "$package" | awk '{print $6}' | grep -E '^\./(bin|sbin|lib[^/]*)/'; then exit 1; fi
creature-audio-router --check-config --config /tmp/router-package-content/usr/share/doc/creature-audio-router/examples/audio-router-bass-example.json
if ldd /usr/bin/creature-audio-router | grep -q 'not found'; then exit 1; fi
dpkg -s avahi-daemon >/dev/null
# The service user exists, with access to the sound card
id -nG creature-audio-router | grep -qw audio
[ "$(getent passwd creature-audio-router | cut -d: -f7)" = /usr/sbin/nologin ]
creature-audio-router --help | grep -q -- '--name'
[ ! -e /usr/bin/creature-controller ]
[ -d /etc/creature/audio-router ]
dpkg-query -W -f='${Conffiles}\n' creature-audio-router | grep -q '/etc/creature/audio-router.json'
# The archive check above covers the manual excluded by the container's dpkg policy.
systemd-analyze --man=no verify /usr/lib/systemd/system/creature-audio-router.service /usr/lib/systemd/system/creature-audio-router@.service
printf '\n' >> /etc/creature/audio-router.json
before=$(md5sum /etc/creature/audio-router.json)
dpkg -i "$package"
[ "$before" = "$(md5sum /etc/creature/audio-router.json)" ]
apt-get remove -y creature-audio-router
[ -f /etc/creature/audio-router.json ]
apt-get purge -y creature-audio-router
[ ! -e /etc/creature/audio-router.json ]
printf 'Router package installation, configuration preservation, removal, and purge passed.\n'
