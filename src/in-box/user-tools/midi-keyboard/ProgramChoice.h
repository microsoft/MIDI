// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "ProgramChoice.g.h"

namespace winrt::midikeyboard::implementation
{
    struct ProgramChoice : ProgramChoiceT<ProgramChoice>
    {
        ProgramChoice() = default;

        ProgramChoice(winrt::hstring const& displayName, winrt::hstring const& tags) :
            m_displayName(displayName),
            m_tags(tags)
        {
        }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        winrt::hstring Tags() const noexcept { return m_tags; }

        winrt::hstring AutomationName() const noexcept
        {
            return m_tags.empty()
                ? m_displayName
                : winrt::hstring{ std::wstring{ m_displayName } + L", " + std::wstring{ m_tags } };
        }

        winrt::Microsoft::UI::Xaml::Visibility TagsVisibility() const noexcept
        {
            return m_tags.empty()
                ? winrt::Microsoft::UI::Xaml::Visibility::Collapsed
                : winrt::Microsoft::UI::Xaml::Visibility::Visible;
        }

    private:
        winrt::hstring m_displayName{};
        winrt::hstring m_tags{};
    };
}

namespace winrt::midikeyboard::factory_implementation
{
    struct ProgramChoice : ProgramChoiceT<ProgramChoice, implementation::ProgramChoice>
    {
    };
}
