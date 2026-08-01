#!/usr/bin/env bash
# Installs the latest Apophysis 7X release for macOS or Linux - no compiler,
# Qt SDK, or vcpkg required, since it downloads the prebuilt package from
# GitHub Releases (built by .github/workflows/release.yml) rather than
# building from source. See README.md's "Building from source" section if
# you want to build it yourself instead.
#
# Usage:
#   curl -fsSL https://raw.githubusercontent.com/andrewlkirby/apophysis-cpp/main/install.sh | bash
set -euo pipefail

REPO="andrewlkirby/apophysis-cpp"
API_URL="https://api.github.com/repos/${REPO}/releases/latest"

log() { printf '==> %s\n' "$1"; }
die() { printf 'error: %s\n' "$1" >&2; exit 1; }

command -v curl >/dev/null || die "curl is required but not found. Install it via your package manager (e.g. 'sudo apt install curl' or 'brew install curl') and re-run this script."

OS="$(uname -s)"
ARCH="$(uname -m)"

WORK_DIR="$(mktemp -d)"
trap 'rm -rf "${WORK_DIR}"' EXIT

log "Looking up the latest release..."
RELEASE_JSON="${WORK_DIR}/release.json"
curl -fsSL "${API_URL}" -o "${RELEASE_JSON}" \
    || die "couldn't reach ${API_URL} - check your internet connection."

# Pulls "$1"'s value out of the release JSON's flat "name"/"browser_download_url"
# fields without a JSON parser dependency - good enough for GitHub's fixed
# release-asset schema.
asset_url_for() {
    local pattern="$1"
    grep -o '"browser_download_url": *"[^"]*"' "${RELEASE_JSON}" \
        | cut -d'"' -f4 \
        | grep -E "${pattern}" \
        | head -n1
}

case "${OS}" in
    Darwin)
        ASSET_PATTERN="apophysis7x-.*-macos-${ARCH}\\.zip$"
        ;;
    Linux)
        [[ "${ARCH}" == "x86_64" ]] || die "no Linux build available for architecture '${ARCH}' (only x86_64 is currently packaged)."
        ASSET_PATTERN="apophysis7x-.*-x86_64\\.AppImage$"
        ;;
    *)
        die "unsupported OS '${OS}'. Windows users should use install.ps1 instead - see README.md."
        ;;
esac

ASSET_URL="$(asset_url_for "${ASSET_PATTERN}")"
[[ -n "${ASSET_URL}" ]] || die "couldn't find a release asset matching '${ASSET_PATTERN}' - see https://github.com/${REPO}/releases for available downloads."
SUMS_URL="$(asset_url_for 'SHA256SUMS$')"

ASSET_NAME="$(basename "${ASSET_URL}")"
log "Downloading ${ASSET_NAME}..."
curl -fL --retry 3 -o "${WORK_DIR}/${ASSET_NAME}" "${ASSET_URL}"

if [[ -n "${SUMS_URL}" ]]; then
    log "Verifying checksum..."
    curl -fsSL "${SUMS_URL}" -o "${WORK_DIR}/SHA256SUMS"
    EXPECTED="$(grep " ${ASSET_NAME}\$" "${WORK_DIR}/SHA256SUMS" | cut -d' ' -f1)"
    if [[ -n "${EXPECTED}" ]]; then
        if command -v sha256sum >/dev/null; then
            ACTUAL="$(sha256sum "${WORK_DIR}/${ASSET_NAME}" | cut -d' ' -f1)"
        else
            ACTUAL="$(shasum -a 256 "${WORK_DIR}/${ASSET_NAME}" | cut -d' ' -f1)"
        fi
        [[ "${EXPECTED}" == "${ACTUAL}" ]] || die "checksum mismatch for ${ASSET_NAME} - download may be corrupted or tampered with. Expected ${EXPECTED}, got ${ACTUAL}."
    fi
fi

if [[ "${OS}" == "Darwin" ]]; then
    log "Unzipping..."
    unzip -q "${WORK_DIR}/${ASSET_NAME}" -d "${WORK_DIR}/extracted"
    APP_BUNDLE="$(find "${WORK_DIR}/extracted" -maxdepth 1 -name '*.app')"
    [[ -n "${APP_BUNDLE}" ]] || die "no .app bundle found inside ${ASSET_NAME}."

    DEST_DIR="/Applications"
    [[ -w "${DEST_DIR}" ]] || DEST_DIR="${HOME}/Applications"
    mkdir -p "${DEST_DIR}"
    DEST="${DEST_DIR}/$(basename "${APP_BUNDLE}")"
    rm -rf "${DEST}"
    cp -R "${APP_BUNDLE}" "${DEST}"

    # Clears any quarantine flag curl's download might have picked up -
    # without this, unsigned/unnotarized apps like this one can get an
    # "Apple could not verify" Gatekeeper block on first launch.
    xattr -dr com.apple.quarantine "${DEST}" 2>/dev/null || true

    log "Installed to ${DEST}"
    echo
    echo "If macOS refuses to open it the first time (\"cannot be opened"
    echo "because the developer cannot be verified\"), this build isn't"
    echo "code-signed/notarized - right-click the app in Finder and choose"
    echo "Open, or allow it via System Settings > Privacy & Security."

elif [[ "${OS}" == "Linux" ]]; then
    INSTALL_DIR="${HOME}/.local/opt/apophysis7x"
    BIN_DIR="${HOME}/.local/bin"
    mkdir -p "${INSTALL_DIR}" "${BIN_DIR}"

    APPIMAGE_DEST="${INSTALL_DIR}/Apophysis7X.AppImage"
    cp "${WORK_DIR}/${ASSET_NAME}" "${APPIMAGE_DEST}"
    chmod +x "${APPIMAGE_DEST}"

    # --appimage-extract-and-run sidesteps the AppImage's default
    # FUSE-mount launch path, so users without libfuse2 installed (common
    # on Ubuntu 22.04+, which dropped it by default) don't hit a "dlopen():
    # error loading libfuse.so.2" failure on first run.
    WRAPPER="${BIN_DIR}/apophysis7x"
    cat > "${WRAPPER}" <<EOF
#!/usr/bin/env bash
exec "${APPIMAGE_DEST}" --appimage-extract-and-run "\$@"
EOF
    chmod +x "${WRAPPER}"

    # Best-effort desktop entry so the app shows up in application
    # launchers - not fatal if the icon extraction step fails.
    APPS_DIR="${HOME}/.local/share/applications"
    ICONS_DIR="${HOME}/.local/share/icons/hicolor/256x256/apps"
    mkdir -p "${APPS_DIR}" "${ICONS_DIR}"
    if (cd "${WORK_DIR}" && APPIMAGE_EXTRACT_AND_RUN=1 "${APPIMAGE_DEST}" --appimage-extract usr/share/icons/hicolor/256x256/apps/apophysis7x.png >/dev/null 2>&1); then
        cp "${WORK_DIR}/squashfs-root/usr/share/icons/hicolor/256x256/apps/apophysis7x.png" "${ICONS_DIR}/apophysis7x.png" 2>/dev/null || true
    fi
    cat > "${APPS_DIR}/apophysis7x.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Apophysis 7X
Comment=Fractal flame editor and renderer
Exec=${WRAPPER}
Icon=apophysis7x
Categories=Graphics;2DGraphics;RasterGraphics;
Terminal=false
EOF

    log "Installed to ${APPIMAGE_DEST}"
    log "Launch command: apophysis7x"
    if [[ ":${PATH}:" != *":${BIN_DIR}:"* ]]; then
        echo
        echo "${BIN_DIR} isn't on your PATH yet - add this to your shell profile:"
        echo "  export PATH=\"${BIN_DIR}:\$PATH\""
    fi
fi
