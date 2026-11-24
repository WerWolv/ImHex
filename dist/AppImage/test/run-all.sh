#!/usr/bin/env bash
# Usage: run-all.sh <path/to/ImHex.AppImage>
#
# Checks that the AppImage starts on every distribution below, under both X11
# and Wayland. Each combination runs run-test.sh in its own container, all in
# parallel. Prints each test's output, then a summary. Exits with 1 if any
# test failed.
#
# Requires Docker.
set -euo pipefail

distros=("ubuntu:24.04" "ubuntu:26.10" "fedora:45")
displays=("x11" "wayland")

appimage=$(realpath "${1:?"Usage: $0 <path/to/ImHex.AppImage>"}")
test_dir=$(dirname "$(realpath "$0")")
log_dir=$(mktemp -d)

# "ubuntu:24.04" -> "ubuntu-24.04", usable in image tags and file names.
slug() {
    echo "${1//:/-}"
}

# Fold output into collapsible sections in GitHub Actions logs.
begin_section() {
    if [[ "${GITHUB_ACTIONS:-}" == true ]]; then echo "::group::$*"; else echo "======== $*"; fi
}
end_section() {
    if [[ "${GITHUB_ACTIONS:-}" == true ]]; then echo "::endgroup::"; fi
}

# ---------------------------------------------------------------------------
# 1. Build one test image per distribution, in parallel
# ---------------------------------------------------------------------------

echo "==> Building test images for: ${distros[*]}"
declare -A build_pids
for distro in "${distros[@]}"; do
    docker build --build-arg "BASE_IMAGE=${distro}" -t "imhex-appimage-test:$(slug "${distro}")" "${test_dir}" \
        >"${log_dir}/build-$(slug "${distro}").log" 2>&1 &
    build_pids[${distro}]=$!
done

build_failed=false
for distro in "${distros[@]}"; do
    if ! wait "${build_pids[${distro}]}"; then
        echo "Building the ${distro} image failed:" >&2
        cat "${log_dir}/build-$(slug "${distro}").log" >&2
        build_failed=true
    fi
done
[[ "${build_failed}" == false ]] || exit 1

# ---------------------------------------------------------------------------
# 2. Start every distribution/display combination, in parallel
# ---------------------------------------------------------------------------

tests=()
declare -A test_pids
for distro in "${distros[@]}"; do
    for display in "${displays[@]}"; do
        test="${distro} ${display}"
        tests+=("${test}")
        docker run --rm --init --network none \
            --mount "type=bind,src=${appimage},dst=/tmp/ImHex.AppImage,readonly" \
            "imhex-appimage-test:$(slug "${distro}")" "${display}" /tmp/ImHex.AppImage \
            >"${log_dir}/$(slug "${distro}")-${display}.log" 2>&1 &
        test_pids[${test}]=$!
    done
done
echo "==> Running ${#tests[@]} tests in parallel"

# ---------------------------------------------------------------------------
# 3. Wait for each test, show its output, then summarize
# ---------------------------------------------------------------------------

declare -A results
for test in "${tests[@]}"; do
    read -r distro display <<< "${test}"
    if wait "${test_pids[${test}]}"; then
        results[${test}]=PASS
    else
        results[${test}]=FAIL
    fi

    # Fold passing tests away; keep failures visible.
    if [[ "${results[${test}]}" == PASS ]]; then
        begin_section "PASS: ${test}"
        cat "${log_dir}/$(slug "${distro}")-${display}.log"
        end_section
    else
        echo "======== FAIL: ${test}"
        cat "${log_dir}/$(slug "${distro}")-${display}.log"
    fi
done

echo
echo "==> Summary"
failures=0
for test in "${tests[@]}"; do
    printf '    %-4s  %s\n' "${results[${test}]}" "${test}"
    [[ "${results[${test}]}" == PASS ]] || failures=$((failures + 1))
done

if ((failures > 0)); then
    echo "${failures} of ${#tests[@]} tests failed" >&2
    exit 1
fi
echo "All ${#tests[@]} tests passed"
