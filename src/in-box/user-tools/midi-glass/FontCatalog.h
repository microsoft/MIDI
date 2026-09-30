// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiglass::fonts
{
    // What a label is drawn in when it names no font, and after the named one on a PC that does
    // not have it.
    constexpr wchar_t DefaultFamily[] = L"Segoe UI Variable Text";

    // The families every Windows PC has. A layout is carried between PCs, and a font that is
    // not on the other one changes how the label looks there.
    std::vector<std::wstring> const& InBoxFamilies() noexcept;

    // Every family installed on this PC, by the name a layout stores, sorted. Read again from
    // Windows when `refresh` is set, so a font installed while the app is open shows up.
    std::vector<std::wstring> InstalledFamilies(_In_ bool refresh);

    // Whether this PC has the family, matched the way Windows matches a name. An empty name is
    // the default and is always there. Anything that cannot be checked counts as there, so a
    // label is never marked missing because a lookup failed.
    bool IsInstalled(_In_ std::wstring const& family) noexcept;

    // What to hand XAML for a label: the family named, then the default for a PC without it.
    media::FontFamily FamilyFor(_In_ std::wstring const& family);
}
