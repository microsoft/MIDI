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
#include "SynthListProblemDialog.h"
#include "SynthStore.h"

// The XAML compiler emits its own wWinMain; this one keeps startup and shutdown under our
// control. The apartment must stay STA: an MTA UI thread makes UI Automation fail. Every call
// into the MIDI service happens on the synth host's own threads, never this one.
int __stdcall wWinMain(_In_ HINSTANCE, _In_opt_ HINSTANCE, _In_ PWSTR, _In_ int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // One copy only. A second would try to create the same endpoints, and with the window
    // hidden in the notification area, launching again is the natural way to ask for it back.
    if (!::midiapp::SingleInstance::AcquireOrActivateExisting(L"SoundModule"))
    {
        return 0;
    }

    // Read before there is a window, because the window saves the list when it closes. A list that
    // can't be used as it is stays exactly as it is: the app says why and doesn't start, rather
    // than save an empty list over the customer's synths. This happens before XAML starts because
    // a XAML app with no window doesn't end when it is told to.
    auto synthList = ::midisoundmodule::SynthStore::Load();

    if (synthList.Problem != ::midisoundmodule::SynthListProblem::None)
    {
        ::midisoundmodule::ShowSynthListProblem(synthList, ::midisoundmodule::SynthStore::FilePath());
        ::midiapp::SingleInstance::Release();

        return 0;
    }

    ::winrt::midisoundmodule::implementation::App::SetStartupSynths(std::move(synthList.Synths));

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midisoundmodule::implementation::App>();
        });

    // The window is gone. Every synth is taken out of Windows and the audio device let go
    // before the process ends, so no app is left talking to an endpoint that has no sound.
    ::midisoundmodule::SynthHost::Current().Shutdown();

    ::midiapp::SingleInstance::Release();

    return 0;
}
