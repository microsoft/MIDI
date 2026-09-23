// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "SurfaceAutomationPeer.h"
#include "SurfaceAutomationPeer.g.cpp"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;
using namespace winrt::Microsoft::UI::Xaml::Automation::Peers;

namespace winrt::glassspike::implementation
{
    SurfaceAutomationPeer::SurfaceAutomationPeer(
        FrameworkElement const& owner,
        glassspike::ISurfaceValueHost const& host)
        : SurfaceAutomationPeerT<SurfaceAutomationPeer>(owner)
        , m_host(host)
    {
    }

    hstring SurfaceAutomationPeer::GetClassNameCore() const
    {
        return L"GlassSurfaceControl";
    }

    hstring SurfaceAutomationPeer::GetNameCore() const
    {
        if (auto host = m_host.get())
        {
            auto name = host.SurfaceName();

            if (!name.empty())
            {
                return name;
            }
        }

        return L"Surface control";
    }

    AutomationControlType SurfaceAutomationPeer::GetAutomationControlTypeCore() const
    {
        return AutomationControlType::Slider;
    }

    Windows::Foundation::IInspectable SurfaceAutomationPeer::GetPatternCore(PatternInterface pattern)
    {
        if (pattern == PatternInterface::RangeValue)
        {
            return get_strong().as<Windows::Foundation::IInspectable>();
        }

        return nullptr;
    }

    double SurfaceAutomationPeer::Value() const
    {
        if (auto host = m_host.get())
        {
            return host.SurfaceValue();
        }

        return 0.0;
    }

    void SurfaceAutomationPeer::SetValue(double value)
    {
        if (auto host = m_host.get())
        {
            host.SurfaceValue(std::clamp(value, 0.0, 1.0));
        }
    }
}
