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
// from this thread - see RouteEngine and BackgroundWork.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // One copy only. A second one would route the same saved patches a second time, and with
    // the window hidden in the notification area, launching again is the natural way to ask for
    // it back.
    if (!::midiapp::SingleInstance::AcquireOrActivateExisting(L"Patchbay"))
    {
        return 0;
    }

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midipatchbay::implementation::App>();
        });

    ::midiapp::SingleInstance::Release();

    return 0;
}
