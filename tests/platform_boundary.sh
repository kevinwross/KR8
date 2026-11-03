#!/usr/bin/env bash
# Fails if an OS-specific header leaks outside src/platform.
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
violations="$(
  grep -R -n -E '#include[[:space:]]*<(windows\.h|Windows\.h|SDL3?/|X11/)' \
    "$root/src" "$root/include" 2>/dev/null |
    grep -v '/src/platform/' || true
)"
if [[ -n "$violations" ]]; then
  printf 'OS header outside src/platform:\n%s\n' "$violations" >&2
  exit 1
fi
printf 'platform boundary ok\n'
