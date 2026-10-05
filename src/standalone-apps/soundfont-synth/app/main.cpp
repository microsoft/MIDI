// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"
#include "SingleInstance.h"
#include "SynthHost.h"

// The XAML compiler emits its own wWinMain; this one keeps startup and shutdown under our
// control. The apartment must stay STA: an MTA UI thread makes UI Automation fail. Every call
// into the MIDI service happens on the synth host's own threads, never this one.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // One copy only. A second would try to create the same endpoints, and with the window
    // hidden in the notification area, launching again is the natural way to ask for it back.
    if (!::midiapp::SingleInstance::AcquireOrActivateExisting(L"SoundFontSynth"))
    {
        return 0;
    }

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midisoundfontsynth::implementation::App>();
        });

    // The window is gone. Every synth is taken out of Windows and the audio device let go
    // before the process ends, so no app is left talking to an endpoint that has no sound.
    ::midisoundfontsynth::SynthHost::Current().Shutdown();

    ::midiapp::SingleInstance::Release();

    return 0;
}
