// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services and should be used
// in your Windows application via an official binary distribution.
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midi2console
{
    // One entry per selectable thing. The picker draws the columns in this order and pads each
    // one, so any list (endpoints, function blocks, Bluetooth devices) can reuse it.
    struct PickerEntry
    {
        std::string Icon;
        std::string PrimaryText;
        std::string SecondaryText;
        std::string TertiaryText;
        std::string QuaternaryText;
        std::string Value;
        bool IsCancelEntry{ false };
    };

    struct PickerResult
    {
        bool Canceled{ true };
        std::string Value;
        std::string DisplayText;
        int SelectedIndex{ -1 };
    };

    // Generic FTXUI list picker. Escape cancels, which is the thing the Spectre.Console prompt
    // could not do. Entries are padded to a common width before being handed in.
    PickerResult ShowPicker(_In_ std::string_view prompt, _In_ std::vector<PickerEntry> entries);

    bool CanShowInteractiveUI();

    // Adds the trailing "(Cancel)" row and pads every column to a shared width.
    void FinalizePickerEntries(_Inout_ std::vector<PickerEntry>& entries);
}
