// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================
// MIDI Glass phase 0 spike. Nothing here ships.

#include "pch.h"
#include "App.xaml.h"
#include "MainWindow.xaml.h"

using namespace winrt;
using namespace winrt::Microsoft::UI::Xaml;

namespace winrt::glassspike::implementation
{
    gspike::SpikeOptions App::s_options{};

    App::App()
    {
        UnhandledException({ this, &App::OnUnhandledException });
    }

    void App::OnUnhandledException(
        Windows::Foundation::IInspectable const& sender,
        UnhandledExceptionEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(sender);
        UNREFERENCED_PARAMETER(args);

        // A spike should fall over loudly rather than hide a measurement error behind a caught
        // exception, so nothing is marked handled here.
    }

    void App::OnLaunched(LaunchActivatedEventArgs const& args)
    {
        UNREFERENCED_PARAMETER(args);

        s_options = gspike::SpikeOptions::ParseProcessCommandLine();

        m_window = make<MainWindow>();
        m_window.Activate();
    }
}
