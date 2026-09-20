#!/usr/bin/env sh
set -eu
cd "$(dirname "$0")"
echo "Open http://localhost:8080 in Chrome/Chromium"
python3 -m http.server 8080 --bind 127.0.0.1
