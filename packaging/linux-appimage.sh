#!/usr/bin/env bash
# Builds the Linux UI and assembles an AppDir. Run from the repository root.
set -euo pipefail

root="$(cd "$(dirname "$0")/.." && pwd)"
cd "$root"

if [[ "${KR8_SKIP_BUILD:-0}" != "1" ]]; then
    cmake --preset linux-gcc-release -DKR8_BUILD_UI=ON
    cmake --build --preset linux-gcc-release
fi
binary="${KR8_BINARY:-build/linux-gcc-release/kr8}"

appdir="$root/dist/KR8.AppDir"
rm -rf "$appdir"
mkdir -p "$appdir/usr/bin" "$appdir/usr/share/applications"
mkdir -p "$appdir/usr/share/icons/hicolor/256x256/apps"
mkdir -p "$appdir/usr/share/doc/kr8"

cp "$binary" "$appdir/usr/bin/kr8"
cp resources/kr8.png "$appdir/usr/share/icons/hicolor/256x256/apps/kr8.png" 2>/dev/null || true
cp README.md LICENSE THIRD_PARTY_NOTICES.md "$appdir/usr/share/doc/kr8/"

cat > "$appdir/kr8.desktop" <<'EOF'
[Desktop Entry]
Type=Application
Name=KR8
Comment=8-bit teaching computer debugger
Exec=kr8
Icon=kr8
Categories=Development;Education;
Terminal=false
EOF
cp "$appdir/kr8.desktop" "$appdir/usr/share/applications/kr8.desktop"

cat > "$appdir/AppRun" <<'EOF'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/usr/bin/kr8" "$@"
EOF
chmod +x "$appdir/AppRun" "$appdir/usr/bin/kr8"

tool="${APPIMAGETOOL:-appimagetool}"
if command -v "$tool" >/dev/null 2>&1; then
    ARCH=x86_64 "$tool" "$appdir" "$root/dist/KR8-x86_64.AppImage"
else
    echo "appimagetool not found; AppDir is ready at $appdir" >&2
    exit 2
fi
