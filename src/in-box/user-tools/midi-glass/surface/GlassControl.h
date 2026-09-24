// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "GlassControl.g.h"

namespace winrt::midiglass::implementation
{
    struct GlassControl : GlassControlT<GlassControl>
    {
        GlassControl() = default;

        double SurfaceValue() const noexcept { return m_value; }

        // Setting it from outside means assistive technology is driving, so this both moves the
        // control and sends. A screen reader that moved a fader and sent nothing would be worse
        // than one that could not reach it, because it would look like it worked.
        void SurfaceValue(_In_ double value);

        hstring SurfaceName() const { return m_name; }
        void SurfaceName(_In_ hstring const& value) { m_name = value; }

        midiglass::SurfaceControlRole SurfaceRole() const noexcept { return m_role; }
        void SurfaceRole(_In_ midiglass::SurfaceControlRole value) noexcept { m_role = value; }

        xaml::Automation::Peers::AutomationPeer OnCreateAutomationPeer();

        // ---- not projected: the renderer and the input router reach these through get_self ----

        // Where this control sits in the layout, counting pages in order then controls within a
        // page. The same index the binding engine uses, so nothing has to translate on the hot
        // path.
        uint32_t ControlIndex() const noexcept { return m_controlIndex; }
        void ControlIndex(_In_ uint32_t value) noexcept { m_controlIndex = value; }

        // Moves the control without sending. Used when a value arrives from a device.
        void SetValueDirect(_In_ double value) noexcept { m_value = std::clamp(value, 0.0, 1.0); }

        // Called when something other than the pointer changes the value. Set by the runtime
        // window; the control never calls up into the UI on its own.
        void SetValueRequestHandler(_In_ std::function<void(uint32_t, double)> handler)
        {
            m_valueRequested = std::move(handler);
        }

        // A momentary press and release, for a button reached through automation.
        void SetInvokeHandler(_In_ std::function<void(uint32_t)> handler)
        {
            m_invoked = std::move(handler);
        }

        void RaiseInvoke();

    private:
        double m_value{ 0.0 };
        hstring m_name{};
        midiglass::SurfaceControlRole m_role{ midiglass::SurfaceControlRole::Slider };
        uint32_t m_controlIndex{ 0 };

        std::function<void(uint32_t, double)> m_valueRequested{};
        std::function<void(uint32_t)> m_invoked{};
    };
}

namespace winrt::midiglass::factory_implementation
{
    struct GlassControl : GlassControlT<GlassControl, implementation::GlassControl>
    {
    };
}
