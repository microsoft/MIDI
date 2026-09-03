// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "App.xaml.g.h"
#include "CommandLineOptions.h"

namespace winrt::midikeyboard::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(xaml::LaunchActivatedEventArgs const& args);

        static ::midikeyboard::CommandLineOptions const& StartupOptions() noexcept { return s_startupOptions; }

    private:
        void OnUnhandledException(
            foundation::IInspectable const& sender,
            xaml::UnhandledExceptionEventArgs const& args);

        static ::midikeyboard::CommandLineOptions s_startupOptions;

        xaml::Window m_window{ nullptr };
    };
}
