// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Deliberately free of pch.h and XAML, like the rest of the document layer.

#include <sal.h>
#include <string>
#include <vector>

#include "LayoutModel.h"

namespace glass
{
    // What a new layout starts from. The first thing somebody needs is not a blank canvas, it is
    // something that already works on the device they just picked.
    enum class LayoutTemplateKind
    {
        Blank = 0,
        Mixer = 1,
        DjDeck = 2,
        DrumPads = 3,
        Transport = 4,
    };

    struct LayoutTemplateInfo
    {
        LayoutTemplateKind Kind{ LayoutTemplateKind::Blank };

        // Looked up in the resources for display. Not shown to a customer as it stands here.
        std::wstring NameResourceKey{};
        std::wstring DescriptionResourceKey{};

        // A Segoe Fluent Icons code point, so the picker has something to show per template.
        wchar_t Glyph{ 0 };
    };

    std::vector<LayoutTemplateInfo> const& LayoutTemplates() noexcept;

    LayoutDocument BuildLayoutFromTemplate(
        _In_ LayoutTemplateKind kind,
        _In_ std::wstring const& layoutName,
        _In_ std::wstring const& deviceName,
        _In_ midiapp::EndpointMatch const& match,
        _In_ midiapp::EndpointMatchMode matchMode) noexcept;

    // What the mixer template sends, written down so the end to end test knows what to look for.
    constexpr uint32_t StarterFaderCount = 8;
    constexpr uint32_t StarterKnobCount = 8;
    constexpr uint32_t StarterPadCount = 8;

    // Volume through to controller 14, the range every DAW maps first.
    constexpr uint32_t StarterFirstFaderController = 7;
    constexpr uint32_t StarterFirstKnobController = 16;

    // The bottom octave of the General MIDI drum map, which is where a pad grid usually lands.
    constexpr uint32_t StarterFirstPadNote = 36;

    // The mixer template, by its old name, because that is what the runtime tests drive.
    LayoutDocument BuildStarterLayout(
        _In_ std::wstring const& layoutName,
        _In_ std::wstring const& deviceName,
        _In_ midiapp::EndpointMatch const& match,
        _In_ midiapp::EndpointMatchMode matchMode) noexcept;
}
