#!/usr/bin/env bash
set -euo pipefail
destination=$1
opencv_source=$2
mkdir -p "$destination/licenses"
cp LICENSE THIRD_PARTY_NOTICES.md "$destination/"
cp "$opencv_source/LICENSE" "$destination/licenses/OpenCV-Apache-2.0.txt"
for name in LGPL-3.0-only GPL-3.0-only GPL-2.0-only; do
    curl --fail --silent --show-error --location --retry 3 \
        "https://raw.githubusercontent.com/qt/qtbase/v6.8.3/LICENSES/$name.txt" \
        --output "$destination/licenses/Qt-$name.txt"
done
