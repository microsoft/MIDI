// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "MidiServiceAvailability.h"

namespace midiapp
{
    namespace wux = ::winrt::Microsoft::UI::Xaml;
    namespace wuxc = ::winrt::Microsoft::UI::Xaml::Controls;
    namespace midi2 = ::winrt::Windows::Devices::Midi2;

    namespace
    {
        // A tool that checks again on a timer would otherwise explain the same thing every time.
        std::atomic<bool> g_explained{ false };
    }

    MidiServiceAvailability CheckMidiServiceAvailability() noexcept
    {
        try
        {
            if (midi2::MidiApi::GetCurrentlySelectedApiMode() == midi2::MidiApiMode::LegacyMode)
            {
                return MidiServiceAvailability::LegacyApiMode;
            }

            return midi2::MidiApi::EnsureServiceAvailable()
                ? MidiServiceAvailability::Available
                : MidiServiceAvailability::Unavailable;
        }
        catch (...)
        {
            return MidiServiceAvailability::Unavailable;
        }
    }

    _Use_decl_annotations_
    winrt::Windows::Foundation::IAsyncOperation<bool> EnsureMidiServiceOrExplainAsync(
        wux::XamlRoot root,
        ServiceUnavailableStrings strings)
    {
        auto availability{ MidiServiceAvailability::Unavailable };

        try
        {
            winrt::apartment_context ui{};

            co_await winrt::resume_background();

            availability = CheckMidiServiceAvailability();

            co_await ui;
        }
        catch (...)
        {
            co_return false;
        }

        if (availability == MidiServiceAvailability::Available)
        {
            co_return true;
        }

        if (root == nullptr || g_explained.exchange(true))
        {
            co_return false;
        }

        try
        {
            wuxc::TextBlock body{};
            body.Text(availability == MidiServiceAvailability::LegacyApiMode
                ? strings.LegacyApiModeMessage
                : strings.UnavailableMessage);
            body.TextWrapping(wux::TextWrapping::Wrap);
            body.IsTextSelectionEnabled(true);

            wuxc::ScrollViewer scroller{};
            scroller.Content(body);
            scroller.MaxHeight(420);
            scroller.VerticalScrollBarVisibility(wuxc::ScrollBarVisibility::Auto);

            wuxc::ContentDialog dialog{};
            dialog.XamlRoot(root);
            dialog.Title(winrt::box_value(strings.Title));
            dialog.Content(scroller);
            dialog.CloseButtonText(strings.CloseButton);
            dialog.DefaultButton(wuxc::ContentDialogButton::Close);

            co_await dialog.ShowAsync();
        }
        catch (...)
        {
            // Another dialog already open is the usual reason. The window still says what it
            // can, and the next check gets to try again.
            g_explained = false;
        }

        co_return false;
    }
}
