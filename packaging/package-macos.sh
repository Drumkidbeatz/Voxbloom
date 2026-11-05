#!/usr/bin/env bash
# Builds VoxBloom-<version>-macOS.pkg (installer) and a .zip from the Release artefacts.
#
# Unsigned by default. When these environment variables are set, the plugins are
# signed with a Developer ID, the installer is signed and the result is notarised:
#   MACOS_CERTIFICATE_P12        base64 of a .p12 holding both Developer ID certificates
#   MACOS_CERTIFICATE_PASSWORD   password of that .p12
#   MACOS_DEV_ID_APP             "Developer ID Application: Name (TEAMID)"
#   MACOS_DEV_ID_INSTALLER       "Developer ID Installer: Name (TEAMID)"
#   APPLE_ID, APPLE_APP_PASSWORD, APPLE_TEAM_ID   for notarytool
set -euo pipefail

VERSION="$1"
ART="${2:-build/VoxBloom_artefacts/Release}"
OUT="${3:-dist}"
WORK="$(mktemp -d)"
mkdir -p "$OUT"

stage() { mkdir -p "$WORK/$1"; cp -R "$2" "$WORK/$1/"; }
stage vst3 "$ART/VST3/VoxBloom.vst3"
stage au   "$ART/AU/VoxBloom.component"
stage app  "$ART/Standalone/VoxBloom.app"

SIGNED=0
if [[ -n "${MACOS_CERTIFICATE_P12:-}" ]]; then
    SIGNED=1
    KEYCHAIN="$WORK/build.keychain"
    KEYCHAIN_PW="$(uuidgen)"
    echo "$MACOS_CERTIFICATE_P12" | base64 --decode > "$WORK/cert.p12"
    security create-keychain -p "$KEYCHAIN_PW" "$KEYCHAIN"
    security set-keychain-settings -lut 21600 "$KEYCHAIN"
    security unlock-keychain -p "$KEYCHAIN_PW" "$KEYCHAIN"
    security import "$WORK/cert.p12" -k "$KEYCHAIN" -P "$MACOS_CERTIFICATE_PASSWORD" \
        -T /usr/bin/codesign -T /usr/bin/productsign
    security set-key-partition-list -S apple-tool:,apple: -s -k "$KEYCHAIN_PW" "$KEYCHAIN"
    security list-keychains -d user -s "$KEYCHAIN" $(security list-keychains -d user | tr -d '"')
fi

for bundle in "$WORK/vst3/VoxBloom.vst3" "$WORK/au/VoxBloom.component" "$WORK/app/VoxBloom.app"; do
    if [[ $SIGNED == 1 ]]; then
        codesign --force --deep --options runtime --timestamp -s "$MACOS_DEV_ID_APP" "$bundle"
    else
        # Ad-hoc signature: required for Apple Silicon to load the binary at all.
        codesign --force --deep -s - "$bundle"
    fi
done

# The AU cache only notices a new component after this.
mkdir -p "$WORK/scripts-au"
cat > "$WORK/scripts-au/postinstall" <<'POST'
#!/bin/sh
killall -9 AudioComponentRegistrar 2>/dev/null || true
exit 0
POST
chmod +x "$WORK/scripts-au/postinstall"

ID=com.drumkidbeatz.voxbloom
pkgbuild --root "$WORK/vst3" --identifier "$ID.vst3" --version "$VERSION" \
         --install-location "/Library/Audio/Plug-Ins/VST3" "$WORK/VoxBloom-VST3.pkg"
pkgbuild --root "$WORK/au" --identifier "$ID.au" --version "$VERSION" --scripts "$WORK/scripts-au" \
         --install-location "/Library/Audio/Plug-Ins/Components" "$WORK/VoxBloom-AU.pkg"
pkgbuild --root "$WORK/app" --identifier "$ID.app" --version "$VERSION" \
         --install-location "/Applications" "$WORK/VoxBloom-App.pkg"

cat > "$WORK/distribution.xml" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>VoxBloom $VERSION</title>
    <options customize="always" require-scripts="false" hostArchitectures="arm64,x86_64"/>
    <choices-outline>
        <line choice="au"/>
        <line choice="vst3"/>
        <line choice="app"/>
    </choices-outline>
    <choice id="au" title="Audio Unit (Logic Pro, GarageBand, FL Studio)">
        <pkg-ref id="$ID.au"/>
    </choice>
    <choice id="vst3" title="VST3 (FL Studio, Ableton Live, Cubase, Studio One)">
        <pkg-ref id="$ID.vst3"/>
    </choice>
    <choice id="app" title="Standalone App">
        <pkg-ref id="$ID.app"/>
    </choice>
    <pkg-ref id="$ID.au">VoxBloom-AU.pkg</pkg-ref>
    <pkg-ref id="$ID.vst3">VoxBloom-VST3.pkg</pkg-ref>
    <pkg-ref id="$ID.app">VoxBloom-App.pkg</pkg-ref>
</installer-gui-script>
XML

PKG="$OUT/VoxBloom-$VERSION-macOS.pkg"
productbuild --distribution "$WORK/distribution.xml" --package-path "$WORK" "$WORK/unsigned.pkg"

if [[ $SIGNED == 1 ]]; then
    productsign --sign "$MACOS_DEV_ID_INSTALLER" --timestamp "$WORK/unsigned.pkg" "$PKG"
    xcrun notarytool submit "$PKG" --apple-id "$APPLE_ID" --password "$APPLE_APP_PASSWORD" \
        --team-id "$APPLE_TEAM_ID" --wait
    xcrun stapler staple "$PKG"
else
    mv "$WORK/unsigned.pkg" "$PKG"
fi

# Plain zip as well; ditto keeps permissions and signatures intact.
mkdir -p "$WORK/zip/VoxBloom-$VERSION-macOS"
cp -R "$WORK/vst3/VoxBloom.vst3" "$WORK/au/VoxBloom.component" "$WORK/app/VoxBloom.app" "$WORK/zip/VoxBloom-$VERSION-macOS/"
ditto -c -k --keepParent "$WORK/zip/VoxBloom-$VERSION-macOS" "$OUT/VoxBloom-$VERSION-macOS.zip"

echo "Signed: $SIGNED"
ls -l "$OUT"
