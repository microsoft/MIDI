// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"

// The XAML compiler emits its own wWinMain; we supply this one so startup stays under our
// control. The apartment must stay STA: an MTA UI thread makes UI Automation fail with
// E_UNEXPECTED and then faults, which would leave the app inaccessible to screen readers.
// The MIDI SDK's session and connection calls block on the service, so they are never made
// from this thread - see MainWindow::StartMonitoringAsync.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midibluetoothsetup::implementation::App>();
        });

    return 0;
}
