// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "TimeSlider.h"
#include "TimeSlider.g.cpp"

namespace winrt::midiplayer::implementation
{
    xaml::Automation::Peers::AutomationPeer TimeSlider::OnCreateAutomationPeer()
    {
        return midiplayer::TimeSliderAutomationPeer{ *this };
    }
}
