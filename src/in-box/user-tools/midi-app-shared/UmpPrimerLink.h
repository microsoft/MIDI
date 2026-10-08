// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

namespace midiapp
{
    inline constexpr wchar_t UmpPrimerUri[]{ L"https://aka.ms/MidiUMPPrimer" };

    // Opens the primer in the default browser. The text comes from the calling app's resources.
    inline winrt::Microsoft::UI::Xaml::Controls::HyperlinkButton MakeUmpPrimerLink(
        _In_ winrt::hstring const& text)
    {
        winrt::Microsoft::UI::Xaml::Controls::HyperlinkButton link{};

        link.Content(winrt::box_value(text));
        link.NavigateUri(winrt::Windows::Foundation::Uri{ UmpPrimerUri });
        link.Padding(winrt::Microsoft::UI::Xaml::ThicknessHelper::FromUniformLength(0));

        return link;
    }
}
