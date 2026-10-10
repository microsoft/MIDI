// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"

// The XAML compiler emits its own wWinMain; this one keeps startup under our control. The
// apartment must stay STA: an MTA UI thread makes UI Automation fail, which would leave the app
// unusable with a screen reader. The MIDI SDK's session and connection calls block on the
// service, so they are never made from this thread (see BackgroundWork).
//
// Not single instance: one window is one sequence, and each copy has its own engine, so two
// sequences can be open, and even play, side by side.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midisequencer::implementation::App>();
        });

    return 0;
}
