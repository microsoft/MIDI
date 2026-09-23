// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "App.xaml.h"

// The XAML compiler emits its own wWinMain; this one keeps startup under our control. The
// apartment stays STA because an MTA XAML thread breaks UI Automation, and half of what this
// spike is measuring is what a screen reader can see.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::glassspike::implementation::App>();
        });

    return 0;
}
