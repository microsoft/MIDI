// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "GlassControl.h"
#include "GlassControl.g.cpp"
#include "SurfaceAutomationPeer.h"

namespace winrt::midiglass::implementation
{
    _Use_decl_annotations_
    void GlassControl::SurfaceValue(double value)
    {
        auto const clamped = std::clamp(value, 0.0, 1.0);

        if (clamped == m_value)
        {
            return;
        }

        m_value = clamped;

        if (m_valueRequested)
        {
            m_valueRequested(m_controlIndex, m_value);
        }
    }

    void GlassControl::RaiseInvoke()
    {
        if (m_invoked)
        {
            m_invoked(m_controlIndex);
        }
    }

    xaml::Automation::Peers::AutomationPeer GlassControl::OnCreateAutomationPeer()
    {
        return midiglass::SurfaceAutomationPeer(*this);
    }
}
