#!/usr/bin/env bash
#
# Regenerate golden reference files for MW160 regression tests.
#
# Usage:
#   ./scripts/regenerate_golden.sh [build_dir]
#
# This script builds the test binary (if needed) and runs the golden
# regression tests with MW160_REGENERATE_GOLDEN=1, causing them to
# write new golden files instead of comparing against existing ones.
#
# After regeneration, review the git diff on tests/golden/ to verify
# the changes are intentional before committing.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${1:-${PROJECT_DIR}/build}"

echo "=== MW160 Golden File Regeneration ==="
echo "Project:   ${PROJECT_DIR}"
echo "Build dir: ${BUILD_DIR}"
echo ""

# Configure and build
cmake -S "${PROJECT_DIR}" -B "${BUILD_DIR}" -DCMAKE_BUILD_TYPE=Release
cmake --build "${BUILD_DIR}" --target MW160Tests -j "$(nproc)"

# Run golden tests in regeneration mode
echo ""
echo "=== Regenerating golden files ==="
MW160_REGENERATE_GOLDEN=1 "${BUILD_DIR}/MW160Tests" "[golden]" --reporter compact

echo ""
echo "=== Golden files regenerated ==="
echo "Files in tests/golden/:"
ls -la "${PROJECT_DIR}/tests/golden/"
echo ""
echo "Review changes with: git diff tests/golden/"
