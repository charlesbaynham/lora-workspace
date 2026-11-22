#!/usr/bin/env bash
# Build lora_link and write the release the phone web app installs, into OUTDIR:
#
#   lora_link-<sha256 prefix>.bin   the image, named by its hash so a file never changes under a manifest
#   manifest.json                   {"fw", "size", "sha256", "file"}, written LAST
#
# Run by the web server's guest (homelab-infra, lora/lora.nix) on every new commit to master, with
# OUTDIR inside the directory it serves, so nothing built is ever committed. To try the update
# flow from your own machine: scripts/build_release.sh web/firmware (git-ignored) and serve web/.
#
# The build stamp ("fw") is read back out of the binary, so it is exactly what the board will
# report after it boots the image. Uses `pio` from PATH, else PlatformIO through uv.
set -euo pipefail
[ $# -eq 1 ] || { echo "usage: $0 OUTDIR" >&2; exit 2; }
out=$(realpath -m "$1")
cd "$(dirname "$0")/.."
export PLATFORMIO_CORE_DIR="${PLATFORMIO_CORE_DIR:-$PWD/.platformio}"

if command -v pio >/dev/null; then pio=(pio); else pio=(uvx --from platformio pio); fi
"${pio[@]}" run -e lora_link

bin=${PLATFORMIO_WORKSPACE_DIR:-.pio}/build/lora_link/firmware.bin
fw=$(grep -a -o '"fw":"[^"]*"' "$bin" | head -1 | sed 's/^"fw":"//; s/"$//')
[ -n "$fw" ] || { echo "no build stamp found in $bin" >&2; exit 1; }

sha=$(sha256sum "$bin" | cut -d' ' -f1)
file="lora_link-${sha:0:12}.bin"
mkdir -p "$out"
previous=$(grep -a -o '"file":"[^"]*"' "$out/manifest.json" 2>/dev/null | sed 's/^"file":"//; s/"$//' || true)

cp "$bin" "$out/$file.tmp" && mv -f "$out/$file.tmp" "$out/$file"
printf '{"fw":"%s","size":%d,"sha256":"%s","file":"%s"}\n' "$fw" "$(stat -c %s "$bin")" "$sha" "$file" > "$out/manifest.json.tmp"
mv -f "$out/manifest.json.tmp" "$out/manifest.json"

# Keep the previous image for a download that started before the switch; drop the rest.
find "$out" -maxdepth 1 -name 'lora_link-*.bin' ! -name "$file" ! -name "${previous:-/}" -delete
echo "released build $fw ($file) in $out"
