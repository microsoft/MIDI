// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "HybridControl.h"
#include "HybridControl.g.cpp"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::glassspike::implementation
{
    Automation::Peers::AutomationPeer HybridControl::OnCreateAutomationPeer()
    {
        auto self = get_strong();

        return glassspike::SurfaceAutomationPeer(
            self.as<FrameworkElement>(),
            self.as<glassspike::ISurfaceValueHost>());
    }
}
