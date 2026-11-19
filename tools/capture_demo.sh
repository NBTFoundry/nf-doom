#!/usr/bin/env bash
# Records the attract loop's first demo (DEMO1) from the host build under
# Xvfb, for the README: docs/assets/doom-demo.webp (animated WebP) and a still,
# docs/assets/doom-demo.png, both framed like the head unit's 1440x540 panel.
#
#   make doom-host && tools/capture_demo.sh [seconds]
#
# Needs Xvfb, ffmpeg (with libwebp) and ImageMagick (`magick`).
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
bin="$root/build/nbt-doom-host"
out="$root/docs/assets"
seconds="${1:-8}"
[[ -x "$bin" ]] || { echo "build $bin first (make doom-host)" >&2; exit 1; }
for tool in Xvfb ffmpeg magick; do
  command -v "$tool" >/dev/null || { echo "$tool is required" >&2; exit 1; }
done
mkdir -p "$out"
work="$(mktemp -d)"
cleanup() {
  [[ -n "${doom:-}" ]] && kill "$doom" 2>/dev/null || true
  [[ -n "${xvfb:-}" ]] && kill "$xvfb" 2>/dev/null || true
  rm -rf "${work:?}"
}
trap cleanup EXIT

exec 3<> >(:)
Xvfb -displayfd 3 -screen 0 1280x480x24 -nolisten tcp 2>/dev/null &
xvfb=$!
read -r -t 10 number <&3 || { echo "Xvfb did not start" >&2; exit 1; }
export DISPLAY=":$number"

mkdir -p "$work/data"
LIBGL_ALWAYS_SOFTWARE=1 NBT_NO_AUDIO=1 NBT_DATA_DIR="$work/data" \
  "$bin" -nosound >"$work/doom.log" 2>&1 &
doom=$!
# The title screen comes first; record once the demo's level is loaded.
for _ in $(seq 100); do
  grep -q '^gld_Precache' "$work/doom.log" && break
  kill -0 "$doom" 2>/dev/null || { cat "$work/doom.log" >&2; exit 1; }
  sleep 0.2
done
grep -q '^gld_Precache' "$work/doom.log" || { echo "the demo did not start" >&2; exit 1; }
sleep 1

ffmpeg -loglevel error -y -f x11grab -draw_mouse 0 -framerate 30 -video_size 1280x480 \
  -i "$DISPLAY" -t "$seconds" -c:v libx264rgb -crf 0 -preset ultrafast "$work/demo.mkv"

pad="pad=1440:540:(ow-iw)/2:(oh-ih)/2:black"
ffmpeg -loglevel error -y -i "$work/demo.mkv" \
  -vf "fps=15,$pad,scale=960:-1:flags=lanczos" \
  -c:v libwebp -lossless 0 -quality 70 -compression_level 6 -loop 0 "$out/doom-demo.webp"
ffmpeg -loglevel error -y -ss 4 -i "$work/demo.mkv" -frames:v 1 -vf "$pad" "$work/still.png"
bezel=22
magick "$work/still.png" -bordercolor "#171b22" -border "$bezel" \
  \( +clone -alpha extract -fill black -colorize 100 -fill white \
     -draw "roundrectangle 0,0 $((1440 + 2 * bezel - 1)),$((540 + 2 * bezel - 1)) 28,28" \) \
  -alpha off -compose CopyOpacity -composite -strip -define png:exclude-chunks=date,time \
  "$out/doom-demo.png"
if command -v pngquant >/dev/null; then
  pngquant --force --skip-if-larger --output "$out/doom-demo.png" -- "$out/doom-demo.png" || true
fi
echo "captures in $out"
