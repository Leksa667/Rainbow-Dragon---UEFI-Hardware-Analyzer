#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

cd "$PROJECT_ROOT"

echo "[BUILD] Building DragonTool.efi with Docker..."

docker build -t dragontool-builder -f Dockerfile --target export --output type=local,dest="./build" .

echo "[OK]    Build successful!"
echo "[INFO] Output: $PROJECT_ROOT/build/DragonTool.efi"
