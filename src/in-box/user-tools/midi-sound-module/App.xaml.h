// Copyright (c) Microsoft Corporation and Contributors.
// Licensed under the MIT License
// ============================================================================
// This is part of Windows MIDI Services
// Further information: https://aka.ms/midi
// ============================================================================

#pragma once

#include "App.xaml.g.h"
#include "CommandLineOptions.h"
#include "SynthList.h"

namespace winrt::midisoundmodule::implementation
{
    struct App : AppT<App>
    {
        App();

        void OnLaunched(_In_ xaml::LaunchActivatedEventArgs const& args);

        static ::midisoundmodule::CommandLineOptions const& StartupOptions() noexcept { return s_startupOptions; }

        // The synth list wWinMain read before XAML started, handed to the window once.
        static void SetStartupSynths(_Inout_ std::vector<::midisoundmodule::SynthDefinition>&& synths) noexcept { s_startupSynths = std::move(synths); }
        static std::vector<::midisoundmodule::SynthDefinition> TakeStartupSynths() noexcept { return std::move(s_startupSynths); }

    private:
        void OnUnhandledException(
            _In_ foundation::IInspectable const& sender,
            _In_ xaml::UnhandledExceptionEventArgs const& args);

        static ::midisoundmodule::CommandLineOptions s_startupOptions;
        static std::vector<::midisoundmodule::SynthDefinition> s_startupSynths;

        xaml::Window m_window{ nullptr };
    };
}
