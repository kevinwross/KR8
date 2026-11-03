#!/usr/bin/env bash
# Xvfb smoke test for the Linux SDL3 UI.
# Usage: ui_smoke_linux.sh <kr8_ui_smoke> <hello.k8|hello.asm> <screenshot.png>
set -euo pipefail

binary="${1:?missing UI binary}"
image="${2:?missing image}"
shot="${3:?missing screenshot path}"

xvfb-run -a -s "-screen 0 1440x900x24" bash -c '
set -euo pipefail
binary="$1"
image="$2"
shot="$3"
"$binary" --ui "$image" &
pid=$!
trap "kill $pid 2>/dev/null || true" EXIT
sleep 2
# Step a line, then run to completion. Both are keyboard-driven and do not
# depend on a window manager or a pointer device.
xdotool key F10
sleep 0.4
xdotool key F5
sleep 1
import -window root "$shot"
test -s "$shot"
' _ "$binary" "$image" "$shot"
