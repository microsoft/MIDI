// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Header only, so an app that takes it only has to include it.

#include <algorithm>

namespace midiapp
{
    // The room a dialog's content has in this window: the window, less the dialog's own title,
    // padding and buttons, and a margin all round so it still reads as a dialog. A dialog whose
    // content fits this never scrolls as a whole.
    inline winrt::Windows::Foundation::Size DialogContentSpace(
        _In_ winrt::Microsoft::UI::Xaml::XamlRoot const& root) noexcept
    {
        // Measured on the default dialog template: 24 of padding each side, then the title above
        // and the button row below the content.
        constexpr double ChromeWidth = 48.0;
        constexpr double ChromeHeight = 176.0;
        constexpr double Margin = 32.0;

        constexpr double SmallestWidth = 320.0;
        constexpr double SmallestHeight = 200.0;

        try
        {
            if (root != nullptr)
            {
                auto const size = root.Size();

                return winrt::Windows::Foundation::Size{
                    static_cast<float>((std::max)(SmallestWidth, size.Width - ChromeWidth - 2 * Margin)),
                    static_cast<float>((std::max)(SmallestHeight, size.Height - ChromeHeight - 2 * Margin)) };
            }
        }
        catch (...)
        {
        }

        return winrt::Windows::Foundation::Size{ 480.0f, 400.0f };
    }

    // The default template stops a dialog at 548 by 756 whatever its content asks for. This lifts
    // that, so the content's size decides. Only takes effect before the dialog first shows.
    inline void LetDialogGrow(_In_ winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& dialog) noexcept
    {
        try
        {
            auto const dictionary = dialog.Resources();

            dictionary.Insert(winrt::box_value(L"ContentDialogMaxWidth"), winrt::box_value(10000.0));
            dictionary.Insert(winrt::box_value(L"ContentDialogMaxHeight"), winrt::box_value(10000.0));
        }
        catch (...)
        {
        }
    }
}
