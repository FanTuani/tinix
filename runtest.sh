#!/usr/bin/env bash
set -euo pipefail

build_dir="${BUILD_DIR:-build}"
jobs="${JOBS:-4}"

cmake -S . -B "${build_dir}"
cmake --build "${build_dir}" -j "${jobs}"
ctest --test-dir "${build_dir}" --output-on-failure "$@"

