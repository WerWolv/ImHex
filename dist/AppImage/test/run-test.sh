#!/usr/bin/env bash
# Usage: run-test.sh <x11|wayland> <path/to/ImHex.AppImage>
#
# Starts ImHex from its AppImage on a virtual display and checks that the main
# window opens and stays open. Meant to run inside the container from the
# Dockerfile next to this script.
#
# What "pass" means:
#   1. ImHex did not exit (e.g. because of a library loading error), and
#   2. its main window, titled "ImHex", was created (the splash screen is
#      titled "Starting ImHex..." and does not count), and
#   3. ImHex is still running a few seconds later.
#
# Not tested: rendering, input, GPU drivers, real desktop environments.
set -euo pipefail

display_type=${1:?"Usage: $0 <x11|wayland> <path/to/ImHex.AppImage>"}
appimage_source=${2:?"Usage: $0 <x11|wayland> <path/to/ImHex.AppImage>"}
timeout_seconds=${TIMEOUT_SECONDS:-120}

# Keep all state (home, runtime dir, logs) in a throwaway directory.
work_dir=$(mktemp -d)
export HOME="${work_dir}/home"
export XDG_RUNTIME_DIR="${work_dir}/runtime"
mkdir -m 700 "${HOME}" "${XDG_RUNTIME_DIR}"
display_log="${work_dir}/display.log"
imhex_log="${work_dir}/imhex.log"

# Work on an executable copy: CI artifacts lose their executable bit.
appimage="${work_dir}/ImHex.AppImage"
cp "${appimage_source}" "${appimage}"
chmod +x "${appimage}"

log() {
    echo "==> $*"
}

wait_for_socket() {
    local socket=$1
    for _ in $(seq 50); do
        [[ -S "${socket}" ]] && return 0
        sleep 0.2
    done
    echo "Display server did not create ${socket}. Its log:" >&2
    cat "${display_log}" >&2
    return 1
}

# ---------------------------------------------------------------------------
# Virtual displays
# ---------------------------------------------------------------------------

start_x11() {
    log "Starting Xvfb (virtual X11 server)"
    Xvfb :99 -screen 0 1600x900x24 -nolisten tcp >"${display_log}" 2>&1 &
    export DISPLAY=:99
    unset WAYLAND_DISPLAY
    wait_for_socket /tmp/.X11-unix/X99
}

start_wayland() {
    log "Starting Weston (Wayland compositor, without a screen)"
    export WAYLAND_DISPLAY=wayland-test
    # Without DISPLAY, ImHex cannot fall back to X11: it must use Wayland.
    unset DISPLAY
    weston --backend=headless --renderer=pixman --socket="${WAYLAND_DISPLAY}" \
        --no-config --idle-time=0 >"${display_log}" 2>&1 &
    wait_for_socket "${XDG_RUNTIME_DIR}/${WAYLAND_DISPLAY}"
    # Log the Wayland requests ImHex makes, so we can see its window title.
    export WAYLAND_DEBUG=client
}

# ---------------------------------------------------------------------------
# Main window detection
# ---------------------------------------------------------------------------

main_window_is_open_x11() {
    # List all windows and look for a visible one titled "ImHex" or "ImHex - <file>".
    local window_id
    window_id=$(xwininfo -root -tree |
        awk '/^ +0x[0-9a-f]+ "ImHex( - [^"]*)?":/ { print $1; exit }')
    [[ -n "${window_id}" ]] && xwininfo -id "${window_id}" | grep -q 'Map State: IsViewable'
}

main_window_is_open_wayland() {
    # WAYLAND_DEBUG logs every request, including the one that sets the title.
    grep -Eq 'xdg_toplevel[#@][0-9]+\.set_title\("ImHex' "${imhex_log}"
}

# ---------------------------------------------------------------------------
# Diagnostics
# ---------------------------------------------------------------------------

# Show whether a few libraries that are known to cause trouble when mixed were
# loaded from the host or from the AppImage. For information only.
print_library_origins() {
    local pid
    # Match the executable path: ImHex renames its own process at runtime.
    pid=$(pgrep -f '/usr/bin/imhex( |$)' | head -n 1)
    if [[ -z "${pid}" ]]; then
        echo "    (ImHex process not found)"
        return 0
    fi
    log "Where some libraries were loaded from:"
    for library in libsystemd libmount libdbus-1 libgtk-3 libdecor; do
        local paths
        paths=$(grep -oE "/[^ ]*/${library}[.-][^ ]*" "/proc/${pid}/maps" | sort -u || true)
        if [[ -z "${paths}" ]]; then
            echo "    ${library}: not loaded"
        else
            while read -r path; do
                case "${path}" in
                    /tmp/appimage_extracted_*) echo "    ${library}: AppImage (${path#/tmp/appimage_extracted_*/})" ;;
                    *)                         echo "    ${library}: host (${path})" ;;
                esac
            done <<< "${paths}"
        fi
    done
}

print_imhex_log() {
    log "ImHex output (last 50 lines, Wayland protocol messages hidden):"
    grep -vE '^\[[0-9:. ]+\] ' "${imhex_log}" | tail -n 50 || true
}

# ---------------------------------------------------------------------------
# Test
# ---------------------------------------------------------------------------

source /etc/os-release
log "Testing on ${PRETTY_NAME} with ${display_type}"

case "${display_type}" in
    x11)     start_x11 ;;
    wayland) start_wayland ;;
    *)       echo "Unknown display type: ${display_type} (expected x11 or wayland)" >&2; exit 2 ;;
esac

# Extract instead of mounting: FUSE is not available in containers.
log "Starting ImHex"
"${appimage}" --appimage-extract-and-run >"${imhex_log}" 2>&1 &
imhex_pid=$!

deadline=$((SECONDS + timeout_seconds))
while ((SECONDS < deadline)); do
    if ! kill -0 "${imhex_pid}" 2>/dev/null; then
        exit_code=0
        wait "${imhex_pid}" || exit_code=$?
        print_imhex_log
        echo "FAIL: ImHex exited with code ${exit_code} before opening its main window" >&2
        exit 1
    fi

    if "main_window_is_open_${display_type}"; then
        log "Main window is open, checking that ImHex keeps running"
        sleep 5
        if ! kill -0 "${imhex_pid}" 2>/dev/null; then
            print_imhex_log
            echo "FAIL: ImHex exited shortly after opening its main window" >&2
            exit 1
        fi
        print_library_origins
        kill "${imhex_pid}"
        echo "PASS: ImHex opened its main window on ${PRETTY_NAME} (${display_type})"
        exit 0
    fi

    sleep 1
done

print_imhex_log
echo "FAIL: ImHex did not open its main window within ${timeout_seconds} seconds" >&2
exit 1
