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
// from this thread.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // One process, however many windows. Two copies would each open their own connection to the
    // same instrument and neither would know what the other had sent, so a running layout and a
    // Panic have to mean the same thing across all of them.
    if (!::midiapp::SingleInstance::AcquireOrActivateExisting(L"Glass"))
    {
        return 0;
    }

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midiglass::implementation::App>();
        });

    ::midiapp::SingleInstance::Release();

    return 0;
}
