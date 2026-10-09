#!/usr/bin/env bash

set -euo pipefail

# Use the same package target and validation as GitHub Actions.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
docker build --pull --target package -f "$script_dir/Dockerfile" "$script_dir" -t creature-packages
package_export=$(mktemp -d "${TMPDIR:-/tmp}/creature-packages.XXXXXX")
trap 'rm -r "$package_export"' EXIT
package_container=$(docker create creature-packages)
trap 'docker rm "$package_container"; rm -r "$package_export"' EXIT
docker cp "$package_container:/package/." "$package_export/"

shopt -s nullglob
for package_name in creature-controller creature-audio-router; do
    packages=("$package_export/${package_name}"_*.deb)
    if [ "${#packages[@]}" -ne 1 ]; then
        echo "Expected exactly one ${package_name} Debian package, found ${#packages[@]}" >&2
        exit 1
    fi
done

mkdir -p "$script_dir/package"
cp "$package_export/"*.deb "$script_dir/package/"
ls -lh "$script_dir/package/"*.deb
