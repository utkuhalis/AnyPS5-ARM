#!/bin/sh
# Builds AnyPS5.app, the converter that turns a game dump into a Mac app without the command line.
#
#     tools/macos/build_converter_app.sh <build dir> <Vulkan SDK macOS dir> <output dir>
#
# The build dir is a finished `ninja all libs` build (relinker and prx libraries); the Vulkan SDK
# provides the loader and MoltenVK that every converted title carries.
set -eu

if [ $# -ne 3 ]; then
    echo "usage: $0 <build dir> <Vulkan SDK macOS dir> <output dir>" >&2
    exit 2
fi
root="$(cd "$(dirname "$0")/../.." && pwd)"
build="$(cd "$1" && pwd)"
vulkan="$(cd "$2" && pwd)"
mkdir -p "$3"
output="$(cd "$3" && pwd)"
source="$root/tools/macos/converter"
app="$output/AnyPS5.app"
version="$(git -C "$root" describe --tags --always 2>/dev/null | sed "s/^v//" || echo 0.0.0)"

swift build --package-path "$source" -c release --arch arm64 --arch x86_64
binary="$(swift build --package-path "$source" -c release --arch arm64 --arch x86_64 --show-bin-path)/AnyPS5"

rm -rf "$app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources/toolkit/libs" "$app/Contents/Resources/toolkit/vulkan" "$app/Contents/Resources/licenses"
cp "$binary" "$app/Contents/MacOS/AnyPS5"
cp "$build/core/relinker/relinker" "$app/Contents/Resources/toolkit/relinker"
cp "$build"/core/libs/libs/*.prx "$app/Contents/Resources/toolkit/libs/"
cp -L "$vulkan/lib/libvulkan.1.dylib" "$vulkan/lib/libMoltenVK.dylib" "$app/Contents/Resources/toolkit/vulkan/"
sed 's#"library_path": "[^"]*"#"library_path": "../../../MacOS/libs/libMoltenVK.dylib"#' "$vulkan/share/vulkan/icd.d/MoltenVK_icd.json" > "$app/Contents/Resources/toolkit/vulkan/MoltenVK_icd.json"
cp "$root/LICENSE" "$app/Contents/Resources/licenses/AnyPS5-LICENSE.txt"
cp "$vulkan/../Licenses/LICENSE.txt" "$app/Contents/Resources/licenses/VulkanSDK-LICENSE.txt"

scratch="$(mktemp -d)"
trap 'rm -rf "$scratch"' EXIT
swift "$source/icon.swift" "$scratch/icon.png"
mkdir "$scratch/AppIcon.iconset"
for size in 16 32 128 256 512; do
    sips -z $size $size "$scratch/icon.png" --out "$scratch/AppIcon.iconset/icon_${size}x${size}.png" >/dev/null
    sips -z $((size * 2)) $((size * 2)) "$scratch/icon.png" --out "$scratch/AppIcon.iconset/icon_${size}x${size}@2x.png" >/dev/null
done
iconutil -c icns "$scratch/AppIcon.iconset" -o "$app/Contents/Resources/AppIcon.icns"

cat > "$app/Contents/Info.plist" <<EOF
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0">
<dict>
    <key>CFBundleName</key><string>AnyPS5</string>
    <key>CFBundleDisplayName</key><string>AnyPS5</string>
    <key>CFBundleIdentifier</key><string>org.anyps5.converter</string>
    <key>CFBundleExecutable</key><string>AnyPS5</string>
    <key>CFBundlePackageType</key><string>APPL</string>
    <key>CFBundleIconFile</key><string>AppIcon</string>
    <key>CFBundleShortVersionString</key><string>$version</string>
    <key>CFBundleVersion</key><string>$version</string>
    <key>LSMinimumSystemVersion</key><string>14.0</string>
    <key>LSApplicationCategoryType</key><string>public.app-category.utilities</string>
    <key>NSHighResolutionCapable</key><true/>
    <key>CFBundleDocumentTypes</key>
    <array>
        <dict>
            <key>CFBundleTypeName</key><string>PS5 game folder</string>
            <key>CFBundleTypeRole</key><string>Viewer</string>
            <key>LSHandlerRank</key><string>Alternate</string>
            <key>LSItemContentTypes</key><array><string>public.folder</string></array>
        </dict>
    </array>
</dict>
</plist>
EOF

codesign --force --deep --sign - "$app"
echo "$app"
