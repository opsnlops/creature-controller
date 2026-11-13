#!/usr/bin/env bash
# Run lintian on the packages that make_packages.sh left in package/, inside Debian
# rather than on the host. Exits with lintian's status; CI treats findings as report-only.

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
shopt -s nullglob
packages=("$script_dir"/package/*.deb)
if [ "${#packages[@]}" -eq 0 ]; then
    echo "No packages in $script_dir/package; run make_packages.sh first" >&2
    exit 1
fi

docker run --rm -v "$script_dir/package:/package:ro" debian:trixie sh -c \
    'apt-get update -qq && apt-get install -y -qq --no-install-recommends lintian >/dev/null &&
     lintian --version && runuser -u nobody -- lintian --info --display-info --tag-display-limit 0 /package/*.deb'
