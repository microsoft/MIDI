// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"
#include "CommandLineOptions.h"
#include "DocumentHandoff.h"

// The XAML compiler emits its own wWinMain; we supply this one so startup stays under our
// control. The apartment must stay STA: an MTA UI thread makes UI Automation fail with
// E_UNEXPECTED and then faults, which would leave the app inaccessible to screen readers.
// The MIDI SDK's reporting calls block on the service, so they are never made from this
// thread - see MainWindow's snapshot helpers.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // One copy only. Two would run the same checks against the same machine at the same time.
    if (!::midiapp::SingleInstance::AcquireOrActivateExisting(L"Troubleshooter"))
    {
        // Another MIDI tool crashed while this one was already open. The open copy is told, so
        // it can show the crash rather than the customer seeing nothing happen.
        auto const options = ::miditroubleshooter::CommandLineOptions::ParseProcessCommandLine();

        if (options.HasCrashReport())
        {
            auto arguments = options.CrashArguments();
            arguments.insert(arguments.begin(), std::wstring{ ::miditroubleshooter::CrashReportHandoffMarker });

            ::midiapp::SendDocumentsToExistingInstance(L"Troubleshooter", arguments);
        }

        return 0;
    }

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::miditroubleshooter::implementation::App>();
        });

    ::midiapp::SingleInstance::Release();

    return 0;
}
