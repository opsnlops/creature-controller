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
creature-audio-router --check-config --config /tmp/router-package-content/usr/share/doc/creature-audio-router/examples/audio-router-bass-example.json
if ldd /usr/bin/creature-audio-router | grep -q 'not found'; then exit 1; fi
[ ! -e /usr/bin/creature-controller ]
[ -d /etc/creature/audio-router ]
dpkg-query -W -f='${Conffiles}\n' creature-audio-router | grep -q '/etc/creature/audio-router.json'
systemd-analyze verify /usr/lib/systemd/system/creature-audio-router.service /usr/lib/systemd/system/creature-audio-router@.service
printf '\n' >> /etc/creature/audio-router.json
before=$(md5sum /etc/creature/audio-router.json)
dpkg -i "$package"
[ "$before" = "$(md5sum /etc/creature/audio-router.json)" ]
apt-get remove -y creature-audio-router
[ -f /etc/creature/audio-router.json ]
apt-get purge -y creature-audio-router
[ ! -e /etc/creature/audio-router.json ]
printf 'Router package installation, configuration preservation, removal, and purge passed.\n'
