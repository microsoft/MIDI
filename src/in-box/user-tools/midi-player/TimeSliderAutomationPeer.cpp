// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TimeSliderAutomationPeer.h"
#include "TimeSliderAutomationPeer.g.cpp"

#include "TimeText.h"

using namespace winrt::Microsoft::UI::Xaml::Automation::Peers;

namespace winrt::midiplayer::implementation
{
    namespace
    {
        // UIA_E_ELEMENTNOTENABLED, which is what UI Automation expects from a disabled control.
        constexpr winrt::hresult ElementNotEnabled{ static_cast<int32_t>(0x80040200) };
    }

    _Use_decl_annotations_
    TimeSliderAutomationPeer::TimeSliderAutomationPeer(midiplayer::TimeSlider const& owner)
        : TimeSliderAutomationPeerT<TimeSliderAutomationPeer>(owner)
        , m_owner(owner)
    {
    }

    _Use_decl_annotations_
    foundation::IInspectable TimeSliderAutomationPeer::GetPatternCore(PatternInterface patternInterface)
    {
        if (patternInterface == PatternInterface::Value)
        {
            return get_strong().as<foundation::IInspectable>();
        }

        return base_type::GetPatternCore(patternInterface);
    }

    // Not read only, or a screen reader can call a slider you can move "read only". Setting the
    // value is the same as typing into Go to time.
    bool TimeSliderAutomationPeer::IsReadOnly() const noexcept
    {
        return false;
    }

    winrt::hstring TimeSliderAutomationPeer::Value() const
    {
        if (auto const owner = m_owner.get())
        {
            return owner.ValueText();
        }

        return {};
    }

    _Use_decl_annotations_
    void TimeSliderAutomationPeer::SetValue(winrt::hstring const& value)
    {
        auto const owner = m_owner.get();

        if (owner == nullptr)
        {
            return;
        }

        if (!owner.IsEnabled())
        {
            throw winrt::hresult_error(ElementNotEnabled);
        }

        auto const seconds = ::midiplayer::ParseTimeText(value);

        if (!seconds.has_value())
        {
            throw winrt::hresult_invalid_argument();
        }

        // Moving the slider is what seeks, so this goes the same way as the keyboard does.
        owner.Value(std::clamp(static_cast<double>(seconds.value()), owner.Minimum(), owner.Maximum()));
    }
}
