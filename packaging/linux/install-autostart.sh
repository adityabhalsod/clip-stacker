#!/usr/bin/env bash

# Stop on the first failure so autostart installation does not half-complete.
set -euo pipefail

# Create the per-user autostart directory required by freedesktop desktop sessions.
mkdir -p "$HOME/.config/autostart"

# Copy the installed desktop file into the user's autostart directory.
cp "/usr/share/applications/clip-stacker.desktop" "$HOME/.config/autostart/clip-stacker.desktop"
