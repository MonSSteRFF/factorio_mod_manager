#!/usr/bin/env bash
# Wrapper: scripts/check-symbols.sh [path-to-Factorio/bin/x64]
exec python3 -I "$(dirname "$0")/check-symbols.py" "$@"
