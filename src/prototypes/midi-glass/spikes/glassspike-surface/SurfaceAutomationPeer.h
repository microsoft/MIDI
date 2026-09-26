// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include "SurfaceAutomationPeer.g.h"

namespace winrt::glassspike::implementation
{
    // One peer for both the templated control and the hybrid element, to show what a custom
    // control owes a screen reader: a control type, a name, and a pattern that carries the value.
    // This is the work the pure composition approach would have to do from nothing.
    struct SurfaceAutomationPeer : SurfaceAutomationPeerT<SurfaceAutomationPeer>
    {
        SurfaceAutomationPeer(
            Microsoft::UI::Xaml::FrameworkElement const& owner,
            glassspike::ISurfaceValueHost const& host);

        hstring GetClassNameCore() const;
        hstring GetNameCore() const;
        Microsoft::UI::Xaml::Automation::Peers::AutomationControlType GetAutomationControlTypeCore() const;
        Windows::Foundation::IInspectable GetPatternCore(
            Microsoft::UI::Xaml::Automation::Peers::PatternInterface pattern);

        // IRangeValueProvider
        bool IsReadOnly() const noexcept { return false; }
        double Value() const;
        double Minimum() const noexcept { return 0.0; }
        double Maximum() const noexcept { return 1.0; }
        double SmallChange() const noexcept { return 1.0 / 127.0; }
        double LargeChange() const noexcept { return 1.0 / 16.0; }
        void SetValue(double value);

    private:
        weak_ref<glassspike::ISurfaceValueHost> m_host;
    };
}

namespace winrt::glassspike::factory_implementation
{
    struct SurfaceAutomationPeer : SurfaceAutomationPeerT<SurfaceAutomationPeer, implementation::SurfaceAutomationPeer>
    {
    };
}
