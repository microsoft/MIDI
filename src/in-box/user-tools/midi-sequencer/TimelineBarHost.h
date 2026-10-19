// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "TimelineBarHost.g.h"
#include "TimelineBarAutomationPeer.g.h"

#include <winrt/Microsoft.UI.Xaml.Automation.Provider.h>

#include <functional>

namespace winrt::midisequencer::implementation
{
    // The scroll bar's value for UI Automation, in bars: where the view starts, and how far it can.
    struct TimelineBarRange
    {
        double Value{ 1 };
        double Minimum{ 1 };
        double Maximum{ 1 };
        double SmallChange{ 1 };
        double LargeChange{ 1 };
    };

    struct TimelineBarHost : TimelineBarHostT<TimelineBarHost>
    {
        TimelineBarHost() = default;

        winrt::Microsoft::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();

        // Set by the bar that draws here.
        std::function<TimelineBarRange()> Range{};
        std::function<void(double)> SetValue{};
    };

    struct TimelineBarAutomationPeer : TimelineBarAutomationPeerT<TimelineBarAutomationPeer>
    {
        explicit TimelineBarAutomationPeer(midisequencer::TimelineBarHost const& owner);

        winrt::hstring GetClassNameCore() const;
        winrt::Microsoft::UI::Xaml::Automation::Peers::AutomationControlType GetAutomationControlTypeCore() const;
        winrt::Windows::Foundation::IInspectable GetPatternCore(winrt::Microsoft::UI::Xaml::Automation::Peers::PatternInterface const& pattern);
        winrt::Microsoft::UI::Xaml::Automation::Peers::AutomationOrientation GetOrientationCore() const;

        bool IsReadOnly() const;
        double Value() const;
        double Minimum() const;
        double Maximum() const;
        double SmallChange() const;
        double LargeChange() const;
        void SetValue(double value);

    private:
        TimelineBarRange Range() const;

        winrt::weak_ref<midisequencer::TimelineBarHost> m_owner{};
    };
}

namespace winrt::midisequencer::factory_implementation
{
    struct TimelineBarHost : TimelineBarHostT<TimelineBarHost, implementation::TimelineBarHost>
    {
    };

    struct TimelineBarAutomationPeer : TimelineBarAutomationPeerT<TimelineBarAutomationPeer, implementation::TimelineBarAutomationPeer>
    {
    };
}
