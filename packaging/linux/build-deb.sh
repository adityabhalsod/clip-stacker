#!/usr/bin/env bash

# Stop on the first failure so broken packages are not produced silently.
set -euo pipefail

# Resolve the repository root relative to this helper script.
ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"

# Configure a release build suitable for packaging.
cmake -S "$ROOT_DIR" -B "$ROOT_DIR/build" -G Ninja -DCMAKE_BUILD_TYPE=Release

# Compile the application and its resources.
cmake --build "$ROOT_DIR/build"

# Produce the Debian package with CPack's DEB generator.
cpack --config "$ROOT_DIR/build/CPackConfig.cmake" -G DEB
