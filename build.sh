#!/usr/bin/env bash
# Builds from WSL through the Windows toolchain: runs build.cmd with cmd.exe.
set -euo pipefail
cd "$(dirname "$0")"
case "$PWD" in
  /mnt/[a-z]/*) ;;
  *) echo "build.sh must run from a Windows drive (/mnt/c/...), cmd.exe cannot use $PWD" >&2; exit 1 ;;
esac
exec cmd.exe /c build.cmd
