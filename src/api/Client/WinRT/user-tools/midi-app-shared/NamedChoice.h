// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

// types outside the consuming app's root namespace get namespace qualified generated headers
#include "MidiAppShared.NamedChoice.g.h"

namespace winrt::MidiAppShared::implementation
{
    struct NamedChoice : NamedChoiceT<NamedChoice>
    {
        NamedChoice() = default;

        NamedChoice(winrt::hstring const& displayName, int32_t value) :
            m_displayName(displayName),
            m_value(value)
        {
        }

        winrt::hstring DisplayName() const noexcept { return m_displayName; }
        int32_t Value() const noexcept { return m_value; }

    private:
        winrt::hstring m_displayName{};
        int32_t m_value{ 0 };
    };
}

namespace winrt::MidiAppShared::factory_implementation
{
    struct NamedChoice : NamedChoiceT<NamedChoice, implementation::NamedChoice>
    {
    };
}
