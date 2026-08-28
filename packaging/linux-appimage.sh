#!/usr/bin/env bash
# Bundles a Release build of apo_gui into a self-contained AppImage - the
# Linux equivalent of cmake/Packaging.cmake's Windows/macOS `deploy`
# targets, but implemented as a plain script rather than a CMake target
# because its tool (linuxdeploy) isn't part of the Qt SDK the way
# windeployqt/macdeployqt are - it has to be fetched over the network,
# which isn't something a normal offline `cmake --build` should require.
#
# Usage: packaging/linux-appimage.sh <build-dir> <version> [output-dir]
# Produces <output-dir>/apophysis7x-<version>-x86_64.AppImage.
#
# Requires: an internet connection (to fetch linuxdeploy) and either
# ImageMagick's `convert` (to derive an icon from src/ui/resources/app.ico)
# or a pre-existing packaging/apophysis7x.png.

set -euo pipefail

BUILD_DIR="${1:?usage: linux-appimage.sh <build-dir> <version> [output-dir]}"
APO_VERSION="${2:?usage: linux-appimage.sh <build-dir> <version> [output-dir]}"
OUTPUT_DIR="${3:-$PWD}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
APO_GUI_BIN="${BUILD_DIR}/src/ui/apo_gui"
if [[ ! -x "${APO_GUI_BIN}" ]]; then
    echo "error: ${APO_GUI_BIN} not found or not executable - build apo_gui first" >&2
    exit 1
fi

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

# --- AppDir staging ------------------------------------------------------

APPDIR="${WORK_DIR}/AppDir"
mkdir -p "${APPDIR}/usr/bin" "${APPDIR}/usr/share/applications" \
    "${APPDIR}/usr/share/icons/hicolor/256x256/apps"

cp "${APO_GUI_BIN}" "${APPDIR}/usr/bin/apo_gui"
cp "${REPO_ROOT}/packaging/apophysis7x.desktop" \
    "${APPDIR}/usr/share/applications/apophysis7x.desktop"

ICON_PNG="${REPO_ROOT}/packaging/apophysis7x.png"
if [[ ! -f "${ICON_PNG}" ]]; then
    command -v convert >/dev/null || {
        echo "error: no packaging/apophysis7x.png and ImageMagick's convert is unavailable to generate one from app.ico" >&2
        exit 1
    }
    ICON_PNG="${WORK_DIR}/apophysis7x.png"
    convert "${REPO_ROOT}/src/ui/resources/app.ico[0]" -resize 256x256 "${ICON_PNG}"
fi
cp "${ICON_PNG}" "${APPDIR}/usr/share/icons/hicolor/256x256/apps/apophysis7x.png"

# --- linuxdeploy + Qt plugin ---------------------------------------------

TOOLS_DIR="${WORK_DIR}/tools"
mkdir -p "${TOOLS_DIR}"

fetch_tool() {
    local name="$1" url="$2"
    curl -fL --retry 3 -o "${TOOLS_DIR}/${name}" "${url}"
    chmod +x "${TOOLS_DIR}/${name}"
}
fetch_tool linuxdeploy-x86_64.AppImage \
    "https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage"
fetch_tool linuxdeploy-plugin-qt-x86_64.AppImage \
    "https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage"

# Avoids depending on FUSE being installed on the machine running this
# script (linuxdeploy's own AppImage would otherwise need it to mount
# itself) - the AppImage runtime honors this env var by extracting and
# running itself directly instead.
export APPIMAGE_EXTRACT_AND_RUN=1
export PATH="${TOOLS_DIR}:${PATH}"
export VERSION="${APO_VERSION}"

cd "${WORK_DIR}"
linuxdeploy-x86_64.AppImage \
    --appdir "${APPDIR}" \
    --desktop-file "${APPDIR}/usr/share/applications/apophysis7x.desktop" \
    --icon-file "${APPDIR}/usr/share/icons/hicolor/256x256/apps/apophysis7x.png" \
    --plugin qt \
    --output appimage

mkdir -p "${OUTPUT_DIR}"
built="$(find "${WORK_DIR}" -maxdepth 1 -name '*.AppImage' ! -name 'linuxdeploy*')"
mv "${built}" "${OUTPUT_DIR}/apophysis7x-${APO_VERSION}-x86_64.AppImage"
echo "Wrote ${OUTPUT_DIR}/apophysis7x-${APO_VERSION}-x86_64.AppImage"
