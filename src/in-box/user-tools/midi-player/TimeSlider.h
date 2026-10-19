// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "TimeSlider.g.h"

namespace winrt::midiplayer::implementation
{
    struct TimeSlider : TimeSliderT<TimeSlider>
    {
        TimeSlider() = default;

        winrt::hstring ValueText() const noexcept { return m_valueText; }

        // Only stored. The value moves whenever this changes, and a screen reader reads this text
        // when it hears that the value moved, so raising a second event of our own here would
        // have it read the same time twice.
        void ValueText(_In_ winrt::hstring const& value) noexcept { m_valueText = value; }

        xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();

    private:
        winrt::hstring m_valueText{};
    };
}

namespace winrt::midiplayer::factory_implementation
{
    struct TimeSlider : TimeSliderT<TimeSlider, implementation::TimeSlider>
    {
    };
}
