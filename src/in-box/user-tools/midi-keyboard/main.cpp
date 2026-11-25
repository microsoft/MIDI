// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"
#include "AppSettings.h"
#include "CommandLineOptions.h"
#include "CrashHandlerRegistration.h"
#include "KeyboardInstance.h"

// The XAML compiler emits its own wWinMain; we supply this one so startup stays under our
// control. The apartment must stay STA: an MTA UI thread makes UI Automation fail with
// E_UNEXPECTED and then faults, which would leave the app inaccessible to screen readers.
// The MIDI SDK's session and connection calls block on the service, so they are never made
// from this thread - see MainWindow::ConnectAsync.
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
    // First, so a crash anywhere after this opens MIDI Troubleshooting and Repair.
    ::midiapp::RegisterCrashHandler();

    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // Read here because whether to open a window at all depends on it.
    ::midikeyboard::AppSettings::Current().Load();

    if (!::midikeyboard::KeyboardInstance::AcquireOrActivateExisting(
            ::midikeyboard::CommandLineOptions::ParseProcessCommandLine()))
    {
        return 0;
    }

    ::winrt::Microsoft::UI::Xaml::Application::Start([](auto&&)
        {
            ::winrt::make<::winrt::midikeyboard::implementation::App>();
        });

    ::midikeyboard::KeyboardInstance::Release();

    return 0;
}
