// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "App.xaml.g.h"
#include "CommandLineOptions.h"

namespace winrt::midisoundbanksynth::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(_In_ xaml::LaunchActivatedEventArgs const& args);

        static ::midisoundbanksynth::CommandLineOptions const& StartupOptions() noexcept { return s_startupOptions; }

    private:
        void OnUnhandledException(
            _In_ foundation::IInspectable const& sender,
            _In_ xaml::UnhandledExceptionEventArgs const& args);

        static ::midisoundbanksynth::CommandLineOptions s_startupOptions;

        xaml::Window m_window{ nullptr };
    };
}
