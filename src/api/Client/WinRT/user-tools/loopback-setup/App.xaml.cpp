// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

#include "AppSettings.h"
#include "StringResources.h"

using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::midiloopbacksetup::implementation
{
    ::midiloopbacksetup::CommandLineOptions App::s_startupOptions{};

    App::App()
    {
        // XAML objects must not call InitializeComponent during construction; winrt::make does it
        UnhandledException({ this, &App::OnUnhandledException });
    }

    _Use_decl_annotations_
    void App::OnUnhandledException(
        foundation::IInspectable const& sender,
        xaml::UnhandledExceptionEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);

        try
        {
            TraceLoggingWrite(
                MidiLoopbackSetupTelemetryProvider::Provider(),
                MIDI_LOOPSETUP_TRACE_EVENT_ERROR,
                TraceLoggingString(__FUNCTION__, MIDI_LOOPSETUP_TRACE_LOCATION_FIELD),
                TraceLoggingLevel(WINEVENT_LEVEL_ERROR),
                TraceLoggingWideString(L"Unhandled XAML exception. Continuing.", MIDI_LOOPSETUP_TRACE_MESSAGE_FIELD),
                TraceLoggingHResult(static_cast<HRESULT>(args.Exception()), MIDI_LOOPSETUP_TRACE_HRESULT_FIELD),
                TraceLoggingWideString(args.Message().c_str(), MIDI_LOOPSETUP_TRACE_ERROR_FIELD));

            // the app stays usable rather than terminating in front of the customer
            args.Handled(true);
        }
        catch (...)
        {
        }
    }

    _Use_decl_annotations_
    void App::OnLaunched(xaml::LaunchActivatedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        try
        {
            s_startupOptions = ::midiloopbacksetup::CommandLineOptions::ParseProcessCommandLine();

            ::midiloopbacksetup::AppSettings::Current().Load();

            auto window = winrt::make_self<MainWindow>();

            // sized and positioned before the first paint, so it does not visibly jump
            window->RestoreWindowPlacement();

            m_window = window.as<xaml::Window>();
            m_window.Activate();
        }
        MIDI_LOOPSETUP_CATCH_AND_LOG(L"Unable to create the main window.")
    }
}
