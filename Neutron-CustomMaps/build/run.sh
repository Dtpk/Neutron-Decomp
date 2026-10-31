#!/usr/bin/env bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

# Add bundled portable libraries to library path
if [ -d "${SCRIPT_DIR}/libs" ]; then
    if [ -n "$LD_LIBRARY_PATH" ]; then
        export LD_LIBRARY_PATH="${SCRIPT_DIR}/libs:${LD_LIBRARY_PATH}"
    else
        export LD_LIBRARY_PATH="${SCRIPT_DIR}/libs"
    fi
fi

# Run the binary. If it fails with GLX/visual error on software/VM sessions, provide automatic fallback.
if ! "${SCRIPT_DIR}/neutron_map_editor" "$@"; then
    EXIT_CODE=$?
    # If exited with error and LIBGL_ALWAYS_SOFTWARE is not already set, offer or try software fallback
    if [ -z "$LIBGL_ALWAYS_SOFTWARE" ]; then
        echo ""
        echo "[run.sh] Retrying with LIBGL_ALWAYS_SOFTWARE=1 (software rasterizer fallback)..."
        LIBGL_ALWAYS_SOFTWARE=1 "${SCRIPT_DIR}/neutron_map_editor" "$@"
    else
        exit $EXIT_CODE
    fi
fi
