#!/usr/bin/env bash
#
# Validate the MW160 plugin using pluginval.
#
# Usage:
#   ./scripts/validate_plugin.sh [build_dir] [strictness_level]
#
# This script downloads pluginval (if not cached), builds the plugin
# (if needed), and runs pluginval against the VST3 build at the
# specified strictness level (default: 5).
#
# On Linux, Xvfb is required for editor tests (the script wraps
# pluginval with xvfb-run automatically).
#
# Exit codes:
#   0  All validation checks passed
#   1  Validation failed or pluginval reported errors

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${1:-${PROJECT_DIR}/build}"
STRICTNESS="${2:-5}"
PLUGINVAL_VERSION="v1.0.4"

echo "=== MW160 Plugin Validation ==="
echo "Project:    ${PROJECT_DIR}"
echo "Build dir:  ${BUILD_DIR}"
echo "Strictness: ${STRICTNESS}"
echo ""

# --- Locate or download pluginval ---
PLUGINVAL=""

if command -v pluginval &>/dev/null; then
    PLUGINVAL="pluginval"
elif [[ -x "${BUILD_DIR}/pluginval" ]]; then
    PLUGINVAL="${BUILD_DIR}/pluginval"
else
    echo "=== Downloading pluginval ${PLUGINVAL_VERSION} ==="
    case "$(uname -s)" in
        Linux)
            PLUGINVAL_URL="https://github.com/Tracktion/pluginval/releases/download/${PLUGINVAL_VERSION}/pluginval_Linux.zip"
            curl -sL -o "${BUILD_DIR}/pluginval.zip" "${PLUGINVAL_URL}"
            unzip -o "${BUILD_DIR}/pluginval.zip" -d "${BUILD_DIR}"
            rm -f "${BUILD_DIR}/pluginval.zip"
            ;;
        Darwin)
            PLUGINVAL_URL="https://github.com/Tracktion/pluginval/releases/download/${PLUGINVAL_VERSION}/pluginval_macOS.zip"
            curl -sL -o "${BUILD_DIR}/pluginval.zip" "${PLUGINVAL_URL}"
            unzip -o "${BUILD_DIR}/pluginval.zip" -d "${BUILD_DIR}"
            rm -f "${BUILD_DIR}/pluginval.zip"
            ;;
        *)
            echo "ERROR: Unsupported platform for pluginval: $(uname -s)"
            exit 1
            ;;
    esac
    chmod +x "${BUILD_DIR}/pluginval"
    PLUGINVAL="${BUILD_DIR}/pluginval"
fi

echo "Using pluginval: ${PLUGINVAL}"
echo ""

# --- Build plugin if needed ---
VST3_PATH="${BUILD_DIR}/MW160_artefacts/Release/VST3/MW160.vst3"
if [[ ! -d "${VST3_PATH}" ]]; then
    echo "=== Building plugin ==="
    cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
    cmake --build "${BUILD_DIR}" --config Release --parallel
    echo ""
fi

if [[ ! -d "${VST3_PATH}" ]]; then
    # Try without Release subdirectory (single-config generators)
    VST3_PATH="${BUILD_DIR}/MW160_artefacts/VST3/MW160.vst3"
fi

if [[ ! -d "${VST3_PATH}" ]]; then
    echo "ERROR: VST3 plugin not found. Build the project first."
    exit 1
fi

echo "=== Validating VST3: ${VST3_PATH} ==="
echo ""

# --- Run pluginval ---
PLUGINVAL_CMD=(
    "${PLUGINVAL}"
    --validate-in-process
    --strictness-level "${STRICTNESS}"
    --validate "${VST3_PATH}"
)

# On Linux without a display, wrap with xvfb-run for editor tests
if [[ "$(uname -s)" == "Linux" && -z "${DISPLAY:-}" ]]; then
    if command -v xvfb-run &>/dev/null; then
        PLUGINVAL_CMD=( xvfb-run -a "${PLUGINVAL_CMD[@]}" )
    else
        echo "WARNING: No display and xvfb-run not found. Editor tests may fail."
    fi
fi

"${PLUGINVAL_CMD[@]}"

echo ""
echo "=== Validation PASSED (strictness level ${STRICTNESS}) ==="
