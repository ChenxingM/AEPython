#!/bin/bash
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
cd "$HERE"
export DEVELOPER_DIR="${DEVELOPER_DIR:-/Applications/Xcode-beta.app/Contents/Developer}"

xcodebuild -project AEPython.xcodeproj -target AEPython -configuration Debug -arch arm64 \
  SYMROOT="$HERE/build" OBJROOT="$HERE/build" build

PLUGIN="$HERE/build/Debug/AEPython.plugin"
mkdir -p "$PLUGIN/Contents/Resources/Scripts"
rm -rf "$PLUGIN/Contents/Resources/Scripts/AEPython"
cp -R "$HERE/../Scripts/AEPython" "$PLUGIN/Contents/Resources/Scripts/AEPython"
find "$PLUGIN/Contents/Resources/Scripts" -name "__pycache__" -type d -prune -exec rm -rf {} + 2>/dev/null || true
codesign --force --sign - "$PLUGIN"

echo "Built and signed: $PLUGIN"
