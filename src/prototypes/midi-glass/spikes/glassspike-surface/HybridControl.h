// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#pragma once

#include "HybridControl.g.h"

namespace winrt::glassspike::implementation
{
    // Approach C. A XAML element with no template: no children, no content, nothing to lay out.
    // It is here for the four things XAML is good at - a place in the tree, hit testing, focus
    // and an automation peer - and its pixels come from a composition visual the renderer
    // attaches to it.
    struct HybridControl : HybridControlT<HybridControl>
    {
        HybridControl() = default;

        double SurfaceValue() const noexcept { return m_value; }
        void SurfaceValue(double value) noexcept { m_value = value; }

        hstring SurfaceName() const { return m_name; }
        void SurfaceName(hstring const& value) { m_name = value; }

        Microsoft::UI::Xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();

        // Called from the renderer through get_self. Keeping the value here is what lets the
        // automation peer answer without the renderer being involved.
        void SetValueDirect(float value) noexcept { m_value = value; }

    private:
        double m_value{ 0.0 };
        hstring m_name;
    };
}

namespace winrt::glassspike::factory_implementation
{
    struct HybridControl : HybridControlT<HybridControl, implementation::HybridControl>
    {
    };
}
