# Overlay of the upstream libmidi2 port. Identical to the registry port except for the
# patches below, which mirror upstream fixes for AM_MIDI2.0Lib issues #39 and #36. Both are
# in upstream main but not in a released version, and a vcpkg publish takes over a month to
# reach the internal build repo, so expect this overlay to live for a while.
#
# This must stay under src/api: the internal Windows build repo is rooted there and cannot
# see anything above it.
#
# The internal build repo carries its own libmidi2 overlay, in a location dictated by that
# repo's own placement rules rather than mirroring this path. The two have not been compared,
# so confirm they are equivalent before assuming either can be removed or is redundant.
#
# Before deleting this, note two deliberate differences from upstream main, both of which
# take effect the moment the overlay is dropped:
#   - upstream initializes d1 to 0 rather than the 255 "no pending data byte" sentinel. That
#     works only because bsToUMP(0,0,x) happens to emit nothing.
#   - upstream also resets d0/d1 in clearAll(). Nothing here calls clearAll().

vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO midi2-dev/AM_MIDI2.0Lib
    REF "v${VERSION}"
    SHA512 867968c6a9a1c7ae31f685fc833df79860d6989e39ba1e76ee6848e2f9e49dc4af6b49a85000b643aa77afeb5b99dbc2e29f769dc7709f0e20ccdcfd7dc3acca
    HEAD_REF main
    PATCHES
        fix-uninitialized-running-status.patch
        fix-stray-sysex-end-byte.patch
)

if(VCPKG_TARGET_IS_WINDOWS)
    vcpkg_check_linkage(ONLY_STATIC_LIBRARY)
endif()

vcpkg_cmake_configure(SOURCE_PATH "${SOURCE_PATH}")
vcpkg_cmake_install()
vcpkg_cmake_config_fixup()

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/share")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE")
