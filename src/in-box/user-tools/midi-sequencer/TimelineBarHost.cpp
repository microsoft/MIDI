// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TimelineBarHost.h"
#include "TimelineBarHost.g.cpp"
#include "TimelineBarAutomationPeer.g.cpp"

namespace winrt::midisequencer::implementation
{
    namespace peers = ::winrt::Microsoft::UI::Xaml::Automation::Peers;

    peers::AutomationPeer TimelineBarHost::OnCreateAutomationPeer()
    {
        return winrt::make<TimelineBarAutomationPeer>(*this);
    }

    _Use_decl_annotations_
    TimelineBarAutomationPeer::TimelineBarAutomationPeer(midisequencer::TimelineBarHost const& owner) :
        TimelineBarAutomationPeerT<TimelineBarAutomationPeer>(owner),
        m_owner{ owner }
    {
    }

    winrt::hstring TimelineBarAutomationPeer::GetClassNameCore() const
    {
        return L"TimelineScrollBar";
    }

    peers::AutomationControlType TimelineBarAutomationPeer::GetAutomationControlTypeCore() const
    {
        return peers::AutomationControlType::ScrollBar;
    }

    peers::AutomationOrientation TimelineBarAutomationPeer::GetOrientationCore() const
    {
        return peers::AutomationOrientation::Horizontal;
    }

    _Use_decl_annotations_
    winrt::Windows::Foundation::IInspectable TimelineBarAutomationPeer::GetPatternCore(peers::PatternInterface const& pattern)
    {
        if (pattern == peers::PatternInterface::RangeValue)
        {
            return *this;
        }

        return TimelineBarAutomationPeerT<TimelineBarAutomationPeer>::GetPatternCore(pattern);
    }

    TimelineBarRange TimelineBarAutomationPeer::Range() const
    {
        try
        {
            if (auto const owner = m_owner.get())
            {
                if (auto const& range = winrt::get_self<TimelineBarHost>(owner)->Range; range)
                {
                    return range();
                }
            }
        }
        catch (...)
        {
        }

        return {};
    }

    bool TimelineBarAutomationPeer::IsReadOnly() const { return false; }
    double TimelineBarAutomationPeer::Value() const { return Range().Value; }
    double TimelineBarAutomationPeer::Minimum() const { return Range().Minimum; }
    double TimelineBarAutomationPeer::Maximum() const { return Range().Maximum; }
    double TimelineBarAutomationPeer::SmallChange() const { return Range().SmallChange; }
    double TimelineBarAutomationPeer::LargeChange() const { return Range().LargeChange; }

    void TimelineBarAutomationPeer::SetValue(double value)
    {
        if (auto const owner = m_owner.get())
        {
            if (auto const& set = winrt::get_self<TimelineBarHost>(owner)->SetValue; set)
            {
                set(value);
            }
        }
    }
}
