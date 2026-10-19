// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// Small pieces the sequencer's dialogs and flyouts are built from in code, so they look and
// read alike. Include after MainWindow.xaml.h.

#include "StringResources.h"

namespace winrt::midisequencer::implementation::dialogparts
{
    // Names in a list, such as the tracks that play to a device.
    inline std::wstring JoinNames(_In_ std::vector<std::wstring> const& names)
    {
        std::wstring const separator{ ::midisequencer::resources::GetString(L"ListSeparator") };
        std::wstring joined{};

        for (auto const& name : names)
        {
            if (!joined.empty())
            {
                joined += separator;
            }

            joined += name;
        }

        return joined;
    }

    inline controls::TextBlock Heading(_In_ winrt::hstring const& text)
    {
        controls::TextBlock heading{};
        heading.Text(text);
        heading.FontWeight(winrt::Microsoft::UI::Text::FontWeights::SemiBold());
        heading.Margin(xaml::Thickness{ 0, 12, 0, 2 });
        automation::AutomationProperties::SetHeadingLevel(heading, automation::Peers::AutomationHeadingLevel::Level2);
        return heading;
    }

    inline controls::TextBlock Note(_In_ winrt::hstring const& text, _In_ media::Brush const& brush)
    {
        controls::TextBlock note{};
        note.Text(text);
        note.TextWrapping(xaml::TextWrapping::Wrap);
        note.FontSize(12);
        note.Foreground(brush);
        return note;
    }

    inline void PlaceIn(_In_ controls::Grid const& grid, _In_ xaml::FrameworkElement const& element, _In_ int32_t column)
    {
        controls::Grid::SetColumn(element, column);
        grid.Children().Append(element);
    }

    inline controls::CheckBox TableCheck(_In_ bool value)
    {
        controls::CheckBox check{};
        check.IsChecked(value);
        check.MinWidth(0);
        check.Padding(xaml::Thickness{ 0, 0, 0, 0 });
        check.HorizontalAlignment(xaml::HorizontalAlignment::Center);
        check.VerticalAlignment(xaml::VerticalAlignment::Center);
        return check;
    }

    inline bool IsChecked(_In_ primitives::ToggleButton const& check)
    {
        return check.IsChecked() != nullptr && check.IsChecked().GetBoolean();
    }

    // The endpoint a list's choice means: the one saved, until the person picks another, so a
    // device that isn't plugged in keeps its name.
    inline ::midisequencer::EndpointRef ChosenEndpoint(_In_ controls::ComboBox const& combo, _In_ ::midisequencer::EndpointRef const& original, _In_ bool changed)
    {
        if (!changed)
        {
            return original;
        }

        auto const choice = combo.SelectedItem().try_as<winrt::midisequencer::DeviceChoice>();

        if (choice == nullptr || choice.EndpointDeviceId().empty())
        {
            return {};
        }

        return ::midisequencer::EndpointRef{ std::wstring{ choice.DisplayName() }, std::wstring{ choice.EndpointDeviceId() } };
    }

    inline uint8_t ChosenGroup(_In_ controls::ComboBox const& combo, _In_ uint8_t fallback)
    {
        auto const choice = combo.SelectedItem().try_as<appshared::NamedChoice>();
        return choice != nullptr ? static_cast<uint8_t>(std::clamp(choice.Value(), 0, 15)) : fallback;
    }
}
