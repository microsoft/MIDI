// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// ============================================================================
// Is this a preview build of the MIDI user tools?
//
// This is the single switch for the whole app family. It is 1 in the GitHub repository, which is
// where the previews are published, and 0 in the copy that ships in Windows. Set it to 0 and the
// PREVIEW badge in every tool's title bar disappears; nothing else changes.
//
// Deliberately a compile time define and NOT a servicing gate. A servicing gate exists so that a
// behavior change can be turned off on a customer's PC after it has shipped; a badge that says
// which build this is has nothing to decide at run time and no behavior to revert.
// ============================================================================

#define MIDI_TOOLS_PREVIEW_BUILD 1

namespace midiapp
{
    inline constexpr bool IsPreviewBuild() noexcept
    {
        return MIDI_TOOLS_PREVIEW_BUILD != 0;
    }

    // Shows or hides the title bar PREVIEW badge. Every tool calls this with its own
    // PreviewChiclet element while it is setting up its title bar.
    inline void ApplyPreviewBadgeVisibility(
        _In_ winrt::Microsoft::UI::Xaml::UIElement const& badge) noexcept
    {
        if (badge == nullptr)
        {
            return;
        }

        badge.Visibility(
            IsPreviewBuild() ?
            winrt::Microsoft::UI::Xaml::Visibility::Visible :
            winrt::Microsoft::UI::Xaml::Visibility::Collapsed);
    }
}
