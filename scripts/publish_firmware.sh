#!/usr/bin/env bash
# Build lora_link and publish it for the phone web app: copies firmware.bin to
# web/firmware/ and writes web/firmware/manifest.json. Commit both files; once the
# web app is deployed, the "Firmware update" card installs this image.
# The build stamp ("fw") is read back out of the binary, so it is exactly what the
# board will report after it boots the image.
set -euo pipefail
cd "$(dirname "$0")/.."
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$PWD/.platformio}"

uvx --from platformio pio run -e lora_link

bin=.pio/build/lora_link/firmware.bin
fw=$(grep -a -o '"fw":"[^"]*"' "$bin" | head -1 | sed 's/^"fw":"//; s/"$//')
[ -n "$fw" ] || { echo "no build stamp found in $bin" >&2; exit 1; }

mkdir -p web/firmware
cp "$bin" web/firmware/lora_link.bin
printf '{"fw":"%s","size":%d,"sha256":"%s"}\n' "$fw" "$(stat -c %s "$bin")" "$(sha256sum "$bin" | cut -d' ' -f1)" > web/firmware/manifest.json
echo "published build $fw ($(stat -c %s "$bin") bytes) to web/firmware/"
