// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ProcessingBlock.h"
#include "RoundedShape.h"
#include "ThemeBrushes.h"

// The pieces the step dialogs are built from, so every dialog looks the same. None of them is
// noexcept: a failure goes to the try block of whatever is building the dialog.
namespace midipatchbay::parts
{
    // Over a group of settings.
    inline controls::TextBlock Heading(_In_ winrt::hstring const& text)
    {
        controls::TextBlock block{};

        block.Text(text);
        block.FontSize(13);
        block.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());

        return block;
    }

    // Small print that explains a setting.
    inline controls::TextBlock Hint(_In_ winrt::hstring const& text)
    {
        controls::TextBlock block{};

        block.Text(text);
        block.FontSize(11);
        block.TextWrapping(xaml::TextWrapping::Wrap);
        block.Foreground(ThemeBrushes::Current().Get(L"TextFillColorTertiaryBrush"));

        return block;
    }

    // Small print right under a heading, pulled up close to it.
    inline controls::TextBlock HeadingHint(_In_ winrt::hstring const& text)
    {
        auto block = Hint(text);

        block.Margin(xaml::ThicknessHelper::FromLengths(0, -4, 0, 4));

        return block;
    }

    // A group of settings on a card of its own.
    inline controls::Grid Card(_In_ xaml::UIElement const& content)
    {
        return MakeRoundedPanel(
            6,
            ThemeBrushes::Current().Get(L"CardBackgroundFillColorSecondaryBrush"),
            ThemeBrushes::Current().Get(L"CardStrokeColorDefaultBrush"),
            xaml::ThicknessHelper::FromLengths(12, 10, 12, 12),
            content).Panel;
    }

    inline controls::CheckBox Check(_In_ winrt::hstring const& text, _In_ bool value)
    {
        controls::CheckBox check{};

        check.Content(winrt::box_value(text));
        check.IsChecked(value);

        return check;
    }

    // Steps by one, or by largeChange with Page Up and Page Down. No header when it is empty.
    inline controls::NumberBox NumberBox(
        _In_ winrt::hstring const& header,
        _In_ double minimum,
        _In_ double maximum,
        _In_ double value,
        _In_ double largeChange)
    {
        controls::NumberBox box{};

        if (!header.empty())
        {
            box.Header(winrt::box_value(header));
        }

        box.Minimum(minimum);
        box.Maximum(maximum);
        box.SmallChange(1);
        box.LargeChange(largeChange);

        // Inline, not Compact: the compact spin buttons live in a popup that the dialog's scroll
        // viewer does not clip, so they hang over everything and never go away.
        box.SpinButtonPlacementMode(controls::NumberBoxSpinButtonPlacementMode::Inline);
        box.ValidationMode(controls::NumberBoxValidationMode::InvalidInputOverwritten);
        box.Value(value);

        return box;
    }

    // Two places, the same as a summary shows.
    inline double RoundedTempo(_In_ double value) noexcept
    {
        return std::round(std::clamp(value, MinimumGeneratorBeatsPerMinute, MaximumGeneratorBeatsPerMinute) * 100.0) / 100.0;
    }
}
