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
# The service runs as a systemd dynamic user; libnss-systemd lets tools show its name
grep -qE '^passwd:.*systemd' /etc/nsswitch.conf
if /usr/lib/creature-controller/start-creature NoSuchCreature; then exit 1; fi
# Debian container images exclude documentation during installation; inspect the archive too.
dpkg-deb -x "$package" /tmp/controller-package-content
[ -f /tmp/controller-package-content/usr/share/doc/creature-controller/README.md ]
apt-get remove -y creature-controller
[ ! -e /usr/lib/systemd/system/creature-controller@.service ]
printf 'Controller package installation and removal passed.\n'
