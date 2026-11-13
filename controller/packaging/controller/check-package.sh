#!/bin/sh
# Run as root in a disposable Debian container.
set -eu
package=${1:?Usage: check-package.sh controller.deb}
[ "$(dpkg-deb -f "$package" Package)" = creature-controller ]
apt-get update
# A hand-made SSH advertisement like the ones on older hosts; install should retire it
mkdir -p /etc/avahi/services
printf '<service-group><name>%%h</name><service><type>_ssh._tcp</type><port>22</port></service></service-group>\n' \
    > /etc/avahi/services/ssh.service
apt-get install -y "$package" systemd udev
creature-controller --version
if ldd /usr/bin/creature-controller | grep -q 'not found'; then exit 1; fi
[ ! -e /usr/bin/creature-audio-router ]
systemd-analyze --man=no verify /usr/lib/systemd/system/creature-controller@.service
udevadm verify /usr/lib/udev/rules.d/70-creature-controller.rules
# Every board name has to resolve to a packaged creature configuration.
for creature in caroll beaky mango kenny; do
    [ -f "/usr/share/creature-controller/creatures/$creature.json" ]
    [ -f "/usr/share/creature-controller/controllers/$creature.json" ]
done
[ -d /etc/creature/controller ]
dpkg -s avahi-daemon >/dev/null
[ -f /etc/avahi/services/creature-controller-ssh.service ]
[ ! -e /etc/avahi/services/ssh.service ]
[ -f /etc/avahi/ssh.service.replaced-by-creature-controller ]
dpkg-query -W -f='${Conffiles}\n' creature-controller | grep -q creature-controller-ssh.service
# The service user exists, with access to the serial board and the sound card
id -nG creature-controller | grep -qw dialout
id -nG creature-controller | grep -qw audio
[ "$(getent passwd creature-controller | cut -d: -f7)" = /usr/sbin/nologin ]
if /usr/lib/creature-controller/start-creature NoSuchCreature; then exit 1; fi
# Debian container images exclude documentation during installation; inspect the archive too.
dpkg-deb -x "$package" /tmp/controller-package-content
[ -f /tmp/controller-package-content/usr/share/doc/creature-controller/README.md ]
# Debian Policy 10.1: nothing ships under an aliased top-level directory such as /bin. The
# usr-merge symlinks hide that on an installed system, so check the archive's own paths.
if dpkg-deb -c "$package" | awk '{print $6}' | grep -E '^\./(bin|sbin|lib[^/]*)/'; then exit 1; fi
[ -f /tmp/controller-package-content/usr/bin/creature-controller ]
# The manual is generated from --help, so it must list every long option.
manual=/tmp/controller-package-content/usr/share/man/man8/creature-controller.8.gz
gzip -t "$manual"
gzip -dc "$manual" | sed 's/\\-/-/g' > /tmp/creature-controller.8
grep -q '^\.TH CREATURE-CONTROLLER "8"' /tmp/creature-controller.8
# Search only the OPTIONS section; the SYNOPSIS lists every option too and would hide a gap.
awk '/^\.SH OPTIONS/ { found = 1; next } found && /^\.SH / { exit } found' /tmp/creature-controller.8 \
    > /tmp/creature-controller.options
for option in $(creature-controller --help | grep -o -- '--[a-z-]*' | sort -u); do
    grep -qF -- "$option" /tmp/creature-controller.options
done
grep -q '^\.SH OPTIONS' /tmp/creature-controller.8
grep -q '^\.SH FILES' /tmp/creature-controller.8
apt-get remove -y creature-controller
[ ! -e /usr/lib/systemd/system/creature-controller@.service ]
printf 'Controller package installation and removal passed.\n'
