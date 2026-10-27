#!/usr/bin/env bash
# Local build for Recoil.dylib, mirrors .github/workflows/build.yml.
# Everything (theos, frida-gum devkit, downloads, output) lives under
# <project>/.build and <project>/out. Nothing is written to /tmp.
#
# Usage:
#   scripts/local-build.sh setup    # fetch frida-gum devkit + theos toolchain
#   scripts/local-build.sh build    # compile Recoil.dylib -> out/Recoil.dylib
#   scripts/local-build.sh all      # setup + build
#   scripts/local-build.sh clean    # remove .theos build dir (keeps toolchain)

set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
FRIDA_VERSION="${FRIDA_VERSION:-17.24.3}"
BUILD_DIR="$ROOT/.build"
DL_DIR="$BUILD_DIR/downloads"
GUM_DIR="$ROOT/vendor/frida-gum"
export THEOS="${THEOS:-$BUILD_DIR/theos}"

log() { printf '[local-build] %s\n' "$*"; }

need() {
    command -v "$1" >/dev/null 2>&1 || { log "missing tool: $1"; exit 1; }
}

setup_gum() {
    if [ -f "$GUM_DIR/frida-gum.h" ] || [ -f "$GUM_DIR/include/frida-gum.h" ]; then
        log "frida-gum devkit already present: $GUM_DIR"
        return
    fi
    mkdir -p "$DL_DIR" "$GUM_DIR"
    local archive="$DL_DIR/frida-gum-devkit-${FRIDA_VERSION}-ios-arm64.tar.xz"
    if [ ! -f "$archive" ]; then
        log "downloading frida-gum devkit ${FRIDA_VERSION}"
        curl -fsSL -o "$archive.part" \
            "https://github.com/frida/frida/releases/download/${FRIDA_VERSION}/frida-gum-devkit-${FRIDA_VERSION}-ios-arm64.tar.xz"
        mv "$archive.part" "$archive"
    fi
    tar -xJf "$archive" -C "$GUM_DIR"
    log "frida-gum devkit extracted to $GUM_DIR"
}

setup_theos() {
    if [ -f "$THEOS/makefiles/common.mk" ] && [ -n "$(ls -A "$THEOS/toolchain/linux/iphone" 2>/dev/null || true)" ]; then
        log "theos already installed: $THEOS"
        return
    fi
    mkdir -p "$DL_DIR"
    local installer="$DL_DIR/install-theos"
    log "downloading theos installer"
    curl -fsSL -o "$installer" https://raw.githubusercontent.com/theos/theos/master/bin/install-theos
    sed -i 's/\blibxml2\b/libxml2-16/' "$installer"
    log "installing theos into $THEOS"
    THEOS="$THEOS" bash "$installer" </dev/null
}

check_prereqs() {
    need make
    need curl
    need tar
    need xz
    need git
}

build() {
    if [ ! -f "$THEOS/makefiles/common.mk" ]; then
        log "theos not found, run: scripts/local-build.sh setup"
        exit 1
    fi
    cd "$ROOT"
    make -j"$(( $(nproc) + 1 ))" ARCHS=arm64 DEBUG=0 FINALPACKAGE=1
    mkdir -p out
    local dylib
    dylib="$(find .theos -name Recoil.dylib ! -path '*dSYM*' | head -1)"
    [ -n "$dylib" ] || { log "Recoil.dylib not found"; exit 1; }
    cp "$dylib" out/Recoil.dylib
    strip -x out/Recoil.dylib 2>/dev/null || true
    log "built: $ROOT/out/Recoil.dylib"
}

cmd="${1:-all}"
check_prereqs
case "$cmd" in
    setup) setup_gum; setup_theos ;;
    build) build ;;
    all)   setup_gum; setup_theos; build ;;
    clean) rm -rf "$ROOT/.theos" ; log "cleaned .theos" ;;
    *) echo "usage: $0 {setup|build|all|clean}"; exit 2 ;;
esac
