// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"
#include "InstanceHandoff.h"

// The XAML compiler emits its own wWinMain; we supply this one so startup stays under our
// control. The apartment must stay STA: an MTA UI thread makes UI Automation fail with
// E_UNEXPECTED and then faults, which would leave the app inaccessible to screen readers.
// The MIDI SDK's session and connection calls block on the service, so they are never made
// from this thread - see PlaybackEngine and BackgroundWork.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    // One player per customer. A queue only means anything if there is one of them, and two
    // copies would both be sending to the same output.
    if (!midiapp::SingleInstance::AcquireOrActivateExisting(::midiplayer::InstanceKey))
    {
        // The running copy has been brought forward. Whatever this launch was asked to open is
        // handed to it rather than being lost.
        ::midiplayer::SendFilesToExistingInstance(
            ::midiplayer::CommandLineOptions::ParseProcessCommandLine().Files);

        return 0;
    }

    winrt::init_apartment(winrt::apartment_type::single_threaded);

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midiplayer::implementation::App>();
        });

    midiapp::SingleInstance::Release();

    return 0;
}
