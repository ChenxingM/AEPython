#!/bin/bash
set -e

AE="${AE_APP:-/Applications/Adobe After Effects 2026}"
HERE="$(cd "$(dirname "$0")" && pwd)"
PLUGIN_SRC="$HERE/build/Debug/AEPython.plugin"
JSX_SRC="$HERE/../Scripts/AEPython.jsx"

if [ ! -d "$PLUGIN_SRC" ]; then
  echo "ERROR: $PLUGIN_SRC not found. Build first (Mac/build.sh)." >&2
  exit 1
fi

echo "Installing AEPython.plugin -> $AE/Plug-ins/AEPython/"
mkdir -p "$AE/Plug-ins/AEPython"
rm -rf "$AE/Plug-ins/AEPython/AEPython.plugin"
cp -R "$PLUGIN_SRC" "$AE/Plug-ins/AEPython/AEPython.plugin"

echo "Installing AEPython.jsx -> $AE/Scripts/Startup/"
cp "$JSX_SRC" "$AE/Scripts/Startup/AEPython.jsx"

echo "Done. Restart After Effects 2026."
