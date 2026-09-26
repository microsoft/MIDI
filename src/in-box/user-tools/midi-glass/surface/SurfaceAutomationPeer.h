// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "SurfaceAutomationPeer.g.h"

namespace winrt::midiglass::implementation
{
    // What a custom drawn control owes a screen reader: a control type, a name, and a pattern
    // that carries its value. The phase 0 spike proved this is the whole difference between a
    // surface a screen reader can drive and one where it finds nothing at all.
    struct SurfaceAutomationPeer : SurfaceAutomationPeerT<SurfaceAutomationPeer>
    {
        SurfaceAutomationPeer(_In_ midiglass::GlassControl const& owner);

        hstring GetClassNameCore() const;
        hstring GetNameCore() const;

        xaml::Automation::Peers::AutomationControlType GetAutomationControlTypeCore() const;

        foundation::IInspectable GetPatternCore(
            _In_ xaml::Automation::Peers::PatternInterface pattern);

        // IRangeValueProvider
        bool IsReadOnly() const noexcept;
        double Value() const;
        double Minimum() const noexcept { return 0.0; }
        double Maximum() const noexcept { return 1.0; }
        double SmallChange() const noexcept { return 1.0 / 127.0; }
        double LargeChange() const noexcept { return 1.0 / 16.0; }
        void SetValue(_In_ double value);

        // IToggleProvider
        xaml::Automation::ToggleState ToggleState() const;
        void Toggle();

        // IInvokeProvider
        void Invoke();

    private:
        midiglass::GlassControl Owner() const;

        weak_ref<midiglass::GlassControl> m_owner;
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct SurfaceAutomationPeer : SurfaceAutomationPeerT<SurfaceAutomationPeer, implementation::SurfaceAutomationPeer>
    {
    };
}
