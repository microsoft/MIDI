// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "App.xaml.g.h"
#include "CommandLineOptions.h"

namespace winrt::miditroubleshooter::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(xaml::LaunchActivatedEventArgs const& args);

        static ::miditroubleshooter::CommandLineOptions const& StartupOptions() noexcept { return s_startupOptions; }

        // False when the customer declined the elevation prompt, which is what the read-only
        // banner on the main window keys off.
        static bool IsElevated() noexcept { return s_isElevated; }

    private:
        void OnUnhandledException(
            foundation::IInspectable const& sender,
            xaml::UnhandledExceptionEventArgs const& args);

        static ::miditroubleshooter::CommandLineOptions s_startupOptions;
        static bool s_isElevated;

        xaml::Window m_window{ nullptr };
    };
}
