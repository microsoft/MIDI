// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "WrapPanel.g.h"

namespace winrt::midisequencer::implementation
{
    // The comps' flex-wrap rows (seq.css gap: 6px), which WinUI has no panel for.
    struct WrapPanel : WrapPanelT<WrapPanel>
    {
        WrapPanel() = default;

        foundation::Size MeasureOverride(_In_ foundation::Size const& availableSize);
        foundation::Size ArrangeOverride(_In_ foundation::Size const& finalSize);
    };
}

namespace winrt::midisequencer::factory_implementation
{
    struct WrapPanel : WrapPanelT<WrapPanel, implementation::WrapPanel>
    {
    };
}
