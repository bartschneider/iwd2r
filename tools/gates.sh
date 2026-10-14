#!/usr/bin/env bash
# The CI gate, run locally: the same steps .github/workflows/ci.yml runs,
# for when GitHub Actions is disabled (gh workflow disable CI) to save
# minutes. Linux presets always; the Windows builds with --windows.
#
#   tools/gates.sh [--base REV] [--windows] [--skip-clang]
#
# --base is the evidence check's base (default: iwd2r/main, what CI's push
# event would have used). Engine tests run serially and muted. Exits 1 on the
# first failing step and prints a summary.
set -u
cd "$(dirname "$0")/.."
BASE=iwd2r/main; WINDOWS=0; CLANG=1
while [ $# -gt 0 ]; do
    case "$1" in
        --base) BASE="$2"; shift 2 ;;
        --windows) WINDOWS=1; shift ;;
        --skip-clang) CLANG=0; shift ;;
        *) echo "gates: unknown option $1" >&2; exit 2 ;;
    esac
done
export IWD2R_AUDIO=null SDL_AUDIO_DRIVER=dummy
# Data tests read a Linux-local copy of the vanilla game when one exists:
# the oracle's copy on the Windows drive is locked while a sandbox replay
# maps it, which fails every engine-starting test with "bad key file".
# Make it with: rsync -a --exclude temp --exclude cache --exclude tempsave
#   --exclude mpsave --exclude ScrnShot /mnt/c/Users/thier/iwd2r-oracle/game/ ~/.local/share/iwd2r/game/
LINUX_DATA="${IWD2R_HOME:-$HOME/.local/share/iwd2r}/game"
if [ -z "${IWD2_DATA:-}" ] && [ -f "$LINUX_DATA/CHITIN.KEY" ]; then export IWD2_DATA="$LINUX_DATA"; fi
# The start-screen smoke test's frame hash depends on the game copy, so each
# machine pins its own: tests/smoke_start_screen.cpp requires the hash in
# IWD2R_SMOKE_HASH when it is set.
SMOKE_HASH_FILE="${IWD2R_HOME:-$HOME/.local/share/iwd2r}/smoke-hash"
if [ -z "${IWD2R_SMOKE_HASH:-}" ] && [ -f "$SMOKE_HASH_FILE" ]; then
    IWD2R_SMOKE_HASH=$(tr -d ' \r\n' < "$SMOKE_HASH_FILE"); export IWD2R_SMOKE_HASH
elif [ -z "${IWD2R_SMOKE_HASH:-}" ]; then
    echo "gates: the smoke frame hash is not pinned; run build/linux-gcc/tests/iwd2r_tests \"start screen smoke\" and save the framebuffer-hash it prints in $SMOKE_HASH_FILE"
fi
# Lavapipe (Mesa's software Vulkan) runs the gpu presenter's tests: on a
# machine that has it, a skipped [present] case means they did not run.
LAVAPIPE=0
if ls /usr/lib/*/libvulkan_lvp.so /usr/lib/libvulkan_lvp.so /usr/lib64/libvulkan_lvp.so >/dev/null 2>&1 \
    || { command -v vulkaninfo >/dev/null 2>&1 && vulkaninfo --summary 2>/dev/null | grep -qi llvmpipe; }; then
    LAVAPIPE=1
fi
failed=()
step() {
    local name="$1"; shift
    echo "=== $name"
    if "$@"; then echo "--- $name: ok"; else echo "--- $name: FAILED"; failed+=("$name"); fi
}
linux_preset() {
    local preset="$1" log rc skipped
    [ -f "build/$preset/CMakeCache.txt" ] || cmake --preset "$preset" || return 1
    cmake --build --preset "$preset" || return 1
    log=$(mktemp)
    ctest --preset "$preset" --output-on-failure | tee "$log"
    rc=${PIPESTATUS[0]}
    if [ "$LAVAPIPE" = 1 ]; then
        # The [present] cases ctest reports as skipped (each test is its own
        # ctest entry, named as the TEST_CASE).
        skipped=$("build/$preset/tests/iwd2r_tests" "[present]" --list-tests --verbosity quiet 2>/dev/null \
            | while IFS= read -r name; do
                [ -n "$name" ] && grep -F -- " $name " "$log" | grep -q 'Skipped' && echo "$name"
            done)
        if [ -n "$skipped" ]; then
            echo "gates: lavapipe is installed but these gpu presenter tests were skipped (no Vulkan device in the test):"
            echo "$skipped" | sed 's/^/  /'
            rc=1
        fi
    fi
    rm -f "$log"
    return "$rc"
}
step "linux-gcc build and tests" linux_preset linux-gcc
[ "$CLANG" = 1 ] && step "linux-clang build and tests" linux_preset linux-clang
step "tool tests" python3 -I tests/test_tools.py
step "subsystem map" python3 -I tools/gen_map.py --check
step "replay fields" python3 tools/gen_fields.py --check
[ -x tools/shaders.py ] && step "shader blobs" python3 tools/shaders.py --check
step "decompiler evidence (base $BASE)" python3 -I tools/check_evidence.py --base "$BASE"
format_check() {
    local files
    files=$(git diff --name-only --diff-filter=d 4fa36bc -- '*.cpp' '*.h' | grep -v '^src/' || true)
    [ -z "$files" ] || clang-format --dry-run --Werror $files
}
step "format" format_check
if [ "$WINDOWS" = 1 ]; then
    # The x86 preset with the oracle runner, injector and recorder; the x64
    # engine through play.py's build helper. Windows ctest runs are covered
    # by the sandbox suite (play.py --vm windows --suite) for now.
    step "windows x86 build" python3 tools/oracle.py build
    step "windows x64 build" python3 -c "import sys; sys.path.insert(0, 'tools'); import play; play.sync_windows(); play.build_windows()"
fi
echo
if [ ${#failed[@]} -eq 0 ]; then echo "gates: all passed"; exit 0; fi
echo "gates: FAILED: ${failed[*]}"; exit 1
