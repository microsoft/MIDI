// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "TimeSliderAutomationPeer.g.h"

namespace winrt::midiplayer::implementation
{
    struct TimeSliderAutomationPeer : TimeSliderAutomationPeerT<TimeSliderAutomationPeer>
    {
        TimeSliderAutomationPeer(_In_ midiplayer::TimeSlider const& owner);

        foundation::IInspectable GetPatternCore(_In_ xaml::Automation::Peers::PatternInterface patternInterface);

        // IValueProvider
        bool IsReadOnly() const noexcept;
        winrt::hstring Value() const;
        void SetValue(_In_ winrt::hstring const& value);

    private:
        winrt::weak_ref<midiplayer::TimeSlider> m_owner;
    };
}

namespace winrt::midiplayer::factory_implementation
{
    struct TimeSliderAutomationPeer : TimeSliderAutomationPeerT<TimeSliderAutomationPeer, implementation::TimeSliderAutomationPeer>
    {
    };
}
