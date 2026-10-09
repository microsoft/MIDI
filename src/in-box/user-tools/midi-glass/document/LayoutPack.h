// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ContentPack.h"
#include "LayoutModel.h"
#include "ThemeModel.h"

namespace glass
{
    // A pack is how a layout or a theme is shared. Unlike a backup it lists every file with its
    // hash, and it can carry signatures that say who published it. See ContentPack.h.
    constexpr wchar_t LayoutPackExtension[] = L".midilayoutpack";
    constexpr wchar_t ThemePackExtension[] = L".midithemepack";

    // In a layout pack, the customer theme the layout uses and that theme's picture.
    constexpr wchar_t PackThemeFolder[] = L"theme";

    struct PackBuildResult
    {
        bool Succeeded{ false };
        std::vector<uint8_t> Bytes{};
        uint32_t FileCount{ 0 };

        // A resource key naming what went wrong, or empty.
        std::wstring FailureKey{};
    };

    // The layout, every picture and video it names, and the customer theme it uses with that
    // theme's picture. A built-in theme travels by name, as it does in the layout file.
    PackBuildResult BuildLayoutPack(_In_ std::wstring const& layoutFilePath) noexcept;

    // A customer theme file and its picture.
    PackBuildResult BuildThemePack(_In_ std::wstring const& themeFilePath) noexcept;

    // What a checked pack holds and what it would replace, for the import dialog. Writes nothing.
    struct PackSummary
    {
        midiapp::ContentPackKind Kind{ midiapp::ContentPackKind::Unknown };

        std::wstring Name{};
        std::wstring Description{};
        std::optional<midiapp::ContentProvenance> Provenance{};
        bool IsFromNewerVersion{ false };

        // The customer theme a layout pack brings along. Empty when it uses a built-in one.
        std::wstring ThemeName{};

        uint32_t FileCount{ 0 };
        uint64_t TotalBytes{ 0 };

        // The item on this PC with the same provenance id, so the pack is an update to it.
        std::wstring ExistingPath{};
        std::optional<midiapp::ContentProvenance> ExistingProvenance{};

        // A different theme with the same name. Two themes can't share a name, so the import
        // has to replace it or give the new one another name.
        std::wstring NameTakenByPath{};

        // The name is one of the themes that ship, so the new one needs another name.
        bool NameIsBuiltIn{ false };
    };

    bool SummarizePack(
        _In_ midiapp::OpenedContentPack const& pack,
        _Out_ PackSummary& summary) noexcept;

    struct PackInstallResult
    {
        bool Succeeded{ false };

        // The layout or theme file.
        std::wstring Path{};

        // Every file the install wrote or found already there with the same bytes.
        std::vector<std::wstring> Files{};

        std::wstring FailureKey{};
    };

    // Files go beside the layouts that are already there. A picture whose name is taken by a
    // different picture gets a new name, and the layout is pointed at it. A theme whose name is
    // taken by a different theme is carried inside the layout instead. `replacePath` is the
    // layout this pack updates, or empty to add a new one.
    PackInstallResult InstallLayoutPack(
        _In_ midiapp::OpenedContentPack const& pack,
        _In_ std::wstring const& targetFolder,
        _In_ std::wstring const& replacePath) noexcept;

    // Into the themes folder. `replacePath` is the theme file this pack updates, or empty.
    // `newName` gives the theme another name, when its own is taken and both are kept.
    PackInstallResult InstallThemePack(
        _In_ midiapp::OpenedContentPack const& pack,
        _In_ std::wstring const& replacePath,
        _In_ std::wstring const& newName) noexcept;

    // A theme name that no theme on this PC has yet: "Name 2", "Name 3" and so on.
    std::wstring UnusedThemeName(_In_ std::wstring const& name) noexcept;
}
